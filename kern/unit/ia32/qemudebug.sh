#! /bin/sh

#qemu-system-i386 -s -S -soundhw sb16 -cdrom cd.iso -m 256
qemu-system-i386 -s -S -vga std -cdrom cd.iso -m 512

