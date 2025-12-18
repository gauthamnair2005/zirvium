#!/bin/bash
# Demo boot of Zirvium OS

echo "═══════════════════════════════════════════════════════"
echo "  Zirvium OS - Boot Demo"
echo "═══════════════════════════════════════════════════════"
echo ""
echo "Kernel Information:"
echo "  - vmzirvium: $(ls -lh vmzirvium | awk '{print $5}') (compressed)"
echo "  - zirvium:   $(ls -lh zirvium | awk '{print $5}') (ELF)"
echo ""
echo "Booting with QEMU (i386)..."
echo "  Memory: 512 MB"
echo "  Boot: CD-ROM (GRUB + zirvium.iso)"
echo ""
echo "Note: Kernel will boot but may not show output yet."
echo "      This is a work-in-progress boot demonstration."
echo ""
echo "Starting in 2 seconds..."
sleep 2

qemu-system-i386 \
    -cdrom zirvium.iso \
    -m 512M \
    -serial mon:stdio \
    -no-reboot
