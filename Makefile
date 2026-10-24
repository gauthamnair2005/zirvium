# Zirvium 2.0 — build and boot helpers.
#
# Everything here is 64-bit.  `make` produces a PE32+ UEFI application, a FAT
# ESP image and an El Torito ISO that OVMF can boot directly.

TARGET      := x86_64-unknown-uefi
PROFILE     ?= release
BUILD       := build
EFI         := target/$(TARGET)/$(PROFILE)/zirvium.efi
ESP         := $(BUILD)/esp.img
ISO         := $(BUILD)/zirvium.iso
ISO_ROOT    := $(BUILD)/iso
ESP_SIZE_MB ?= 64

CARGO       ?= cargo
QEMU        ?= qemu-system-x86_64

OVMF_CODE   ?= /usr/share/OVMF/OVMF_CODE_4M.fd
OVMF_VARS   ?= /usr/share/OVMF/OVMF_VARS_4M.fd
QEMU_MEMORY ?= 2G
QEMU_SMP    ?= 8
QEMU_DISPLAY ?= gtk
QEMU_CPU    ?= max
QEMU_VIDEO  ?= virtio-vga

.PHONY: all build esp iso run run-gui run-headless clean fmt clippy check test

all: esp iso

# ---------------------------------------------------------------------------
# Compilation
# ---------------------------------------------------------------------------

build:
	$(CARGO) build --profile $(PROFILE) --target $(TARGET)

check:
	$(CARGO) check --target $(TARGET)

clippy:
	$(CARGO) clippy --target $(TARGET)

fmt:
	$(CARGO) fmt --all

# The std library is `no_std`, so its tests run on the host.
test:
	$(CARGO) test -p zirvstdlib --target x86_64-unknown-linux-gnu

# ---------------------------------------------------------------------------
# Boot media
# ---------------------------------------------------------------------------

esp: build
	mkdir -p $(BUILD)
	rm -f $(ESP)
	dd if=/dev/zero of=$(ESP) bs=1M count=$(ESP_SIZE_MB) status=none
	mformat -i $(ESP) -F -v ZIRVIUM ::
	mmd -i $(ESP) ::/EFI
	mmd -i $(ESP) ::/EFI/BOOT
	mcopy -i $(ESP) $(EFI) ::/EFI/BOOT/BOOTX64.EFI
	@echo "ESP: $(ESP)"

iso: esp
	rm -rf $(ISO_ROOT)
	mkdir -p $(ISO_ROOT)/EFI/BOOT $(ISO_ROOT)/boot
	cp $(EFI) $(ISO_ROOT)/EFI/BOOT/BOOTX64.EFI
	cp $(ESP) $(ISO_ROOT)/boot/esp.img
	xorriso -as mkisofs \
		-o $(ISO) \
		-R -J -V ZIRVIUM \
		-e boot/esp.img -no-emul-boot \
		-isohybrid-gpt-basdat \
		$(ISO_ROOT)
	@echo "ISO: $(ISO)"

# ---------------------------------------------------------------------------
# Running
# ---------------------------------------------------------------------------

# Copy the firmware variables once per run so that boot entries do not leak
# between runs.
$(BUILD)/OVMF_VARS.fd:
	mkdir -p $(BUILD)
	cp $(OVMF_VARS) $(BUILD)/OVMF_VARS.fd

define qemu_common
	$(QEMU) \
		-machine q35,accel=kvm:tcg \
		-cpu $(QEMU_CPU) \
		-m $(QEMU_MEMORY) \
		-smp $(QEMU_SMP),sockets=1,dies=1 \
		-drive if=pflash,format=raw,readonly=on,file=$(OVMF_CODE) \
		-drive if=pflash,format=raw,file=$(BUILD)/OVMF_VARS.fd \
		-drive format=raw,file=$(ESP) \
		-device $(QEMU_VIDEO) \
		-device ich9-intel-hda \
		-device hda-duplex \
		-netdev user,id=zirvium-net \
		-device e1000,netdev=zirvium-net
endef

run: esp $(BUILD)/OVMF_VARS.fd
	$(qemu_common) \
		-serial stdio \
		-display $(QEMU_DISPLAY),gl=on

run-gui: run

run-headless: esp $(BUILD)/OVMF_VARS.fd
	$(qemu_common) \
		-display none \
		-serial stdio

# ---------------------------------------------------------------------------
# Cleaning
# ---------------------------------------------------------------------------

clean:
	$(CARGO) clean
	rm -rf $(BUILD)
