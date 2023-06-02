#include "exec.h"
#include "thread.h"
#include "stdio-kernel.h"
#include "fs.h"
#include "string.h"
#include "global.h"
#include "memory.h"
#include "assert.h"

#define ET_EXEC 2
#define EM_386  3

extern void intr_exit(void);

typedef uint32_t Elf32_Word;
typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Off;
typedef uint16_t Elf32_Half;

struct Elf32_Ehdr {
  unsigned char e_ident[16];
  Elf32_Half    e_type;
  Elf32_Half    e_machine;
  Elf32_Word    e_version;
  Elf32_Addr    e_entry;
  Elf32_Off     e_phoff;
  Elf32_Off     e_shoff;
  Elf32_Word    e_flags;
  Elf32_Half    e_ehsize;
  Elf32_Half    e_phentsize;
  Elf32_Half    e_phnum;
  Elf32_Half    e_shentsize;
  Elf32_Half    e_shnum;
  Elf32_Half    e_shstrndx;
};

struct Elf32_Phdr {
  Elf32_Word p_type;
  Elf32_Off  p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;
};

enum segment_type {
  PT_NULL,
  PT_LOAD,            // loadbale
  PT_DYNAMIC,
  PT_INTERP,
  PT_NOTE,
  PT_SHLIB,
  PT_PHDR             // program header table
};

// load elf segment to mem
//    (offset, filesz) to (vaddr)
static bool segment_load(int32_t fd, uint32_t offset, uint32_t filesz, uint32_t vaddr) {

  // TODO: force align...
  uint32_t vaddr_first_page = vaddr & 0xfffff000;
  uint32_t size_in_first_page = PG_SIZE - (vaddr & 0x00000fff);

  uint32_t occupy_pages = 1;
  if (filesz > size_in_first_page) {
    uint32_t left_size = filesz - size_in_first_page;
    occupy_pages = DIV_ROUND_UP(left_size, PG_SIZE) + 1;
  }

  uint32_t page_i = 0;
  uint32_t vaddr_page = vaddr_first_page;
  while (page_i < occupy_pages) {
    uint32_t* pde = pde_ptr(vaddr_page);
    uint32_t* pte = pte_ptr(vaddr_page);

    if (!(*pde & 0x00000001) || !(*pte & 0x00000001)) {
      if (get_a_page(PF_USER, vaddr_page) == NULL) {
        return false;
      }
    }
    vaddr_page += PG_SIZE;
    page_i++;
  }

  sys_lseek(fd, offset, SEEK_SET);
  sys_read(fd, (void*)vaddr, filesz);
  return true;
}

// load a whole elf file
static int32_t load(const char* pathname) {

  struct Elf32_Ehdr elf_header;
  struct Elf32_Phdr prog_header;
  memset(&elf_header, 0, sizeof(struct Elf32_Ehdr));

  int32_t fd = sys_open(pathname, O_RDONLY);
  if (fd == -1) {
    printk("load: fail to open file\n");
    return -1;
  }

  if (sys_read(fd, &elf_header, sizeof(struct Elf32_Ehdr)) != sizeof(struct Elf32_Ehdr)) {
    printk("load: fail to read\n");
    goto die;
  }

  // check magic
  //      7f 45 4c 46 01 01 01
  //                  32 le ver
  if (memcmp(elf_header.e_ident, "\177ELF\1\1\1", 7) \
    || elf_header.e_type != ET_EXEC \
    || elf_header.e_machine != EM_386 \
    || elf_header.e_version != 1 \
    || elf_header.e_phnum > 1024 \
    || elf_header.e_phentsize != sizeof(struct Elf32_Phdr)) {
    printk("load: file type doesn't match\n");
    printk("%x %x %x %x %x",
           elf_header.e_type,
           elf_header.e_machine,
           elf_header.e_version,
           elf_header.e_phnum,
           elf_header.e_phentsize);

    goto die;
  }

  Elf32_Off prog_header_offset = elf_header.e_phoff;
  Elf32_Half prog_header_size = elf_header.e_phentsize;

  // foreach phdr
  //    load all loadable segs
  for (uint32_t i = 0; i < elf_header.e_phnum; i++) {
    memset(&prog_header, 0, prog_header_size);

    sys_lseek(fd, prog_header_offset, SEEK_SET);
    if (sys_read(fd, &prog_header, prog_header_size) != prog_header_size) {
      goto die;
    }

    // load loadable seg
    if (PT_LOAD == prog_header.p_type) {
      if (!segment_load(fd, prog_header.p_offset, prog_header.p_filesz, prog_header.p_vaddr)) {
        printk("load: segment_load error\n");
        goto die;
      }
    }

    prog_header_offset += elf_header.e_phentsize;
  }
  return elf_header.e_entry;

die:
  sys_close(fd);
  return -1;
}

// replaces the current process image with a new process image
//      about how is the file stored in the fucking path:
//          dd to raw-disk, then kernel move it to fs-disk
int32_t sys_execv(const char* path, const char* argv[]) {
  uint32_t argc = 0;

  assert(argv);
  while (argv[argc]) {
    argc++;
  }

  int32_t entry_point = load(path);
  if (entry_point == -1) {
    printk("sys_execv: error to load\n");
    return -1;
  }

  struct task_struct* cur = running_thread();


  // filename as proc name
  memcpy(cur->name, path, TASK_NAME_LEN);
  cur->name[TASK_NAME_LEN-1] = 0;

  struct intr_stack* intr_0_stack = (struct intr_stack*)((uint32_t)cur + PG_SIZE - sizeof(struct intr_stack));

  // args
  intr_0_stack->ebx = (int32_t)argv;
  intr_0_stack->ecx = argc;
  // printk("path %s %x\n", path, entry_point);

  intr_0_stack->eip = (void*)entry_point;
  // intr_0_stack->eip = (void*)(0x08049030);
  // intr_0_stack->eip = (void*)(0x080492c0);

  intr_0_stack->esp = (void*)0xc0000000;

  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (intr_0_stack) : "memory");
  return 0;
}
