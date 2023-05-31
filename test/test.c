#include "test.h"

void test_main() {
  // process_execute(u_prog_a, "user_prog_a");
  // process_execute(u_prog_b, "user_prog_b");
  // process_execute(u_prog_c, "user_prog_c");
  // process_execute(u_prog_d, "user_prog_d");
  // intr_enable();
  // thread_create("k_thread_a", 31, k_thread_a, "thread_a");
  // thread_create("k_thread_b", 31, k_thread_b, "thread_b");
  // thread_create("k_thread_c", 31, k_thread_c, "thread_c");
  // thread_create("k_thread_d", 31, k_thread_d, "thread_d");
  // console_put_str(" main_pid:0x");
  // console_put_int(sys_getpid());
  // console_put_char('\n');

  // void* addr = sys_malloc(33);
  // printk("%s, pid:%d addr:0x%x %c", "main", sys_getpid(), (int)addr, '\n');
  // process_execute(u_prog_a, "u_prog_a");
  // process_execute(u_prog_b, "u_prog_b");
  // thread_create("k_thread_a", 31, k_thread_a, "I am thread_a");
  // thread_create("k_thread_b", 31, k_thread_b, "I am thread_b");

  // test_dir();
  // test_openclosedir();
  // test_api_dir();
  // test_cwd();
  test_stat();
  while(1);

}

void test_stat() {
   struct stat obj_stat;
   sys_stat("/", &obj_stat);
   printf("/`s info\n   i_no:%d\n   size:%d\n   filetype:%s\n", \
	 obj_stat.st_ino, obj_stat.st_size, \
	 obj_stat.st_filetype == 2 ? "directory" : "regular");
   sys_stat("/dir1", &obj_stat);
   printf("/dir1`s info\n   i_no:%d\n   size:%d\n   filetype:%s\n", \
	 obj_stat.st_ino, obj_stat.st_size, \
	 obj_stat.st_filetype == 2 ? "directory" : "regular");
}

void test_cwd() {
  char cwd_buf[12] = {0};
  sys_getcwd(cwd_buf, 12);
  printf("cwd:%s\n", cwd_buf);

  sys_chdir("/dir1");

  printf("change cwd now\n");
  sys_getcwd(cwd_buf, 12);
  printf("cwd:%s\n", cwd_buf);
}

void test_api_dir() {

  printf("/dir1 content before delete /dir1/subdir1:\n");
  struct dir* dir = sys_opendir("/dir1/");
  char* type = NULL;
  struct dir_entry* dir_e = NULL;
  while((dir_e = sys_readdir(dir))) {
    if (dir_e->f_type == FT_REGULAR) {
      type = "regular";
    } else {
      type = "directory";
    }
    printf("      %s   %s\n", type, dir_e->filename);
  }

  printf("try to delete nonempty directory /dir1/subdir1\n");
  if (sys_rmdir("/dir1/subdir1") == -1) {
    printf("sys_rmdir: /dir1/subdir1 delete fail!\n");
  }

  printf("try to delete /dir1/subdir1/file2\n");
  if (sys_rmdir("/dir1/subdir1/file2") == -1) {
    printf("sys_rmdir: /dir1/subdir1/file2 delete fail!\n");
  }

  if (sys_unlink("/dir1/subdir1/file2") == 0 ) {
    printf("sys_unlink: /dir1/subdir1/file2 delete done\n");
  }

  printf("try to delete directory /dir1/subdir1 again\n");
  if (sys_rmdir("/dir1/subdir1") == 0) {
    printf("/dir1/subdir1 delete done!\n");
  }

  printf("/dir1 content after delete /dir1/subdir1:\n");
  sys_rewinddir(dir);

  while((dir_e = sys_readdir(dir))) {
    if (dir_e->f_type == FT_REGULAR) {
      type = "regular";
    } else {
      type = "directory";
    }
    printf("      %s   %s\n", type, dir_e->filename);
  }

}

void test_dir() {

  // dir can only be make when its father exist

  printf("/dir1/subdir1 create %s!\n", sys_mkdir("/dir1/subdir1") == 0 ? "done" : "fail");
  printf("/dir1 create %s!\n", sys_mkdir("/dir1") == 0 ? "done" : "fail");
  printf("now, /dir1/subdir1 create %s!\n", sys_mkdir("/dir1/subdir1") == 0 ? "done" : "fail");

  int fd = sys_open("/dir1/subdir1/file2", O_CREAT|O_RDWR);

  if (fd != -1) {
    printf("/dir1/subdir1/file2 create done!\n");

    sys_write(fd, "fucking hungry!!!\n", 21);
    sys_lseek(fd, 0, SEEK_SET);

    char buf[32] = {0};

    sys_read(fd, buf, 21);

    printf("/dir1/subdir1/file2 says:\n%s", buf);

    sys_close(fd);
  }
  while(1);

}

