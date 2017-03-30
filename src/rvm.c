#include "rvm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

segment_t * segbase_to_segment(rvm_t rvm, void *segbase) {
  segment_t *i;
  LIST_FOREACH(i, &rvm.segments, next_seg)
    if(i->seg_addr == segbase)
      return i;
  return NULL;
}

segment_t * segname_to_segment(rvm_t rvm, const char *segname) {
  segment_t *i;
  LIST_FOREACH(i, &rvm.segments, next_seg)
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
    fprintf(stdout, "File exists at path, not directory\n");
    exit(1);
  }

  char *logfile = malloc(strlen(directory));
  strcpy(logfile, directory);
  strcat(logfile, ".rvm_log");
  rvm.directory = directory;
  rvm.commit_log_file = logfile;

  LIST_INIT(&rvm.segments);

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
  segment_t *i;
  if(segname_to_segment(rvm, segname) != NULL)
    return NULL;

  if (access(seg_path, F_OK) == 0) {
    // crash recovery stuff happens here, since the backing
    // file already exists.

    // if the segment is already in our data structures, need
    // to make sure the most up-to-date version of the disk
    // segment is in memory

    // if bigger, extend the segment to fit the newly requested
    // size. If smaller, truncate the backing file.
    fprintf(stdout, "Not implemented\n");
    exit(1);
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
      close(fd);
    }

    // insert into segment list
    segment_t *seg_copy = (segment_t *) malloc(sizeof(segment_t));
    *seg_copy = seg;
    LIST_INSERT_HEAD(&rvm.segments, seg_copy, next_seg);
  }

  return seg.seg_addr;
}

void rvm_unmap(rvm_t rvm, void *segbase) {
  segment_t *i = segbase_to_segment(rvm, segbase);
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
  if((i = segname_to_segment(rvm, segname)))
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
    seg = segbase_to_segment(rvm, segbases[i]);
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
    seg = segbase_to_segment(rvm, segbases[i]);
    // we know that seg is not null, because we already
    // checked for that earlier
    seg->trans_id = trans_id;
  }

  return trans_id;
}

void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size) {
  // do the checks
  // save stuff in the logs
}

void rvm_commit_trans(trans_t tid) {
  // write stuff from the logs to the backing file
  // clean the logs (truncate)
}

void rvm_abort_trans(trans_t tid) {

}

void rvm_truncate_log(rvm_t rvm) {

}

void rvm_verbose(int enable_flag) {

}
