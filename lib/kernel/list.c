#include "list.h"
#include "interrupt.h"

#define NULL ((void*)0)

void list_init (struct list* list) {
   list->head.prev = NULL;
   list->head.next = &list->tail;
   list->tail.prev = &list->head;
   list->tail.next = NULL;
}

// before->prev elem before
void list_insert_before(struct list_elem* before, struct list_elem* elem) { 

   enum intr_status old_status = intr_disable();

   // break two link and create four new link
   before->prev->next = elem; 
   elem->prev = before->prev;
   elem->next = before;
   before->prev = elem;

   intr_set_status(old_status);
}

// push elem to the head
void list_push(struct list* plist, struct list_elem* elem) {
   list_insert_before(plist->head.next, elem);
}

// push elem to the tail
void list_append(struct list* plist, struct list_elem* elem) {
   list_insert_before(&plist->tail, elem);
}

// remove given elem
void list_remove(struct list_elem* pelem) {
   enum intr_status old_status = intr_disable();

   // break two link (don't care about the link of elem)
   pelem->prev->next = pelem->next;
   pelem->next->prev = pelem->prev;

   intr_set_status(old_status);
}

// pop the first elem
struct list_elem* list_pop(struct list* plist) {
   struct list_elem* elem = plist->head.next;
   list_remove(elem);
   return elem;
} 

// determine if given obj_elem in list
int elem_find(struct list* plist, struct list_elem* obj_elem) {

   for (struct list_elem* elem = plist->head.next;
   elem != &plist->tail; 
   elem = elem->next) {
      if (elem == obj_elem) {
	 return 1;
      }
   }
   return 0;
}

// find if "suitable" elem exists
// by: foreach elem in list, call a given filter function on it
struct list_elem* list_traversal(struct list* plist, function func, int arg) {
   if (list_empty(plist)) { 
      return NULL;
   }

   for (struct list_elem* elem = plist->head.next;
   elem != &plist->tail; 
   elem = elem->next) {
      if (func(elem, arg)) { // return the first suitable elem
	 return elem;
      }
   }
   return NULL;
}

uint32_t list_len(struct list* plist) {
   uint32_t length = 0;
   for (struct list_elem* elem = plist->head.next;
   elem != &plist->tail; 
   elem = elem->next) {
      length++; 
   }
   return length;
}

int list_empty(struct list* plist) {
   return (plist->head.next == &plist->tail ? 1 : 0);
}
