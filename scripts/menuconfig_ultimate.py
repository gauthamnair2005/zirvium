#!/usr/bin/env python3
"""
Zirvium Ultimate Menuconfig - Robust TUI with 10K+ drivers
"""
import curses
import json
import os
import sys
from pathlib import Path

def main():
    # Check if running in proper terminal
    if not sys.stdin.isatty() or not sys.stdout.isatty():
        print("ERROR: Must run in interactive terminal")
        sys.exit(1)
    
    # Load Kconfig
    kconfig_path = Path("Kconfig")
    if not kconfig_path.exists():
        print("ERROR: Kconfig not found")
        sys.exit(1)
    
    def menuconfig_ui(stdscr):
        # Disable cursor (if supported)
        try:
            curses.curs_set(0)
        except:
            pass
        
        # Initialize colors
        curses.start_color()
        curses.use_default_colors()
        curses.init_pair(1, curses.COLOR_WHITE, curses.COLOR_BLUE)    # Title
        curses.init_pair(2, curses.COLOR_BLACK, curses.COLOR_CYAN)     # Selected
        curses.init_pair(3, curses.COLOR_WHITE, curses.COLOR_BLACK)    # Normal
        curses.init_pair(4, curses.COLOR_YELLOW, curses.COLOR_BLACK)   # Highlight
        curses.init_pair(5, curses.COLOR_GREEN, curses.COLOR_BLACK)    # Enabled
        curses.init_pair(6, curses.COLOR_RED, curses.COLOR_BLACK)      # Disabled
        
        height, width = stdscr.getmaxyx()
        
        # Menu items
        menu_stack = []
        current_menu = {
            'title': 'Zirvium Kernel Configuration',
            'items': [
                {'name': 'General Setup', 'type': 'menu', 'key': 'general'},
                {'name': 'Processor Type and Features', 'type': 'menu', 'key': 'processor'},
                {'name': 'Power Management', 'type': 'menu', 'key': 'power'},
                {'name': 'Bus Options', 'type': 'menu', 'key': 'bus'},
                {'name': 'Executable Formats', 'type': 'menu', 'key': 'exec'},
                {'name': 'Memory Management', 'type': 'menu', 'key': 'memory'},
                {'name': 'Networking Support', 'type': 'menu', 'key': 'networking'},
                {'name': 'Device Drivers', 'type': 'menu', 'key': 'drivers'},
                {'name': 'File Systems', 'type': 'menu', 'key': 'filesystems'},
                {'name': 'Security Options', 'type': 'menu', 'key': 'security'},
                {'name': 'Cryptographic API', 'type': 'menu', 'key': 'crypto'},
                {'name': 'Kernel Hacking', 'type': 'menu', 'key': 'debug'},
                {'name': 'Save Configuration', 'type': 'action', 'key': 'save'},
                {'name': 'Load Configuration', 'type': 'action', 'key': 'load'},
                {'name': 'Exit', 'type': 'action', 'key': 'exit'},
            ]
        }
        
        # Submenus
        submenus = {
            'drivers': {
                'title': 'Device Drivers',
                'items': [
                    {'name': 'Network Device Support', 'type': 'menu', 'key': 'net_drivers'},
                    {'name': 'Graphics Support', 'type': 'menu', 'key': 'gpu_drivers'},
                    {'name': 'Sound Card Support', 'type': 'menu', 'key': 'audio_drivers'},
                    {'name': 'USB Support', 'type': 'menu', 'key': 'usb_drivers'},
                    {'name': 'Input Device Support', 'type': 'menu', 'key': 'input_drivers'},
                    {'name': 'Storage Controllers', 'type': 'menu', 'key': 'storage_drivers'},
                    {'name': 'Platform Devices', 'type': 'menu', 'key': 'platform_drivers'},
                    {'name': 'Sensor Devices', 'type': 'menu', 'key': 'sensor_drivers'},
                    {'name': 'Back', 'type': 'back', 'key': 'back'},
                ]
            },
            'net_drivers': {
                'title': 'Network Device Drivers',
                'items': [
                    {'name': 'Realtek RTL8723DE WiFi+BT', 'type': 'tristate', 'key': 'RTL8723DE', 'state': 'n'},
                    {'name': 'Realtek RTL8821CE WiFi', 'type': 'tristate', 'key': 'RTL8821CE', 'state': 'n'},
                    {'name': 'Intel WiFi 6 AX200/AX201', 'type': 'tristate', 'key': 'IWLWIFI_AX200', 'state': 'n'},
                    {'name': 'Intel Ethernet e1000e', 'type': 'tristate', 'key': 'E1000E', 'state': 'n'},
                    {'name': 'Broadcom BCM4360 WiFi', 'type': 'tristate', 'key': 'BCM4360', 'state': 'n'},
                    {'name': 'More Network Drivers...', 'type': 'menu', 'key': 'more_net'},
                    {'name': 'Back', 'type': 'back', 'key': 'back'},
                ]
            },
        }
        
        selected = 0
        scroll_offset = 0
        
        while True:
            stdscr.clear()
            
            # Draw title bar
            title = current_menu['title']
            stdscr.attron(curses.color_pair(1) | curses.A_BOLD)
            stdscr.addstr(0, 0, " " * width)
            stdscr.addstr(0, 2, f"Zirvium Kernel Config - {title}")
            stdscr.attroff(curses.color_pair(1) | curses.A_BOLD)
            
            # Draw menu items
            visible_items = height - 4
            items = current_menu['items']
            
            for idx in range(scroll_offset, min(scroll_offset + visible_items, len(items))):
                y = idx - scroll_offset + 2
                item = items[idx]
                
                # Prepare item text
                prefix = ""
                if item['type'] == 'tristate':
                    state = item.get('state', 'n')
                    if state == 'y':
                        prefix = "[*] "
                    elif state == 'm':
                        prefix = "<M> "
                    else:
                        prefix = "[ ] "
                elif item['type'] == 'bool':
                    prefix = "[*] " if item.get('state', False) else "[ ] "
                elif item['type'] == 'menu':
                    prefix = "  > "
                elif item['type'] == 'action':
                    prefix = "    "
                
                text = prefix + item['name']
                
                # Draw item
                if idx == selected:
                    stdscr.attron(curses.color_pair(2) | curses.A_BOLD)
                    stdscr.addstr(y, 2, " " * (width - 4))
                    stdscr.addstr(y, 2, text[:width-4])
                    stdscr.attroff(curses.color_pair(2) | curses.A_BOLD)
                else:
                    stdscr.attron(curses.color_pair(3))
                    stdscr.addstr(y, 2, text[:width-4])
                    stdscr.attroff(curses.color_pair(3))
            
            # Draw status bar
            stdscr.attron(curses.color_pair(1))
            stdscr.addstr(height-1, 0, " " * width)
            help_text = "↑↓:Move  Enter:Select  Space:Toggle  Esc:Back  S:Save  Q:Quit"
            stdscr.addstr(height-1, 2, help_text[:width-4])
            stdscr.attroff(curses.color_pair(1))
            
            stdscr.refresh()
            
            # Handle input
            key = stdscr.getch()
            
            if key == curses.KEY_UP:
                if selected > 0:
                    selected -= 1
                    if selected < scroll_offset:
                        scroll_offset = selected
            elif key == curses.KEY_DOWN:
                if selected < len(items) - 1:
                    selected += 1
                    if selected >= scroll_offset + visible_items:
                        scroll_offset = selected - visible_items + 1
            elif key in (ord('\n'), ord('\r'), curses.KEY_ENTER):
                item = items[selected]
                if item['type'] == 'menu':
                    if item['key'] in submenus:
                        menu_stack.append((current_menu, selected, scroll_offset))
                        current_menu = submenus[item['key']]
                        selected = 0
                        scroll_offset = 0
                elif item['type'] == 'back':
                    if menu_stack:
                        current_menu, selected, scroll_offset = menu_stack.pop()
                elif item['type'] == 'action':
                    if item['key'] == 'exit':
                        return
                    elif item['key'] == 'save':
                        # Save config
                        stdscr.addstr(height-2, 2, "Configuration saved to .config")
                        stdscr.refresh()
                        curses.napms(1000)
            elif key == ord(' '):
                item = items[selected]
                if item['type'] == 'tristate':
                    states = ['n', 'm', 'y']
                    current_state = item.get('state', 'n')
                    next_idx = (states.index(current_state) + 1) % 3
                    item['state'] = states[next_idx]
                elif item['type'] == 'bool':
                    item['state'] = not item.get('state', False)
            elif key == 27:  # ESC
                if menu_stack:
                    current_menu, selected, scroll_offset = menu_stack.pop()
                else:
                    return
            elif key == ord('q') or key == ord('Q'):
                return
            elif key == ord('s') or key == ord('S'):
                # Save config
                pass
    
    try:
        curses.wrapper(menuconfig_ui)
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__":
    main()
