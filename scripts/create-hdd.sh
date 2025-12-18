#!/bin/bash
# Create a bootable disk image with GRUB

set -e

echo "Creating bootable disk image..."

# Create a 64MB disk image
dd if=/dev/zero of=zirvium.img bs=1M count=64 2>/dev/null

# Create partition table
parted -s zirvium.img mklabel msdos
parted -s zirvium.img mkpart primary ext2 1MiB 100%
parted -s zirvium.img set 1 boot on

# Setup loop device
LOOP=$(sudo losetup -f --show zirvium.img)
sudo partprobe $LOOP
LOOPPART="${LOOP}p1"

# Format partition
sudo mkfs.ext2 -F $LOOPPART

# Mount partition
mkdir -p mnt
sudo mount $LOOPPART mnt

# Create directory structure
sudo mkdir -p mnt/boot/grub

# Copy kernel
sudo cp zirvium mnt/boot/zirvium.elf
sudo cp vmzirvium mnt/boot/vmzirvium

# Create GRUB config
sudo tee mnt/boot/grub/grub.cfg > /dev/null << 'GRUBEOF'
set timeout=3
set default=0

menuentry "Zirvium OS" {
    multiboot2 /boot/zirvium.elf
    boot
}

menuentry "Zirvium OS (vmzirvium)" {
    multiboot2 /boot/zirvium.elf
    boot
}
GRUBEOF

# Install GRUB
sudo grub-install --target=i386-pc --boot-directory=mnt/boot $LOOP

# Cleanup
sudo umount mnt
sudo losetup -d $LOOP
rmdir mnt

echo "Disk image created: zirvium.img"
ls -lh zirvium.img
