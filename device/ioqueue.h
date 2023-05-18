#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H

#include "stdint.h"
#include "thread.h"
#include "sync.h"

#define bufsize 64

// circular buffer (thread-safe)
struct ioqueue {
  struct lock lock; // write lock (who can set the waitlist and buf?)

  struct task_struct* producer; // waiting producer
  struct task_struct* consumer; // waiting consumer

  char buf[bufsize]; // actual size: bufsize-1

  int32_t head;
  int32_t tail;
};

void ioqueue_init(struct ioqueue* ioq);

int ioq_empty(struct ioqueue* ioq);
int ioq_full(struct ioqueue* ioq);

char ioq_getchar(struct ioqueue* ioq);
void ioq_putchar(struct ioqueue* ioq, char byte);

#endif
