#include "string.h"
#include "global.h"
// #include "debug.h"
#include "assert.h"

void *memset(void *s, uint8_t value, uint32_t n)
{
	assert(s != NULL);
	uint8_t *p = (uint8_t *)s;
	while (n--)
		*p++ = value;
	return s;
}

void *memcpy(void *dst, const void *src, uint32_t n)
{
	assert(dst != NULL && src != NULL);
	uint8_t *d = dst;
	const uint8_t *s = src;
	while (n--)
		*d++ = *s++;
	return dst;
}

int memcmp(const void *s1, const void *s2, uint32_t n)
{
	assert(s1 != NULL || s2 != NULL);
	const char *a = s1;
	const char *b = s2;
	for (; n--; a++, b++) {
		if (*a != *b) {
			return *a > *b ? 1 : -1;
		}
	}
	return 0;
}

char *strcpy(char *dst_, const char *src_)
{
	assert(dst_ != NULL && src_ != NULL);
	char *r = dst_;
	while ((*dst_++ = *src_++)) {
	}
	return r;
}

uint32_t strlen(const char *s)
{
	assert(s != NULL);
	const char *p = s;
	while (*p++) {
	}
	return (p - s - 1);
}

int8_t strcmp(const char *a, const char *b)
{
	assert(a != NULL && b != NULL);
	for (; *a && *a == *b; a++, b++) {
	}
	return *a < *b ? -1 : *a > *b; // a == NULL fallback to *a < *b
}

// locate character in string
// return by addr but not array id
char *strchr(const char *s, const uint8_t c)
{
	assert(s != NULL);
	for (; *s; s++) {
		if (*s == c) {
			return (char *)s; // avoid warning?
		}
	}
	return NULL;
}

// like strchr, but reversely
char *strrchr(const char *s, const uint8_t c)
{
	assert(s != NULL);
	const char *ret = NULL;
	for (; *s != 0; s++) {
		if (*s == c) {
			ret = s; // update last match
		}
	}
	return (char *)ret;
}

// append src_ to dst_
char *strcat(char *dst_, const char *src_)
{
	assert(dst_ != NULL && src_ != NULL);
	char *str = dst_;
	while (*str++) {
	}
	--str; // located to NULL
	while ((*str++ = *src_++)) {
	} // end if src is NULL
	return dst_;
}

// count char c in str
uint32_t strchrs(const char *s, uint8_t c)
{
	assert(s != NULL);
	uint32_t cnt = 0;
	for (; *s != 0; s++) {
		if (*s == c) {
			cnt++;
		}
	}
	return cnt;
}
