#!/usr/bin/env python3
"""
Zirvium Kernel Configuration Menu (TUI)
Advanced ncurses-based configuration interface
"""

import curses
import os
import sys

class MenuConfig:
    def __init__(self, stdscr):
        self.stdscr = stdscr
        self.config_file = ".config"
        self.config = self.load_config()
        self.current_row = 0
        self.top_row = 0
        
        # Menu structure
        self.menu_items = [
            ("General Setup", [
                ("CONFIG_COMPRESS_KERNEL", "Compress kernel image", "y"),
                ("CONFIG_KERNEL_GZIP", "Use Gzip compression", "y"),
                ("CONFIG_MODULAR", "Loadable module support", "y"),
            ]),
            ("Architecture", [
                ("CONFIG_X86_64", "64-bit x86 architecture", "y"),
                ("CONFIG_ARM64", "64-bit ARM architecture", "n"),
                ("CONFIG_RISCV", "RISC-V architecture", "n"),
                ("CONFIG_PPC", "PowerPC architecture", "n"),
            ]),
            ("Processor Features", [
                ("CONFIG_SMP", "Symmetric Multi-Processing", "y"),
                ("CONFIG_HYPER_THREADING", "Hyper-Threading support", "y"),
                ("CONFIG_NR_CPUS", "Maximum CPUs (64)", "64"),
            ]),
            ("Networking", [
                ("CONFIG_NET", "Networking support", "y"),
                ("CONFIG_INET", "TCP/IP networking", "y"),
                ("CONFIG_IPV6", "IPv6 support", "y"),
                ("CONFIG_WIRELESS", "Wireless support", "y"),
            ]),
            ("Network Drivers", [
                ("CONFIG_REALTEK_8169", "Realtek RTL8169", "y"),
                ("CONFIG_INTEL_E1000", "Intel E1000", "y"),
                ("CONFIG_BROADCOM_TIGON3", "Broadcom Tigon3", "y"),
                ("CONFIG_INTEL_WIFI", "Intel WiFi", "y"),
                ("CONFIG_BROADCOM_WIFI", "Broadcom WiFi", "y"),
            ]),
            ("Input Devices", [
                ("CONFIG_INPUT_KEYBOARD", "Keyboard support", "y"),
                ("CONFIG_KEYBOARD_USB", "USB keyboard", "y"),
                ("CONFIG_KEYBOARD_RGB", "RGB keyboard backlight", "n"),
                ("CONFIG_INPUT_MOUSE", "Mouse support", "y"),
                ("CONFIG_INPUT_TOUCHPAD", "Touchpad support", "y"),
                ("CONFIG_INPUT_TOUCHSCREEN", "Touchscreen support", "y"),
            ]),
            ("USB Support", [
                ("CONFIG_USB_SUPPORT", "USB support", "y"),
                ("CONFIG_USB_XHCI_HCD", "USB 3.0 (xHCI)", "y"),
                ("CONFIG_USB_EHCI_HCD", "USB 2.0 (EHCI)", "y"),
                ("CONFIG_USB4", "USB4/Thunderbolt", "y"),
            ]),
            ("Graphics", [
                ("CONFIG_DRM", "Direct Rendering Manager", "y"),
                ("CONFIG_DRM_NVIDIA", "NVIDIA GPU", "y"),
                ("CONFIG_DRM_AMD", "AMD GPU", "y"),
                ("CONFIG_DRM_INTEL", "Intel GPU", "y"),
                ("CONFIG_FB_VESA", "VESA framebuffer", "y"),
            ]),
            ("Display", [
                ("CONFIG_HDMI_SUPPORT", "HDMI support", "y"),
                ("CONFIG_DISPLAY_PORT", "DisplayPort", "y"),
                ("CONFIG_EDP_SUPPORT", "eDP support", "y"),
                ("CONFIG_VGA_SUPPORT", "VGA support", "y"),
            ]),
            ("Audio", [
                ("CONFIG_SOUND", "Sound support", "y"),
                ("CONFIG_SND_HDA_INTEL", "Intel HD Audio", "y"),
                ("CONFIG_SND_HDA_AMD", "AMD HD Audio", "y"),
                ("CONFIG_SND_USB_AUDIO", "USB Audio", "y"),
            ]),
            ("Bluetooth", [
                ("CONFIG_BT", "Bluetooth support", "y"),
                ("CONFIG_BT_INTEL", "Intel Bluetooth", "y"),
                ("CONFIG_BT_BROADCOM", "Broadcom Bluetooth", "y"),
            ]),
            ("Sensors", [
                ("CONFIG_SENSORS", "Sensor support", "y"),
                ("CONFIG_SENSORS_ACCEL", "Accelerometer", "y"),
                ("CONFIG_SENSORS_GYRO", "Gyroscope", "y"),
                ("CONFIG_SENSORS_PROXIMITY", "Proximity", "y"),
                ("CONFIG_SENSORS_LIGHT", "Light sensor", "y"),
                ("CONFIG_SENSORS_FINGERPRINT", "Fingerprint", "y"),
            ]),
            ("Webcams", [
                ("CONFIG_VIDEO_WEBCAM", "Webcam support", "y"),
                ("CONFIG_WEBCAM_LOGITECH", "Logitech webcams", "y"),
                ("CONFIG_WEBCAM_MICROSOFT", "Microsoft webcams", "y"),
                ("CONFIG_WEBCAM_GENERIC", "Generic webcams", "y"),
            ]),
            ("Storage", [
                ("CONFIG_ATA", "ATA/SATA support", "y"),
                ("CONFIG_NVME", "NVMe support", "y"),
                ("CONFIG_MMC", "MMC/SD support", "y"),
                ("CONFIG_CDROM", "CD-ROM support", "y"),
            ]),
            ("Cryptography", [
                ("CONFIG_CRYPTO", "Crypto API", "y"),
                ("CONFIG_CRYPTO_AES", "AES cipher", "y"),
                ("CONFIG_CRYPTO_SHA256", "SHA256", "y"),
            ]),
            ("Virtualization", [
                ("CONFIG_HYPERVISOR_GUEST", "Hypervisor guest", "y"),
                ("CONFIG_KVM_GUEST", "KVM guest", "y"),
                ("CONFIG_VMWARE_GUEST", "VMware guest", "y"),
                ("CONFIG_VIRTIO", "Virtio drivers", "y"),
            ]),
        ]
        
        self.flatten_menu()
        
    def flatten_menu(self):
        self.flat_menu = []
        for category, items in self.menu_items:
            self.flat_menu.append(("CATEGORY", category, None))
            for item in items:
                self.flat_menu.append(("ITEM", item[0], item[1]))
    
    def load_config(self):
        config = {}
        if os.path.exists(self.config_file):
            with open(self.config_file, 'r') as f:
                for line in f:
                    line = line.strip()
                    if line and not line.startswith('#') and '=' in line:
                        key, value = line.split('=', 1)
                        config[key] = value
        return config
    
    def save_config(self):
        with open(self.config_file, 'w') as f:
            f.write("# Zirvium Kernel Configuration\n")
            f.write("# Generated by menuconfig\n\n")
            for category, items in self.menu_items:
                f.write(f"\n# {category}\n")
                for item in items:
                    key = item[0]
                    value = self.config.get(key, item[2])
                    f.write(f"{key}={value}\n")
    
    def draw_header(self):
        height, width = self.stdscr.getmaxyx()
        header = " Zirvium Kernel Configuration "
        self.stdscr.attron(curses.color_pair(1))
        self.stdscr.addstr(0, (width - len(header)) // 2, header)
        self.stdscr.attroff(curses.color_pair(1))
        self.stdscr.addstr(1, 0, "=" * width)
    
    def draw_footer(self):
        height, width = self.stdscr.getmaxyx()
        footer = " <Space> Toggle  <Enter> Select  <S> Save  <Q> Quit "
        self.stdscr.attron(curses.color_pair(1))
        self.stdscr.addstr(height - 1, (width - len(footer)) // 2, footer)
        self.stdscr.attroff(curses.color_pair(1))
    
    def draw_menu(self):
        height, width = self.stdscr.getmaxyx()
        max_items = height - 4
        
        for idx, (item_type, key, desc) in enumerate(self.flat_menu[self.top_row:self.top_row + max_items]):
            y = idx + 2
            
            if item_type == "CATEGORY":
                self.stdscr.attron(curses.color_pair(2) | curses.A_BOLD)
                self.stdscr.addstr(y, 2, f"[{key}]")
                self.stdscr.attroff(curses.color_pair(2) | curses.A_BOLD)
            else:
                value = self.config.get(key, "n")
                marker = "[*]" if value == "y" else "[ ]"
                
                if self.current_row == self.top_row + idx:
                    self.stdscr.attron(curses.color_pair(3))
                    self.stdscr.addstr(y, 2, f" {marker} {desc}".ljust(width - 4))
                    self.stdscr.attroff(curses.color_pair(3))
                else:
                    self.stdscr.addstr(y, 2, f" {marker} {desc}")
    
    def toggle_current(self):
        item_type, key, desc = self.flat_menu[self.current_row]
        if item_type == "ITEM":
            current = self.config.get(key, "n")
            self.config[key] = "n" if current == "y" else "y"
    
    def run(self):
        curses.curs_set(0)
        curses.init_pair(1, curses.COLOR_BLACK, curses.COLOR_CYAN)
        curses.init_pair(2, curses.COLOR_YELLOW, curses.COLOR_BLACK)
        curses.init_pair(3, curses.COLOR_BLACK, curses.COLOR_WHITE)
        
        while True:
            self.stdscr.clear()
            self.draw_header()
            self.draw_menu()
            self.draw_footer()
            self.stdscr.refresh()
            
            key = self.stdscr.getch()
            
            height, width = self.stdscr.getmaxyx()
            max_items = height - 4
            
            if key == curses.KEY_UP and self.current_row > 0:
                self.current_row -= 1
                if self.current_row < self.top_row:
                    self.top_row -= 1
            elif key == curses.KEY_DOWN and self.current_row < len(self.flat_menu) - 1:
                self.current_row += 1
                if self.current_row >= self.top_row + max_items:
                    self.top_row += 1
            elif key == ord(' '):
                self.toggle_current()
            elif key == ord('s') or key == ord('S'):
                self.save_config()
                self.stdscr.addstr(height - 2, 2, "Configuration saved!")
                self.stdscr.refresh()
                curses.napms(1000)
            elif key == ord('q') or key == ord('Q'):
                break

def main(stdscr):
    menu = MenuConfig(stdscr)
    menu.run()

if __name__ == "__main__":
    try:
        curses.wrapper(main)
    except KeyboardInterrupt:
        pass
