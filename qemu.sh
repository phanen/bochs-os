qemu-system-i386 \
  -boot c,menu=off \
  -drive file=master-hd60M.img,format=raw,if=ide,index=0,media=disk \
  -drive file=slave-hd80M.img,format=raw,if=ide,index=1,media=disk \
  -display gtk,show-tabs=on \
  -m 32 \
  -S -s \
  # -bios /usr/share/qemu/bios.bin \
  # -bios /usr/share/bochs/BIOS-bochs-latest \
  # -kernel build/kernel.bin \
  # -machine virt,acpi=off \
  # -nographic\
  # -display egl-headless\
  # -name giaogiao
  # -nographic
  # -chardev stdio,mux=on,id=com1 \
  # -cpu core2duo \
  # -bios /usr/share/qemu/bios.bin \
  # -vga std \
  # -smp 1,cores=1,threads=1 \
  # -machine type=pc,accel=kvm \
  # -device ich9-ahci,id=ahci \
  # -device ide-drive,drive=drive0,bus=ide.0 \
  # -device ide-drive,drive=drive1,bus=ide.1 \
