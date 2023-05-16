#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H

#include "global.h"

#define offset(struct_type,member) (int)(&((struct_type*)0)->member)
#define elem2entry(struct_type, struct_member_name, elem_ptr) \
    (struct_type*)((int)elem_ptr - offset(struct_type, struct_member_name))

struct list_elem {
    struct list_elem* prev;
    struct list_elem* next;
};


struct list {
    // the first elem is head.next, the last elem is tail.prev
    struct list_elem head;
    struct list_elem tail;
};

// useful for `list_traversal`
// retval: if `elem` satisfy the condition `arg`
typedef int (function)(struct list_elem*, int arg);

void list_init (struct list*);
void list_insert_before(struct list_elem* before, struct list_elem* elem);
void list_push(struct list* plist, struct list_elem* elem);
void list_iterate(struct list* plist);
void list_append(struct list* plist, struct list_elem* elem);  
void list_remove(struct list_elem* pelem);
struct list_elem* list_pop(struct list* plist);
int list_empty(struct list* plist);
uint32_t list_len(struct list* plist);
struct list_elem* list_traversal(struct list* plist, function func, int arg);
int elem_find(struct list* plist, struct list_elem* obj_elem);

#endif
