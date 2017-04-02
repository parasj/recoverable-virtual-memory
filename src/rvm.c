#include "rvm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

struct segment_list *segments;
struct range_list *ranges;
const char *commit_log_file;

void rage_quit(const char* source, const char* error) {
  fprintf(stderr, "%s: %s", source, error);
  exit(1);
}

segment_t *segbase_to_segment(void *segbase) {
  segment_t *i;
  LIST_FOREACH(i, segments, next_seg)
    if(i->seg_addr == segbase)
      return i;
  return NULL;
}

segment_t *segname_to_segment(const char *segname) {
  segment_t *i;
  LIST_FOREACH(i, segments, next_seg)
    if(strcmp(i->seg_name, segname) == 0)
      return i;
  return NULL;
}

rvm_t rvm_init(const char *directory) {
  rvm_t rvm;

  // check if directory exists (or make if not)
  struct stat st;
  int r = stat(directory, &st);
  if (r == -1) {
    mkdir(directory, 0770);
  } else if (!S_ISDIR(st.st_mode)) {
    rage_quit(__FUNCTION__, "File exists at path, not directory\n");
  }

  char *logfile = malloc(strlen(directory));
  rvm.directory = directory;
  
  strcpy(logfile, directory);
  strcat(logfile, ".rvm_log");
  commit_log_file = logfile;
  LIST_INIT(segments);
  LIST_INIT(ranges);

  return rvm;
}

void *rvm_map(rvm_t rvm, const char *segname, int size_to_create) {
  segment_t seg;
  char seg_path[512];
  strcat(seg_path, rvm.directory);
  strcat(seg_path, "/.");
  strcat(seg_path, segname);
  strcat(seg_path, ".seg");

  // mustn't remap a segment that's already been mapped
  if(segname_to_segment(segname) != NULL)
    return NULL;

  if (access(seg_path, F_OK) == 0) {
    // crash recovery stuff happens here, since the backing
    // file already exists.

    // if the segment is already in our data structures, need
    // to make sure the most up-to-date version of the disk
    // segment is in memory

    // if bigger, extend the segment to fit the newly requested
    // size. If smaller, truncate the backinAlso holy shit I took it ing file.
    rage_quit(__FUNCTION__, "Not implemented");
  } else {
    // init segment data structures
    seg.trans_id = 0;
    seg.seg_size = size_to_create;
    seg.seg_addr = (char *) malloc(size_to_create);
    seg.seg_name = (char *) malloc(sizeof(segname));
    strcpy(seg.seg_name, segname);

    // touch the file
    int fd = open(seg_path, O_RDWR | O_CREAT, 0770);
    if (fd != -1) {
      ftruncate(fd, size_to_create);
      close(fd);
    }

    // insert into segment list
    segment_t *seg_copy = (segment_t *) malloc(sizeof(segment_t));
    *seg_copy = seg;
    LIST_INSERT_HEAD(segments, seg_copy, next_seg);
  }

  return seg.seg_addr;
}

void rvm_unmap(rvm_t rvm, void *segbase) {
  segment_t *i = segbase_to_segment(segbase);
  if(i) {
      LIST_REMOVE(i, next_seg);
      free(i->seg_addr);
      free(i);
      return;
    }
}

void rvm_destroy(rvm_t rvm, const char *segname) {
  // mustn't destroy a segment that's mapped
  segment_t *i;
  if((i = segname_to_segment(segname)))
    return;

  // not mapped, so we can delete the backing
  // file safely
  char seg_path[512];
  strcat(seg_path, rvm.directory);
  strcat(seg_path, "/.");
  strcat(seg_path, segname);
  strcat(seg_path, ".seg");
  remove(seg_path);
}

trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases) {
  static trans_t trans_num = 0;
  segment_t *seg;

  for(int i = 0; i < numsegs; i++) {
    seg = segbase_to_segment(segbases[i]);
    if(seg) {
      if(seg->trans_id != 0) {
        // segment already involved in a transaction
        return -1;
      }
    } else {
      // segment not found in the rvm list
      return -1;
    }
  }

  // all segments have been found in the list, none
  // of them are involved in transactions.
  trans_t trans_id = trans_num++;
  for(int i = 0; i < numsegs; i++) {
    seg = segbase_to_segment(segbases[i]);
    // we know that seg is not null, because we already
    // checked for that earlier
    seg->trans_id = trans_id;
  }

  // undo records
  for(int i = 0; i < numsegs; i++) {
    range_t *undo = (range_t*) malloc(sizeof(range_t));
    // write the header (offset of zero and size of seg_size means its an undo log)
    seg = segbase_to_segment(segbases[i]);
    undo->tid     = seg->trans_id;
    undo->segbase = seg->seg_addr;
    undo->offset  = 0;
    undo->size    = seg->seg_size;
    undo->data    = malloc(seg->seg_size);
    undo->is_undo = 1; // is an undo record
    // write the data range to save
    memcpy(undo->data, seg->seg_addr, seg->seg_size);
    LIST_INSERT_HEAD(ranges, undo, next_range);
  }

  return trans_id;
}

void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size) {
  // do the checks
  segment_t *seg = segbase_to_segment(segbase);
  if(!seg) return; // check that the segment exists
  // check that the transaction id matches the segment
  if(seg->trans_id != tid) return; 
  // check that the offset and size are within bounds
  if(offset+size >= seg->seg_size) return; 

  range_t *range = (range_t*) malloc(sizeof(range_t));
  range->tid     = tid;
  range->segbase = segbase;
  range->offset  = offset;
  range->size    = size;
  range->data    = malloc(size);
  range->is_undo = 1; // is an undo record
  memcpy(range->data, segbase + offset, size);
  LIST_INSERT_HEAD(ranges, range, next_range);
}

void rvm_commit_trans(trans_t tid) {
  // actually write the ranges for a tid into the log file
  range_t *i, *j;

  // skip the current tid as well as the forward pointer
  int next_trans=sizeof(int) + sizeof(tid);
  LIST_FOREACH(i, ranges, next_range)
    if(i->tid == tid && !i->is_undo)
      // skip the range header and the actual data
      next_trans += sizeof(range_t) + i->size;
  // check that ranges were found
  if(next_trans == sizeof(int) + sizeof(tid))
    rage_quit(__FUNCTION__, "Couldn't find ranges with that transaction id");
      
  FILE *logfile = fopen(commit_log_file, "a");
  fwrite(&tid, sizeof(tid), 1, logfile);
  fwrite(&next_trans, sizeof(next_trans), 1, logfile);

  i = LIST_FIRST(ranges);
  while (i != NULL) {
    j = LIST_NEXT(i, next_range);
    if(i->tid == tid) {
      LIST_REMOVE(i, next_range);
      if(!i->is_undo) {
        fwrite(i, sizeof(range_t), 1, logfile);
        fwrite(i->data, i->size, 1, logfile);
      }
      free(i->data);
      free(i);
    }
    i = j;
  }
}

void rvm_abort_trans(trans_t tid) {
  // actually write the ranges for a tid into the log file
  range_t *i, *j;

  // skip the current tid as well as the forward pointer
  int next_trans=sizeof(int) + sizeof(tid);
  LIST_FOREACH(i, ranges, next_range)
    if(i->tid == tid && !i->is_undo)
      // skip the range header and the actual data
      next_trans += sizeof(range_t) + i->size;
  // check that ranges were found
  if(next_trans == sizeof(int) + sizeof(tid)) return;

}

void rvm_truncate_log(rvm_t rvm) {

}

void rvm_verbose(int enable_flag) {

}