void test_file() {
  // sys_open("/file1", O_CREAT);

  // create
  uint32_t fd = sys_open("/file1", O_CREAT);
  printf("create fd:%d\n", fd);
  sys_close(fd);

  // write
  fd = sys_open("/file1", O_RDWR);
  printf("write fd:%d\n", fd);
  sys_write(fd, "hello,world\n", 12);
  sys_write(fd, "hello,world\n", 12);
  sys_close(fd);
  printf("%d closed now\n", fd);

  // read
  fd = sys_open("/file1", O_RDWR);
  printf("read fd:%d\n", fd);
  printf("open /file1, fd:%d\n", fd);

  char buf[64] = {0};
  int read_bytes = sys_read(fd, buf, 18);
  // hello,world\nhello
  printf("1_ read %d bytes:\n%s\n", read_bytes, buf);

  memset(buf, 0, 64);
  read_bytes = sys_read(fd, buf, 6);
  // world\n
  printf("2_ read %d bytes:\n%s", read_bytes, buf);

  memset(buf, 0, 64);
  // nothing
  read_bytes = sys_read(fd, buf, 6);
  printf("3_ read %d bytes:\n%s", read_bytes, buf);

  // reset fd_pos by close and reopen
  // printf("________  close file1 and reopen  ________\n");
  // sys_close(fd);

  // reset fd_pos by lseek
  sys_lseek(fd, 0, SEEK_SET);
  memset(buf, 0, 64);
  read_bytes = sys_read(fd, buf, 24);
  printf("4_ read %d bytes:\n%s", read_bytes, buf);

  sys_close(fd);

  // printf("/file1 delete %s!\n", sys_unlink("/file1") == 0 ? "done" : "fail");
}

void k_thread_c(void* arg) {
  char* para = arg;
  void* addr1;
  void* addr2;
  void* addr3;
  void* addr4;
  void* addr5;
  void* addr6;
  void* addr7;
  console_put_str("  thread_a start\n");
  int max = 10;
  while (max-- > 0) {
    int size = 128;
    addr1 = sys_malloc(size);
    size *= 2;
    addr2 = sys_malloc(size);
    size *= 2;
    addr3 = sys_malloc(size);
    sys_free(addr1);
    addr4 = sys_malloc(size);
    size *= 2; size *= 2; size *= 2; size *= 2;
    size *= 2; size *= 2; size *= 2;
    addr5 = sys_malloc(size);
    addr6 = sys_malloc(size);
    sys_free(addr5);
    size *= 2;
    addr7 = sys_malloc(size);
    sys_free(addr6);
    sys_free(addr7);
    sys_free(addr2);
    sys_free(addr3);
    sys_free(addr4);
  }
  console_put_str(" thread_a end\n");
  while(1);
}

void k_thread_d(void* arg) {
  char* para = arg;
  void* addr1;
  void* addr2;
  void* addr3;
  void* addr4;
  void* addr5;
  void* addr6;
  void* addr7;
  void* addr8;
  void* addr9;
  int max = 10;
  console_put_str(" thread_b start\n");
  while (max-- > 0) {
    int size = 9;
    addr1 = sys_malloc(size);
    size *= 2;
    addr2 = sys_malloc(size);
    size *= 2;
    sys_free(addr2);
    addr3 = sys_malloc(size);
    sys_free(addr1);
    addr4 = sys_malloc(size);
    addr5 = sys_malloc(size);
    addr6 = sys_malloc(size);
    sys_free(addr5);
    size *= 2;
    addr7 = sys_malloc(size);
    sys_free(addr6);
    sys_free(addr7);
    sys_free(addr3);
    sys_free(addr4);

    size *= 2; size *= 2; size *= 2;
    addr1 = sys_malloc(size);
    addr2 = sys_malloc(size);
    addr3 = sys_malloc(size);
    addr4 = sys_malloc(size);
    addr5 = sys_malloc(size);
    addr6 = sys_malloc(size);
    addr7 = sys_malloc(size);
    addr8 = sys_malloc(size);
    addr9 = sys_malloc(size);
    sys_free(addr1);
    sys_free(addr2);
    sys_free(addr3);
    sys_free(addr4);
    sys_free(addr5);
    sys_free(addr6);
    sys_free(addr7);
    sys_free(addr8);
    sys_free(addr9);
  }
  console_put_str(" thread_b end\n");
  while(1);
}

void k_thread_a(void* arg) {
  char* para = arg;
  void* addr = sys_malloc(31);
  printk("%s, pid:%d addr:0x%x %c", para, sys_getpid(), (int)addr, '\n');
  while (1) {
  }
}

void k_thread_b(void* arg) {

  char* para = arg;
  void* addr = sys_malloc(63);
  printk("%s, pid:%d addr:0x%x %c", para, sys_getpid(), (int)addr, '\n');
  while (1) {
  }
}

void u_prog_a(void) {
  char* name = "uprog_a";
  printf("%s, pid:%d %c", name, getpid(),'\n');
  while(1) {
    bss_var_a = getpid();
  }

}

void u_prog_b(void) {
  char* name = "uprog_b";
  printf("%s, pid:%d %c", name, getpid(),'\n');
  while(1) {
    bss_var_b = getpid();
  }
}

void u_prog_c(void) {
  void* addr1 = malloc(256);
  void* addr2 = malloc(255);
  void* addr3 = malloc(254);
  printf(" prog_a malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);

  int cpu_delay = 1000;
  while(cpu_delay-- > 0);
  free(addr1);
  free(addr2);
  free(addr3);
  while(1);
}

void u_prog_d(void) {
  void* addr1 = malloc(256);
  void* addr2 = malloc(255);
  void* addr3 = malloc(254);
  printf(" prog_b malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);

  int cpu_delay = 1000;
  while(cpu_delay-- > 0);
  free(addr1);
  free(addr2);
  free(addr3);
  while(1);
}
