#include "stdio.h"
#include "file.h"
#include "interrupt.h"
#include "global.h"
#include "string.h"
#include "syscall.h"
#include "print.h"
#include "assert.h"

#include "stdarg.h"

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
  char* arg_str;

  while(cur_char) {
    if (cur_char != '%') {
      *(bp++) = cur_char;
      cur_char = *(++fp);
      continue;
    }
    // the placeholder char
    cur_char = *(++fp);
    switch(cur_char) {
      case 'x': // hex
        // read the content as int
        arg_int = va_arg(ap, int);
        // int to hex ascii
        itoa(arg_int, &bp, 16);
        cur_char = *(++fp);
        break;

      case 'd': // oct
        arg_int = va_arg(ap, int);
        if (arg_int < 0) {
          arg_int = 0 - arg_int;
          *bp++ = '-';
        }
        itoa(arg_int, &bp, 10);
        cur_char = *(++fp);
        break;

      case 'c': // char
        *(bp++) = va_arg(ap, char);
        cur_char = *(++fp);
        break;

      case 's': // string
        arg_str = va_arg(ap, char*);
        strcpy(bp, arg_str);
        bp += strlen(arg_str);
        cur_char = *(++fp);
        break;


      default:
        panic("no such format");
    }
  }

  return strlen(str);
}

// print format to string
uint32_t sprintf(char* buf, const char* format, ...) {
   va_list args;
   uint32_t retval;
   va_start(args, format);
   retval = vsprintf(buf, format, args);
   va_end(args);
   return retval;
}

// legendary
uint32_t printf(const char* format, ...) {

  va_list args;
  va_start(args, format);
  char buf[1024] = {0}; // TODO: fix overflow
  vsprintf(buf, format, args);
  va_end(args);

  return write(stdout_no, buf, strlen(buf));
}
