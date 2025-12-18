#!/usr/bin/env python3
"""
Zirvium Advanced Kernel Configuration Menu
Feature-rich TUI with dependency checking, conflict resolution, and search
"""

import curses
import os
import sys
import json
import re
from typing import Dict, List, Tuple, Optional, Set

class ConfigOption:
    """Represents a configuration option with dependencies and conflicts"""
    def __init__(self, name: str, desc: str, default: str, 
                 depends: List[str] = None, conflicts: List[str] = None,
                 help_text: str = "", option_type: str = "bool"):
        self.name = name
        self.desc = desc
        self.default = default
        self.depends = depends or []
        self.conflicts = conflicts or []
        self.help_text = help_text
        self.option_type = option_type  # bool, tristate, int, string, choice
        self.value = default
        
    def can_enable(self, config: Dict) -> Tuple[bool, str]:
        """Check if option can be enabled given current config"""
        # Check dependencies
        for dep in self.depends:
            if dep not in config or config[dep] not in ['y', 'm']:
                return False, f"Depends on {dep} being enabled"
        
        # Check conflicts
        for conf in self.conflicts:
            if conf in config and config[conf] == 'y':
                return False, f"Conflicts with {conf}"
        
        return True, ""

class MenuConfig:
    def __init__(self, stdscr):
        self.stdscr = stdscr
        self.config_file = ".config"
        self.config = {}
        self.options = self.build_options()
        self.current_row = 0
        self.top_row = 0
        self.current_menu = "main"
        self.menu_stack = []
        self.search_mode = False
        self.search_query = ""
        self.status_message = ""
        self.show_help = False
        self.help_text = ""
        self.filter_mode = "all"  # all, enabled, modified
        self.load_config()
        
        # Initialize colors
        curses.init_pair(1, curses.COLOR_BLACK, curses.COLOR_CYAN)     # Header
        curses.init_pair(2, curses.COLOR_YELLOW, curses.COLOR_BLACK)   # Category
        curses.init_pair(3, curses.COLOR_BLACK, curses.COLOR_WHITE)    # Selected
        curses.init_pair(4, curses.COLOR_RED, curses.COLOR_BLACK)      # Error
        curses.init_pair(5, curses.COLOR_GREEN, curses.COLOR_BLACK)    # Success
        curses.init_pair(6, curses.COLOR_BLUE, curses.COLOR_BLACK)     # Info
        curses.init_pair(7, curses.COLOR_MAGENTA, curses.COLOR_BLACK)  # Modified
        curses.init_pair(8, curses.COLOR_CYAN, curses.COLOR_BLACK)     # Help
        
    def build_options(self) -> Dict[str, Dict]:
        """Build comprehensive option tree with dependencies and conflicts"""
        return {
            "main": {
                "title": "Main Menu",
                "items": [
                    ("General Setup", "general", "submenu"),
                    ("Processor & Architecture", "arch", "submenu"),
                    ("Boot Options", "boot", "submenu"),
                    ("Memory Management", "memory", "submenu"),
                    ("Process Management", "process", "submenu"),
                    ("File Systems", "filesystems", "submenu"),
                    ("Networking", "networking", "submenu"),
                    ("Device Drivers", "drivers", "submenu"),
                    ("Security", "security", "submenu"),
                    ("Virtualization", "virtualization", "submenu"),
                    ("Power Management", "power", "submenu"),
                    ("Debugging", "debug", "submenu"),
                ]
            },
            "general": {
                "title": "General Setup",
                "items": [
                    ConfigOption("CONFIG_KERNEL_VERSION", "Kernel version string", "0.2.0", 
                                option_type="string", help_text="Version of the kernel"),
                    ConfigOption("CONFIG_LOCALVERSION", "Local version append", "", 
                                option_type="string"),
                    ConfigOption("CONFIG_COMPRESS_KERNEL", "Compress kernel image", "y",
                                help_text="Reduce kernel size using compression"),
                    ConfigOption("CONFIG_KERNEL_GZIP", "Use Gzip compression", "y",
                                depends=["CONFIG_COMPRESS_KERNEL"],
                                conflicts=["CONFIG_KERNEL_XZ", "CONFIG_KERNEL_LZ4"]),
                    ConfigOption("CONFIG_KERNEL_XZ", "Use XZ compression", "n",
                                depends=["CONFIG_COMPRESS_KERNEL"],
                                conflicts=["CONFIG_KERNEL_GZIP", "CONFIG_KERNEL_LZ4"]),
                    ConfigOption("CONFIG_KERNEL_LZ4", "Use LZ4 compression (fast)", "n",
                                depends=["CONFIG_COMPRESS_KERNEL"],
                                conflicts=["CONFIG_KERNEL_GZIP", "CONFIG_KERNEL_XZ"]),
                    ConfigOption("CONFIG_MODULAR", "Loadable module support", "y",
                                help_text="Enable loading kernel modules at runtime"),
                    ConfigOption("CONFIG_MODULE_UNLOAD", "Module unloading", "y",
                                depends=["CONFIG_MODULAR"]),
                    ConfigOption("CONFIG_MODULE_FORCE_UNLOAD", "Forced module unloading", "n",
                                depends=["CONFIG_MODULE_UNLOAD"]),
                ]
            },
            "arch": {
                "title": "Processor & Architecture",
                "items": [
                    ConfigOption("CONFIG_X86_64", "x86_64 (64-bit)", "y",
                                conflicts=["CONFIG_X86", "CONFIG_ARM", "CONFIG_ARM64"],
                                help_text="Intel/AMD 64-bit architecture"),
                    ConfigOption("CONFIG_X86", "x86 (32-bit)", "n",
                                conflicts=["CONFIG_X86_64", "CONFIG_ARM", "CONFIG_ARM64"]),
                    ConfigOption("CONFIG_ARM64", "ARM64 (AArch64)", "n",
                                conflicts=["CONFIG_X86", "CONFIG_X86_64", "CONFIG_ARM"]),
                    ConfigOption("CONFIG_ARM", "ARM (32-bit)", "n",
                                conflicts=["CONFIG_X86", "CONFIG_X86_64", "CONFIG_ARM64"]),
                    ConfigOption("CONFIG_RISCV", "RISC-V", "n",
                                conflicts=["CONFIG_X86", "CONFIG_X86_64"]),
                    ConfigOption("CONFIG_SMP", "Symmetric Multi-Processing", "y",
                                help_text="Support for multiple CPUs"),
                    ConfigOption("CONFIG_HYPER_THREADING", "Hyper-Threading (SMT)", "y",
                                depends=["CONFIG_SMP", "CONFIG_X86_64"],
                                help_text="Intel Hyper-Threading technology"),
                    ConfigOption("CONFIG_NR_CPUS", "Maximum CPUs", "64",
                                depends=["CONFIG_SMP"], option_type="int"),
                ]
            },
            "drivers": {
                "title": "Device Drivers",
                "items": [
                    ("Network Devices", "drivers_net", "submenu"),
                    ("USB Support", "drivers_usb", "submenu"),
                    ("Graphics & Display", "drivers_gpu", "submenu"),
                    ("Input Devices", "drivers_input", "submenu"),
                    ("Audio Devices", "drivers_audio", "submenu"),
                    ("Bluetooth", "drivers_bluetooth", "submenu"),
                    ("Sensors", "drivers_sensors", "submenu"),
                    ("Storage Controllers", "drivers_storage", "submenu"),
                    ("Multimedia", "drivers_multimedia", "submenu"),
                    ("Platform Specific", "drivers_platform", "submenu"),
                ]
            },
            "drivers_net": {
                "title": "Network Device Drivers",
                "items": [
                    ConfigOption("CONFIG_NETDEVICES", "Network device support", "y",
                                help_text="Enable network device drivers"),
                    ConfigOption("CONFIG_NET_ETHERNET", "Ethernet support", "y",
                                depends=["CONFIG_NETDEVICES"]),
                    ("Realtek Drivers", "drivers_net_realtek", "submenu"),
                    ("Intel Drivers", "drivers_net_intel", "submenu"),
                    ("Broadcom Drivers", "drivers_net_broadcom", "submenu"),
                    ("AMD Drivers", "drivers_net_amd", "submenu"),
                    ("Wireless Drivers", "drivers_net_wireless", "submenu"),
                ]
            },
            "drivers_net_realtek": {
                "title": "Realtek Network Drivers",
                "items": [
                    ConfigOption("CONFIG_REALTEK_8169", "RTL8169 Gigabit Ethernet", "y",
                                depends=["CONFIG_NET_ETHERNET"],
                                help_text="Realtek RTL8169 PCI Gigabit Ethernet"),
                    ConfigOption("CONFIG_REALTEK_8139", "RTL8139 Fast Ethernet", "y",
                                depends=["CONFIG_NET_ETHERNET"]),
                    ConfigOption("CONFIG_REALTEK_8168", "RTL8168 PCI-E Gigabit", "y",
                                depends=["CONFIG_NET_ETHERNET"]),
                    ConfigOption("CONFIG_REALTEK_8125", "RTL8125 2.5G Ethernet", "n",
                                depends=["CONFIG_NET_ETHERNET"]),
                ]
            },
            "drivers_gpu": {
                "title": "Graphics & Display Drivers",
                "items": [
                    ConfigOption("CONFIG_DRM", "Direct Rendering Manager", "y",
                                help_text="Modern graphics driver framework"),
                    ConfigOption("CONFIG_DRM_NVIDIA", "NVIDIA GPU support", "y",
                                depends=["CONFIG_DRM"]),
                    ConfigOption("CONFIG_DRM_NVIDIA_PROPRIETARY", "NVIDIA proprietary driver", "n",
                                depends=["CONFIG_DRM_NVIDIA"]),
                    ConfigOption("CONFIG_DRM_AMD", "AMD GPU support", "y",
                                depends=["CONFIG_DRM"]),
                    ConfigOption("CONFIG_DRM_AMD_COMPUTE", "AMD ROCm compute", "n",
                                depends=["CONFIG_DRM_AMD"]),
                    ConfigOption("CONFIG_DRM_INTEL", "Intel GPU support", "y",
                                depends=["CONFIG_DRM"]),
                    ConfigOption("CONFIG_FB", "Framebuffer support", "y"),
                    ConfigOption("CONFIG_FB_VESA", "VESA framebuffer", "y",
                                depends=["CONFIG_FB"]),
                ]
            },
            "drivers_sensors": {
                "title": "Sensor Drivers",
                "items": [
                    ConfigOption("CONFIG_SENSORS", "Sensor support", "y"),
                    ConfigOption("CONFIG_SENSORS_ACCEL", "Accelerometer", "y",
                                depends=["CONFIG_SENSORS"]),
                    ConfigOption("CONFIG_SENSORS_GYRO", "Gyroscope", "y",
                                depends=["CONFIG_SENSORS"]),
                    ConfigOption("CONFIG_SENSORS_PROXIMITY", "Proximity sensor", "y",
                                depends=["CONFIG_SENSORS"]),
                    ConfigOption("CONFIG_SENSORS_LIGHT", "Ambient light", "y",
                                depends=["CONFIG_SENSORS"]),
                    ConfigOption("CONFIG_SENSORS_FINGERPRINT", "Fingerprint reader", "n",
                                depends=["CONFIG_SENSORS"]),
                ]
            },
        }
    
    def load_config(self):
        """Load configuration from file"""
        if os.path.exists(self.config_file):
            with open(self.config_file, 'r') as f:
                for line in f:
                    line = line.strip()
                    if line and not line.startswith('#') and '=' in line:
                        key, value = line.split('=', 1)
                        self.config[key] = value
    
    def save_config(self):
        """Save configuration with validation"""
        conflicts = self.check_all_conflicts()
        if conflicts:
            self.status_message = f"Cannot save: {len(conflicts)} conflicts!"
            return False
        
        with open(self.config_file, 'w') as f:
            f.write("# Zirvium Kernel Configuration\n")
            f.write("# Generated by advanced menuconfig\n")
            f.write(f"# Architecture: {self.get_arch()}\n\n")
            
            for menu_name, menu_data in self.options.items():
                if menu_name == "main":
                    continue
                f.write(f"\n#\n# {menu_data['title']}\n#\n")
                for item in menu_data['items']:
                    if isinstance(item, ConfigOption):
                        key = item.name
                        value = self.config.get(key, item.default)
                        if item.option_type == "bool":
                            if value not in ['y', 'n', 'm']:
                                value = 'n'
                        f.write(f"{key}={value}\n")
        
        self.status_message = "Configuration saved successfully!"
        return True
    
    def get_arch(self) -> str:
        """Get selected architecture"""
        if self.config.get("CONFIG_X86_64") == "y":
            return "x86_64"
        elif self.config.get("CONFIG_ARM64") == "y":
            return "arm64"
        elif self.config.get("CONFIG_X86") == "y":
            return "x86"
        return "unknown"
    
    def check_conflicts(self, option: ConfigOption) -> List[str]:
        """Check for conflicts with current configuration"""
        conflicts = []
        for conflict in option.conflicts:
            if conflict in self.config and self.config[conflict] == 'y':
                conflicts.append(conflict)
        return conflicts
    
    def check_dependencies(self, option: ConfigOption) -> List[str]:
        """Check if dependencies are met"""
        missing = []
        for dep in option.depends:
            if dep not in self.config or self.config[dep] not in ['y', 'm']:
                missing.append(dep)
        return missing
    
    def check_all_conflicts(self) -> List[Tuple[str, str]]:
        """Check all conflicts in configuration"""
        all_conflicts = []
        for menu_data in self.options.values():
            for item in menu_data.get('items', []):
                if isinstance(item, ConfigOption):
                    if self.config.get(item.name) == 'y':
                        conflicts = self.check_conflicts(item)
                        for conf in conflicts:
                            all_conflicts.append((item.name, conf))
        return all_conflicts
    
    def toggle_option(self, option: ConfigOption):
        """Toggle option value with validation"""
        current = self.config.get(option.name, option.default)
        
        if option.option_type == "bool":
            new_value = 'n' if current == 'y' else 'y'
            
            if new_value == 'y':
                # Check dependencies
                missing_deps = self.check_dependencies(option)
                if missing_deps:
                    self.status_message = f"Missing dependencies: {', '.join(missing_deps)}"
                    return
                
                # Check conflicts
                conflicts = self.check_conflicts(option)
                if conflicts:
                    self.status_message = f"Conflicts with: {', '.join(conflicts)}"
                    return
            
            self.config[option.name] = new_value
            self.status_message = f"{option.name} = {new_value}"
    
    def draw_header(self):
        """Draw fancy header"""
        height, width = self.stdscr.getmaxyx()
        
        # Title bar
        self.stdscr.attron(curses.color_pair(1) | curses.A_BOLD)
        title = f" Zirvium Kernel Configuration v0.2.0 [{self.get_arch()}] "
        self.stdscr.addstr(0, (width - len(title)) // 2, title)
        self.stdscr.attroff(curses.color_pair(1) | curses.A_BOLD)
        
        # Menu breadcrumb
        breadcrumb = " > ".join([self.options[m]["title"] for m in 
                                [self.current_menu] if m in self.options])
        self.stdscr.attron(curses.color_pair(6))
        self.stdscr.addstr(1, 2, f"📍 {breadcrumb}"[:width-4])
        self.stdscr.attroff(curses.color_pair(6))
        
        self.stdscr.addstr(2, 0, "═" * width)
    
    def draw_footer(self):
        """Draw fancy footer with status"""
        height, width = self.stdscr.getmaxyx()
        
        # Status line
        if self.status_message:
            color = curses.color_pair(4) if "conflict" in self.status_message.lower() else curses.color_pair(5)
            self.stdscr.attron(color)
            self.stdscr.addstr(height - 3, 2, f"ℹ {self.status_message}"[:width-4])
            self.stdscr.attroff(color)
        
        self.stdscr.addstr(height - 2, 0, "─" * width)
        
        # Keybindings
        self.stdscr.attron(curses.color_pair(1))
        keys = " [Space]Toggle [Enter]Select [S]Save [Q]Quit [/]Search [H]Help [F]Filter "
        self.stdscr.addstr(height - 1, (width - len(keys)) // 2, keys[:width])
        self.stdscr.attroff(curses.color_pair(1))
    
    def draw_menu(self):
        """Draw current menu with indicators"""
        height, width = self.stdscr.getmaxyx()
        max_items = height - 6
        
        menu_items = self.options[self.current_menu]['items']
        
        for idx, item in enumerate(menu_items[self.top_row:self.top_row + max_items]):
            y = idx + 3
            
            if isinstance(item, tuple):
                # Submenu
                title, _, _ = item
                prefix = "📁 "
                if self.current_row == self.top_row + idx:
                    self.stdscr.attron(curses.color_pair(3) | curses.A_BOLD)
                    self.stdscr.addstr(y, 2, f"{prefix}{title}".ljust(width - 4))
                    self.stdscr.attroff(curses.color_pair(3) | curses.A_BOLD)
                else:
                    self.stdscr.attron(curses.color_pair(2))
                    self.stdscr.addstr(y, 2, f"{prefix}{title}")
                    self.stdscr.attroff(curses.color_pair(2))
            
            elif isinstance(item, ConfigOption):
                value = self.config.get(item.name, item.default)
                
                # Determine marker
                if item.option_type == "bool":
                    marker = "[✓]" if value == "y" else "[ ]"
                elif item.option_type == "tristate":
                    marker = "[✓]" if value == "y" else "[M]" if value == "m" else "[ ]"
                else:
                    marker = f"<{value}>"
                
                # Check for issues
                missing_deps = self.check_dependencies(item) if value == 'y' else []
                conflicts = self.check_conflicts(item) if value == 'y' else []
                
                issue_marker = ""
                if missing_deps:
                    issue_marker = " ⚠ "
                elif conflicts:
                    issue_marker = " ⚡"
                
                line = f" {marker} {item.desc}{issue_marker}"
                
                if self.current_row == self.top_row + idx:
                    self.stdscr.attron(curses.color_pair(3))
                    self.stdscr.addstr(y, 2, line[:width-4].ljust(width - 4))
                    self.stdscr.attroff(curses.color_pair(3))
                else:
                    color = curses.color_pair(7) if value != item.default else 0
                    if color:
                        self.stdscr.attron(color)
                    self.stdscr.addstr(y, 2, line[:width-4])
                    if color:
                        self.stdscr.attroff(color)
    
    def draw_help_panel(self):
        """Draw help panel for current option"""
        if not self.show_help:
            return
        
        height, width = self.stdscr.getmaxyx()
        menu_items = self.options[self.current_menu]['items']
        
        if self.current_row < len(menu_items):
            item = menu_items[self.current_row]
            if isinstance(item, ConfigOption) and item.help_text:
                # Draw help box
                help_height = min(10, height // 3)
                help_width = min(60, width - 10)
                help_y = (height - help_height) // 2
                help_x = (width - help_width) // 2
                
                # Background
                for i in range(help_height):
                    self.stdscr.addstr(help_y + i, help_x, " " * help_width, 
                                     curses.color_pair(8))
                
                # Border
                self.stdscr.addstr(help_y, help_x, "╔" + "═" * (help_width - 2) + "╗",
                                 curses.color_pair(8))
                self.stdscr.addstr(help_y + help_height - 1, help_x, 
                                 "╚" + "═" * (help_width - 2) + "╝",
                                 curses.color_pair(8))
                
                # Title
                title = f" Help: {item.name} "
                self.stdscr.addstr(help_y, help_x + (help_width - len(title)) // 2,
                                 title, curses.color_pair(8) | curses.A_BOLD)
                
                # Content
                import textwrap
                lines = textwrap.wrap(item.help_text, help_width - 4)
                for i, line in enumerate(lines[:help_height-3]):
                    self.stdscr.addstr(help_y + i + 2, help_x + 2, line,
                                     curses.color_pair(8))
                
                # Dependencies
                if item.depends:
                    dep_text = f"Depends: {', '.join(item.depends)}"
                    self.stdscr.addstr(help_y + help_height - 2, help_x + 2,
                                     dep_text[:help_width-4], curses.color_pair(8))
    
    def run(self):
        """Main event loop"""
        curses.curs_set(0)
        
        while True:
            self.stdscr.clear()
            self.draw_header()
            self.draw_menu()
            self.draw_footer()
            self.draw_help_panel()
            self.stdscr.refresh()
            
            key = self.stdscr.getch()
            
            height, width = self.stdscr.getmaxyx()
            max_items = height - 6
            menu_items = self.options[self.current_menu]['items']
            
            # Navigation
            if key == curses.KEY_UP and self.current_row > 0:
                self.current_row -= 1
                if self.current_row < self.top_row:
                    self.top_row -= 1
                self.status_message = ""
            
            elif key == curses.KEY_DOWN and self.current_row < len(menu_items) - 1:
                self.current_row += 1
                if self.current_row >= self.top_row + max_items:
                    self.top_row += 1
                self.status_message = ""
            
            elif key == ord(' '):  # Toggle
                if self.current_row < len(menu_items):
                    item = menu_items[self.current_row]
                    if isinstance(item, ConfigOption):
                        self.toggle_option(item)
            
            elif key == ord('\n'):  # Enter submenu
                if self.current_row < len(menu_items):
                    item = menu_items[self.current_row]
                    if isinstance(item, tuple) and item[2] == "submenu":
                        self.menu_stack.append((self.current_menu, self.current_row, self.top_row))
                        self.current_menu = item[1]
                        self.current_row = 0
                        self.top_row = 0
                        self.status_message = ""
            
            elif key == curses.KEY_LEFT or key == 27:  # ESC or Left - Go back
                if self.menu_stack:
                    prev_menu, prev_row, prev_top = self.menu_stack.pop()
                    self.current_menu = prev_menu
                    self.current_row = prev_row
                    self.top_row = prev_top
                    self.status_message = ""
            
            elif key == ord('s') or key == ord('S'):  # Save
                self.save_config()
            
            elif key == ord('h') or key == ord('H'):  # Help
                self.show_help = not self.show_help
            
            elif key == ord('q') or key == ord('Q'):  # Quit
                if not self.menu_stack:
                    break
                else:
                    prev_menu, prev_row, prev_top = self.menu_stack.pop()
                    self.current_menu = prev_menu
                    self.current_row = prev_row
                    self.top_row = prev_top

def main(stdscr):
    menu = MenuConfig(stdscr)
    menu.run()

if __name__ == "__main__":
    try:
        curses.wrapper(main)
    except KeyboardInterrupt:
        pass
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
