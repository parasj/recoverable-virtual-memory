#include "rvm.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
    // todo create log file if it doesn't exist

    return rvm;
}

void *rvm_map(rvm_t rvm, const char *segname, int size_to_create) {

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