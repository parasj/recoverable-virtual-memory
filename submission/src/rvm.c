#include "rvm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

struct segment_list segments;
struct range_list ranges;
const char *commit_log_file;
int verbose;


void rage_quit(const char* source, const char* error) {
  fprintf(stderr, "%s: %s", source, error);
  exit(1);
}

segment_t *segbase_to_segment(void *segbase) {
  segment_t *i;
  LIST_FOREACH(i, &segments, next_seg)
    if(i->seg_addr == segbase)
      return i;
  return NULL;
}

segment_t *segname_to_segment(const char *segname) {
  segment_t *i;
  LIST_FOREACH(i, &segments, next_seg)
    if(strcmp(i->seg_name, segname) == 0)
      return i;
  return NULL;
}

char *segname_to_segpath(const char* dir, const char *segname) {
  char *seg_path = malloc(strlen(dir) + strlen(segname) + 10);
  strcpy(seg_path, dir);
  strcat(seg_path, "/.");
  strcat(seg_path, segname);
  strcat(seg_path, ".seg");
  return seg_path;
}

rvm_t rvm_init(const char *directory) {
  rvm_t rvm;

  verbose = 1;

  // check if directory exists (or make if not)
  struct stat st;
  int r = stat(directory, &st);
  if (r == -1) {
    mkdir(directory, 0770);
  } else if (!S_ISDIR(st.st_mode)) {
    rage_quit(__FUNCTION__, "File exists at path, not directory\n");
  }

  char *logfile = malloc(strlen(directory) + 16);
  rvm.directory = directory;
  
  strcpy(logfile, directory);
  strcat(logfile, "/.rvm.log");
  commit_log_file = logfile;
  int fd = open(commit_log_file, O_CREAT, 0770);
  close(fd);

  LIST_INIT(&segments);
  LIST_INIT(&ranges);

  return rvm;
}

void *rvm_map(rvm_t rvm, const char *segname, int size_to_create) {
  segment_t seg;
  char *seg_path = segname_to_segpath(rvm.directory, segname);

  // mustn't remap a segment that's already been mapped
  if(segname_to_segment(segname) != NULL)
    return NULL;

  // init segment data structures
  seg.trans_id = 0;
  seg.seg_size = size_to_create;
  seg.seg_addr = (char *) malloc(size_to_create);
  seg.seg_name = (char *) malloc(sizeof(segname));
  strcpy(seg.seg_name, segname);

  // insert into segment list
  segment_t *seg_copy = (segment_t *) malloc(sizeof(segment_t));
  *seg_copy = seg;
  LIST_INSERT_HEAD(&segments, seg_copy, next_seg);
    
  if (access(seg_path, F_OK) == 0) {
    rvm_truncate_log(rvm);
    if (verbose) fprintf(stderr, "{'function': 'map', 'found_backing_file': %d}, \n", 1);
    FILE *segfile = fopen(seg_path, "r");
    // if bigger, extend the segment to fit the newly requested
    // size, padding the extra space with nulls.
    // if smaller, truncate the backing file, losing extra data.
    truncate(seg_path, size_to_create);
    // crash recovery stuff happens here, since the backing
    // file already exists and is the right size.
    // we know that the logfile has been truncated
    // so the backing files should have the most up to date
    // versions of the recoverable memory segments. We can
    // just copy into memory from the backing file.
    fread(seg.seg_addr, seg.seg_size, 1, segfile);
    fclose(segfile);
  } else {
    if (verbose) fprintf(stderr, "{'function': 'map', 'found_backing_file': %d}, \n", 0);
    // create the backing file of appropriate size if
    // it doesn't exist
    int fd = open(seg_path, O_RDWR | O_CREAT, 0770);
    if (fd != -1) {
      ftruncate(fd, size_to_create);
      close(fd);
    }
  }

  free(seg_path);
  if (verbose) fprintf(stderr, "{'function': 'map', 'segname': %s}, \n", segname);
  return seg.seg_addr;
}

void rvm_unmap(rvm_t rvm, void *segbase) {
  segment_t *i = segbase_to_segment(segbase);
  if(i) {
      LIST_REMOVE(i, next_seg);
      free(i->seg_addr);
      free(i->seg_name);
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
  char *seg_path = segname_to_segpath(rvm.directory, segname);
  remove(seg_path);
  free(seg_path);
}

trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases) {
  static trans_t trans_num = 0;
  segment_t *seg;

  for(int i = 0; i < numsegs; i++) {
    seg = segbase_to_segment(segbases[i]);
    if(seg) {
      if(seg->trans_id != 0) {
        // segment already involved in a transaction
        rage_quit(__FUNCTION__, "segment already in a transaction");
        return -1;
      }
    } else {
      // segment not found in the rvm list
      rage_quit(__FUNCTION__, "segment not in rvm list");
      return -1;
    }
  }

  // all segments have been found in the list, none
  // of them are involved in transactions.
  trans_t trans_id = ++trans_num;
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
    undo->tid       = seg->trans_id;
    undo->segbase   = seg->seg_addr;
    undo->offset    = 0;
    undo->size      = seg->seg_size;
    undo->is_backed = 0;
    undo->data      = malloc(seg->seg_size);
    undo->is_undo   = 1; // is an undo record
    // write the data range to save
    memcpy(undo->data, seg->seg_addr, seg->seg_size);
    LIST_INSERT_HEAD(&ranges, undo, next_range);
  }

  if (verbose) fprintf(stderr, "{'function': 'begin_trans', 'id': %d}, \n", trans_id);
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
  range->is_undo = 0; // is not an undo record
  range->is_backed = 0;
  range->segname = seg->seg_name;
  range->namesize = strlen(range->segname);
  LIST_INSERT_HEAD(&ranges, range, next_range);
  if (verbose) fprintf(stderr, "{'function': 'about_to_modify', 'id': %d, 'segbase': %p, 'offset': %d, 'size': %d}, \n", tid, segbase, offset, size);
}

