#ifndef RVM_H
#define RVM_H

#include <sys/queue.h>

LIST_HEAD(segment_list, segment_t);
LIST_HEAD(range_list, range_t);

typedef int trans_t;
trans_t trans_id;

typedef struct segment_t {
	char* seg_name;
	int seg_size;
	void* seg_addr;
  trans_t trans_id;

	LIST_ENTRY(segment_t) next_seg;
} segment_t;


typedef struct range_t {
  trans_t tid;
  void* segbase;
  int offset;
  int size;
  void* data;
  int is_undo;
  LIST_ENTRY(range_t) next_range;
} range_t;

typedef struct rvm_t {
  const char *directory;
} rvm_t;

// need structures to represent logfile data
// transaction header (just the id?)
// range header (segbase, offset, size)
// data
// end mark

rvm_t rvm_init(const char *directory);
void *rvm_map(rvm_t rvm, const char *segname, int size_to_create);
void rvm_unmap(rvm_t rvm, void *segbase);
void rvm_destroy(rvm_t rvm, const char *segname);
trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases);
void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size);
void rvm_commit_trans(trans_t tid);
void rvm_abort_trans(trans_t tid);

void rvm_truncate_log(rvm_t rvm);
void rvm_verbose(int enable_flag);

#endif /* RVM_H */
