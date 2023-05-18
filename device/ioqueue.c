#include "ioqueue.h"
#include "interrupt.h"
#include "global.h"
#include "debug.h"

#define NULL ((void*)0)

// however, the implement is meaningless...

void ioqueue_init(struct ioqueue* ioq) {
   lock_init(&ioq->lock);
   ioq->producer = ioq->consumer = NULL;
   ioq->head = ioq->tail = 0;
}

// get the next available pos
static int32_t next_pos(int32_t pos) {
   return (pos + 1) % bufsize;
}

// if the queue is full
int ioq_full(struct ioqueue* ioq) {
   ASSERT(intr_get_status() == INTR_OFF); // here is ugly...

   // TODO: I think that a user himself never need to disable intr

   // so... why we need intr off here?
   // proof: e.g.
   //    first you get: next_pos(ioq->head)
   //    then the schedule happends: 
   //    when back here: ioq->head has changes (may point to anywhere...

   // brief: we want the following statement be atom
   return next_pos(ioq->head) == ioq->tail;
}

// if the queue is empty
int ioq_empty(struct ioqueue* ioq) {
   ASSERT(intr_get_status() == INTR_OFF); // ugly again...
   return ioq->head == ioq->tail;
}

// make cur thread wait for buf ready
// (by set waiters list (list size is 1 though)
static void ioq_wait(struct task_struct** waiter) {
   ASSERT(*waiter == NULL && waiter != NULL);
   *waiter = running_thread();
   thread_block(TASK_BLOCKED);
}

// wake up the waiter for buf
static void ioq_wake(struct task_struct** waiter) {
   ASSERT(*waiter != NULL);
   thread_unblock(*waiter);
   *waiter = NULL;
}

// consume from tail of queue
char ioq_getchar(struct ioqueue* ioq) {
   // ugly... but this statement seems not necesary?
   ASSERT(intr_get_status() == INTR_OFF);

   // check if buf is ready
   while (ioq_empty(ioq)) {
      // if buf not ready, then try to wait for buf ready

      // buf if there has been another wating thread (consumer), wait for it first
      //    (proof: the thread must be consumer
      //     if the thread is producer -> then the buf should be full at that time
      //                               -> but now the buf is empty
      //                               -> must be consumed after producer sleep
      //                               -> the consumer will weak up producer
      //                               -> the producer cannot be waiting here
      lock_acquire(&ioq->lock);

      // when no other waiting, now it can set the wait flag
      // then switch to another ready thread
      ioq_wait(&ioq->consumer);

      // the buf is ready, then notify other waiting consumer
      lock_release(&ioq->lock);
   }

   // consume
   char byte = ioq->buf[ioq->tail];
   ioq->tail = next_pos(ioq->tail);

   //
   if (ioq->producer != NULL) {
      ioq_wake(&ioq->producer);
   }

   return byte;
}

// produer write one byte to ioq
void ioq_putchar(struct ioqueue* ioq, char byte) {
   ASSERT(intr_get_status() == INTR_OFF);

   //
   while (ioq_full(ioq)) {
      lock_acquire(&ioq->lock);
      ioq_wait(&ioq->producer);
      lock_release(&ioq->lock);
   }

   ioq->buf[ioq->head] = byte;
   ioq->head = next_pos(ioq->head);

   // weak up consumer on queue
   if (ioq->consumer != NULL) {
      ioq_wake(&ioq->consumer);
   }
}