void rvm_commit_trans(trans_t tid) {
  // actually write the ranges for a tid into the log file
  range_t *i, *j;

  FILE *logfile = fopen(commit_log_file, "a");
  i = LIST_FIRST(&ranges);
  while (i != NULL) {
    j = LIST_NEXT(i, next_range);
    if(i->tid == tid) {
      LIST_REMOVE(i, next_range);
      if(!i->is_undo) {
        // not an undo record, should write to logfile
        if (verbose) fprintf(stderr, "{'function': 'commit_trans', 'range->size': %d}, \n", i->size);
        fwrite(i, sizeof(range_t), 1, logfile);
        fwrite(i->segname, i->namesize, 1, logfile);
        memcpy(i->data, i->segbase + i->offset, i->size);
        fwrite(i->data, i->size, 1, logfile);
      }
      // remove from the ranges list
      free(i->data);
      free(i);
    }
    i = j;
  }

  // reset the transaction id on the segments that belong
  // to this transaction
  segment_t *k,*l;
  k = LIST_FIRST(&segments);
  while (k != NULL) {
    l = LIST_NEXT(k, next_seg);
    if(k->trans_id == tid) {
      k->trans_id = 0;
    }
    k = l;
  }

  fclose(logfile);
  if (verbose) fprintf(stderr, "{'function': 'commit_trans', 'id': %d}, \n", tid);
}

void rvm_abort_trans(trans_t tid) {
  range_t *i, *j;
  i = LIST_FIRST(&ranges);
  while (i != NULL) {
    j = LIST_NEXT(i, next_range);
    if(i->tid == tid) {
      LIST_REMOVE(i, next_range);
      if(i->is_undo) {
        // undo record, so copy it back
        memcpy(i->segbase, i->data, i->size);
      }
      // free and remove regardless
      free(i->data);
      free(i);
    }
    i = j;
  }
}

void rvm_truncate_log(rvm_t rvm) {
  range_t range;
  char *seg_path, *new_log_file;

  new_log_file = (char*)malloc(strlen(commit_log_file) + 10);
  strcpy(new_log_file, commit_log_file);
  strcat(new_log_file, ".new");
  FILE *logfile = fopen(commit_log_file, "rb"), *segfile;
  FILE *newlog  = fopen(new_log_file, "w");
  while((fread(&range, 1, sizeof(range_t), logfile))) {
    // check if the segment is still mapped and if
    // the range has already been copied over;
    // no need to copy data over if it isn't
    range.data = malloc(range.size);
    range.segname = malloc(range.namesize);
    // get the range data from the logfile
    fread(range.segname, range.namesize, 1, logfile);
    range.segname[range.namesize] = 0;
    fread(range.data, range.size, 1, logfile);

    segment_t *seg;
    if(!range.is_backed) {
      if ((seg = segname_to_segment(range.segname))) {
        // open the segment backing file to copy the range data
        // into it
        seg_path = segname_to_segpath(rvm.directory, seg->seg_name);
        segfile = fopen(seg_path, "rb+");
        // go to the right offset
        fseek(segfile, range.offset, SEEK_SET);
        // write the range data in
        fwrite(range.data, range.size, 1, segfile);
        fclose(segfile);

        // update the is_backed on the log
        range.is_backed = 1;
        fseek(logfile, -sizeof(range_t) - range.size - range.namesize, SEEK_CUR);
        fwrite(&range, sizeof(range_t), 1, logfile);
        fseek(logfile, sizeof(range_t) + range.size + range.namesize, SEEK_CUR);

        if (verbose) fprintf(stderr, "{'function': 'truncate', 'id': %d, 'segbase': %p, 'offset': %d, 'size': %d}, \n",
                range.tid, range.segbase, range.offset, range.size);
      } else {
        // segment not mapped, but not backed, so must move
        fwrite(&range, sizeof(range_t), 1, newlog);
        fwrite(range.segname, range.namesize, 1, newlog);
        fwrite(range.data, range.size, 1, newlog);
        free(range.segname);
        free(range.data);
      }
    } // backed already, can ignore completely
  }
  fclose(logfile);
  fclose(newlog);

  rename(new_log_file, commit_log_file);
}

void rvm_verbose(int enable_flag) {
  verbose = enable_flag;
}
