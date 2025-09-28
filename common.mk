AS = nasm
# AS = yasm
CC = clang
LD = ld

ASFLAGS = -f elf -g
CFLAGS = -m32 -static -fno-builtin -nostdlib -fno-stack-protector \
		 -mno-sse -g
		 # -W -Wstrict-prototypes -Wmissing-prototypes

ENTRY_POINT = 0xc0001500
LDFLAGS = -e main -static -Ttext $(ENTRY_POINT) -m elf_i386

QFLAGS= -m 4G \
		-machine pc \
		-smp 1 \
		-boot order=d \
		-drive file=boot.img,if=ide,format=raw,index=0,media=disk \
		-drive file=fs.img,if=ide,format=raw,index=1,media=disk \
		-serial mon:stdio \
		-d unimp,guest_errors,int,cpu_reset \
		--no-reboot
		# -nographic
		# -bios ./script/BIOS-bochs-legacy \

QEMU = qemu-system-i386

BOCHS = bochs -q
# BOCHS = /usr/bin/bochs -q
BCONF = script/bochs.conf
# BXIMAGE = bximage
