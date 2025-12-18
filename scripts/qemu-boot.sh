#!/bin/bash
# Boot Zirvium with QEMU using existing ISO

set -e

echo "╔════════════════════════════════════════════════╗"
echo "║     Booting Zirvium OS with QEMU + GRUB       ║"
echo "╚════════════════════════════════════════════════╝"
echo ""

# Check if ISO exists
if [ ! -f "zirvium.iso" ]; then
    echo "Building ISO..."
    make iso > /dev/null 2>&1
fi

echo "Kernel: $(ls -lh zirvium | awk '{print $5}')"
echo "vmzirvium: $(ls -lh vmzirvium | awk '{print $5}')"
echo "ISO: $(ls -lh zirvium.iso | awk '{print $5}')"
echo ""
echo "Starting QEMU..."
echo "  Memory: 512 MB"
echo "  Boot: CD-ROM (zirvium.iso)"
echo "  Serial: stdio"
echo ""
echo "Press Ctrl+C to stop"
echo "─────────────────────────────────────────────────"
echo ""

# Boot with QEMU
qemu-system-i386 \
    -cdrom zirvium.iso \
    -m 512M \
    -serial stdio \
    "$@"
