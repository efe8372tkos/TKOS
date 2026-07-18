# stage2 derle
nasm -f bin boot/stage2.asm -o build/stage2.bin

nasm -f elf64 kernel/kernel_entry.asm -o build/kernel_entry.o
nasm -f elf64 kernel/isr_stubs.asm -o build/isr_stubs.o
nasm -f elf64 kernel/sched_asm.asm -o build/sched_asm.o

for f in framebuffer fb_console marquee keyboard button mouse exec syscall string pcspk heap pic idt pit alloc sched ata fat16 kernel_main; do
  clang -target x86_64-unknown-none \
        -ffreestanding -fno-stack-protector \
        -fno-builtin -mno-red-zone \
        -Ikernel -O2 -c \
        kernel/$f.c -o build/$f.o
done

ld -T linker.ld -nostdlib \
   build/kernel_entry.o \
   build/isr_stubs.o    \
   build/sched_asm.o    \
   build/framebuffer.o  \
   build/fb_console.o   \
   build/keyboard.o     \
   build/button.o         \
   build/string.o       \
   build/pic.o          \
   build/marquee.o     \
   build/fat16.o         \
   build/ata.o          \
   build/exec.o          \
   build/syscall.o       \
   build/mouse.o     \
   build/idt.o          \
   build/pit.o          \
   build/pcspk.o     \
   build/heap.o        \
   build/alloc.o        \
   build/sched.o        \
   build/kernel_main.o  \
   -o build/kernel.elf
   
objcopy -O binary build/kernel.elf build/kernel.bin

# image

dd if=/dev/zero of=build/tkos.img bs=1M count=20
dd if=boot/stage1.bin of=build/tkos.img bs=512 seek=0 conv=notrunc
dd if=build/stage2.bin of=build/tkos.img bs=512 seek=1 conv=notrunc
dd if=build/kernel.bin of=build/tkos.img bs=512 seek=7 conv=notrunc

mkfs.fat -F 16 -n TKOSDATA -S 512 --offset=2048 build/tkos.img 18432

mcopy -i build/tkos.img@@1M HELLO.TXT ::HELLO.TXT


ld -T linker.ld -nostdlib build/kernel_entry.o build/isr_stubs.o build/sched_asm.o build/framebuffer.o build/fb_console.o build/keyboard.o build/string.o build/pic.o build/fat16.o build/ata.o build/mouse.o build/idt.o build/pit.o build/pcspk.o build/heap.o build/alloc.o build/sched.o build/exec.o build/syscall.o build/kernel_main.o -o build/kernel.elf
