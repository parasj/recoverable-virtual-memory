#include "rvm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

static struct segment_list segments;

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

    rvm.directory = directory;
    // rvm.commit_log_file = 

    LIST_INIT(&segments);

    return rvm;
}

void *rvm_map(rvm_t rvm, const char *segname, int size_to_create) {
    segment_t seg;

    char seg_path[512];
    strcat(seg_path, rvm.directory);
    strcat(seg_path, "/");
    strcat(seg_path, segname);
    strcat(seg_path, ".seg");
    
    if (access(seg_path, F_OK) != -1) {
        fprintf(stdout, "Non-implemented\n");
        exit(1);
    } else {
        // init segment data structures
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
        LIST_INSERT_HEAD(&segments, seg_copy, next_seg);
    }

    return seg.seg_addr;
}

void rvm_unmap(rvm_t rvm, void *segbase) {

}

void rvm_destroy(rvm_t rvm, const char *segname) {

}

trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases) {
    trans_t trans;
    return trans;
}

void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size) {

}

void rvm_commit_trans(trans_t tid) {

}

void rvm_abort_trans(trans_t tid) {

}

void rvm_truncate_log(rvm_t rvm) {

}

void rvm_verbose(int enable_flag) {

}