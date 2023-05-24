#include "stdio.h"
#include "interrupt.h"
#include "global.h"
#include "string.h"
#include "syscall.h"
#include "print.h"
#include "debug.h"

// init the arg list head
#define va_start(ap, last)   ap = (va_list)&last
// read content as `type` and update ap
#define va_arg(ap, type)     *((type*)(ap += 4))
#define va_end(ap)           ap = NULL

// integer to ascii (LE)
static void itoa(uint32_t value, char** buf_ptr_addr, uint8_t base) {
  uint32_t m = value % base;
  uint32_t i = value / base;
  if (i) { // many ascii, low addr high bit (LE)
    itoa(i, buf_ptr_addr, base);
  }
  if (m < 10) { // 0~9
    *((*buf_ptr_addr)++) = m + '0';
  }
  else { // a~f
    *((*buf_ptr_addr)++) = m - 10 + 'a';
  }
}

// ap print str into base on format
// return len of str
uint32_t vsprintf(char* str, const char* format, va_list ap) {
  char* bp = str;
  const char* fp = format;
  char cur_char = *fp;
  int32_t arg_int;

  while(cur_char) {
    if (cur_char != '%') {
      *(bp++) = cur_char;
      cur_char = *(++fp);
      continue;
    }
    // the placeholder char
    cur_char = *(++fp);
    switch(cur_char) {
      case 'x':
        // read the content as int
        arg_int = va_arg(ap, int);
        // convert int to ascii
        itoa(arg_int, &bp, 16);
        cur_char = *(++fp);
        break;
      default:
        PANIC("no such format");
    }
  }

  return strlen(str);
}

// legendary
uint32_t printf(const char* format, ...) {

  va_list args;
  va_start(args, format);
  char buf[1024] = {0}; // TODO: fix overflow
  vsprintf(buf, format, args);
  va_end(args);

  return write(buf); 
}
