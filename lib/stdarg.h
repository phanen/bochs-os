// init the arg list head
#define va_start(ap, last) ap = (va_list)&last
// read content as `type` and update ap
#define va_arg(ap, type) *((type *)(ap += 4))
#define va_end(ap) ap = NULL
