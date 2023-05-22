#include "string.h"
#include "global.h"
#include "debug.h"

void memset(void* dst_, uint8_t value, uint32_t size) {
  ASSERT(dst_ != NULL);
  uint8_t* dst = (uint8_t*)dst_;
  while (size--)
    *dst++ = value;
}

void memcpy(void* dst_, const void* src_, uint32_t size) {
  ASSERT(dst_ != NULL && src_ != NULL);
  uint8_t* dst = dst_;
  const uint8_t* src = src_;
  while (size--)
    *dst++ = *src++;
}

int memcmp(const void* a_, const void* b_, uint32_t size) {
  ASSERT(a_ != NULL || b_!= NULL);
  const char* a = a_;
  const char* b = b_;
  for (; size-- ; a++, b++) {
    if(*a != *b) {
      return *a > *b ? 1 : -1;
    }
  }
  return 0;
}

// end by null, so where is NULL defined...
char* strcpy(char* dst_, const char* src_) {
  ASSERT(dst_ != NULL && src_ != NULL); // may allow src = NULL?
  char* r = dst_;
  while ((*dst_++ = *src_++)) {}
  return r;
}

uint32_t strlen(const char* s) {
  ASSERT(s != NULL);
  const char* p = s;
  while (*p++) {}
  return (p - s - 1);
}


int8_t strcmp (const char* a, const char* b) {
  ASSERT(a != NULL && b != NULL);
  for (; *a && *a == *b; a++, b++) {}
  return *a < *b ? -1 : *a > *b; // a == NULL fallback to *a < *b
}

// locate character in string
// return by addr but not array id
char* strchr(const char* s, const uint8_t c) {
  ASSERT(s != NULL);
  for (;*s; s++) {
    if (*s == c) {
      return (char*)s; // avoid warning?
    }
  }
  return NULL;
}

// like strchr, but reversely
char* strrchr(const char* s, const uint8_t c) {
  ASSERT(s != NULL);
  const char* ret = NULL;
  for (;*s != 0; s++) {
    if (*s == c) {
      ret = s; // update last match
    }
  }
  return (char*)ret;
}

// append src_ to dst_
char* strcat(char* dst_, const char* src_) {
  ASSERT(dst_ != NULL && src_ != NULL);
  char* str = dst_;
  while (*str++) {}
  --str;  // located to NULL
  while((*str++ = *src_++)) {} // end if src is NULL
  return dst_;
}

// count char c in str
uint32_t strchrs(const char* s, uint8_t c) {
  ASSERT(s != NULL);
  uint32_t cnt = 0;
  for (; *s != 0; s++) {
    if (*s == c) {
      cnt++;
    }
  }
  return cnt;
}
