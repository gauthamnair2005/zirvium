#!/usr/bin/env python3
"""
Zirvium Kernel Configuration System
Professional TUI menuconfig with advanced features
"""

import curses
import json
import os
import sys
import re
from typing import Dict, List, Tuple, Any, Optional
from dataclasses import dataclass, field
from enum import Enum

class ItemType(Enum):
    MENU = "menu"
    BOOL = "bool"
    TRISTATE = "tristate"
    STRING = "string"
    INT = "int"
    HEX = "hex"
    CHOICE = "choice"

class ItemState(Enum):
    N = "n"  # Not selected
    M = "m"  # Module
    Y = "y"  # Built-in

@dataclass
class ConfigItem:
    name: str
    type: ItemType
    prompt: str
    help_text: str = ""
    default: Any = None
    depends: List[str] = field(default_factory=list)
    selects: List[str] = field(default_factory=list)
    conflicts: List[str] = field(default_factory=list)
    parent: Optional[str] = None
    children: List[str] = field(default_factory=list)
    range_min: Optional[int] = None
    range_max: Optional[int] = None
    value: Any = None
    visible: bool = True
    enabled: bool = True

class MenuConfig:
    def __init__(self, stdscr, init_curses=True):
        self.stdscr = stdscr
        self.config_items: Dict[str, ConfigItem] = {}
        self.menu_stack: List[str] = ["main"]
        self.current_pos = 0
        self.scroll_offset = 0
        self.search_term = ""
        self.show_help = False
        self.help_item = None
        self.modified = False
        self.filter_mode = None  # None, 'changed', 'enabled'
        self.init_curses = init_curses
        
        if init_curses and hasattr(curses, 'init_pair'):
            # Color pairs
            curses.init_pair(1, curses.COLOR_BLACK, curses.COLOR_CYAN)    # Title bar
            curses.init_pair(2, curses.COLOR_YELLOW, curses.COLOR_BLUE)   # Selected item
            curses.init_pair(3, curses.COLOR_WHITE, curses.COLOR_BLUE)    # Normal item
            curses.init_pair(4, curses.COLOR_BLACK, curses.COLOR_WHITE)   # Status bar
            curses.init_pair(5, curses.COLOR_GREEN, curses.COLOR_BLUE)    # Enabled option
            curses.init_pair(6, curses.COLOR_RED, curses.COLOR_BLUE)      # Disabled option
            curses.init_pair(7, curses.COLOR_CYAN, curses.COLOR_BLUE)     # Module option
            curses.init_pair(8, curses.COLOR_MAGENTA, curses.COLOR_BLUE)  # Menu/submenu
            curses.init_pair(9, curses.COLOR_YELLOW, curses.COLOR_BLACK)  # Help text
            curses.init_pair(10, curses.COLOR_WHITE, curses.COLOR_RED)    # Error/conflict
            
            curses.curs_set(0)
            self.stdscr.bkgd(' ', curses.color_pair(3))
        
    def load_config(self, kconfig_file: str):
        """Load Kconfig file"""
        try:
            with open(kconfig_file, 'r') as f:
                self._parse_kconfig(f.read())
        except Exception as e:
            self.show_error(f"Error loading config: {e}")
    
    def _parse_kconfig(self, content: str):
        """Parse Kconfig content"""
        lines = content.split('\n')
        current_item = None
        current_menu = "main"
        
        # Create root menu
        self.config_items["main"] = ConfigItem(
            name="main",
            type=ItemType.MENU,
            prompt="Zirvium Kernel Configuration",
            help_text="Main configuration menu"
        )
        
        indent_stack = [("main", -1)]
        
        i = 0
        while i < len(lines):
            line = lines[i].rstrip()
            i += 1
            
            if not line or line.strip().startswith('#'):
                continue
            
            # Calculate indentation
            indent = len(line) - len(line.lstrip())
            line = line.strip()
            
            # Handle menu hierarchy
            while indent_stack and indent <= indent_stack[-1][1]:
                indent_stack.pop()
            if indent_stack:
                current_menu = indent_stack[-1][0]
            
            # Parse different config types
            if line.startswith('menu '):
                menu_name = line[5:].strip('"')
                menu_id = f"menu_{len(self.config_items)}"
                item = ConfigItem(
                    name=menu_id,
                    type=ItemType.MENU,
                    prompt=menu_name,
                    parent=current_menu
                )
                self.config_items[menu_id] = item
                if current_menu in self.config_items:
                    self.config_items[current_menu].children.append(menu_id)
                indent_stack.append((menu_id, indent))
                current_menu = menu_id
                
            elif line.startswith('endmenu'):
                if indent_stack:
                    indent_stack.pop()
                if indent_stack:
                    current_menu = indent_stack[-1][0]
                    
            elif line.startswith('config '):
                config_name = line[7:].strip()
                current_item = ConfigItem(
                    name=config_name,
                    type=ItemType.BOOL,
                    prompt=config_name,
                    parent=current_menu
                )
                self.config_items[config_name] = current_item
                if current_menu in self.config_items:
                    self.config_items[current_menu].children.append(config_name)
                    
            elif current_item and line.startswith('bool '):
                current_item.type = ItemType.BOOL
                current_item.prompt = line[5:].strip('"')
                
            elif current_item and line.startswith('tristate '):
                current_item.type = ItemType.TRISTATE
                current_item.prompt = line[9:].strip('"')
                
            elif current_item and line.startswith('string '):
                current_item.type = ItemType.STRING
                current_item.prompt = line[7:].strip('"')
                
            elif current_item and line.startswith('int '):
                current_item.type = ItemType.INT
                current_item.prompt = line[4:].strip('"')
                
            elif current_item and line.startswith('hex '):
                current_item.type = ItemType.HEX
                current_item.prompt = line[4:].strip('"')
                
            elif current_item and line.startswith('default '):
                current_item.default = line[8:].strip('"')
                current_item.value = current_item.default
                
            elif current_item and line.startswith('depends on '):
                deps = line[11:].strip().split('&&')
                current_item.depends.extend([d.strip() for d in deps])
                
            elif current_item and line.startswith('select '):
                current_item.selects.append(line[7:].strip())
                
            elif current_item and line.startswith('conflicts '):
                current_item.conflicts.append(line[10:].strip())
                
            elif current_item and line.startswith('range '):
                parts = line[6:].split()
                if len(parts) >= 2:
                    current_item.range_min = int(parts[0])
                    current_item.range_max = int(parts[1])
                    
            elif current_item and line.startswith('help'):
                # Read help text
                help_lines = []
                while i < len(lines):
                    help_line = lines[i].rstrip()
                    if help_line and not help_line[0].isspace():
                        break
                    help_lines.append(help_line.strip())
                    i += 1
                current_item.help_text = '\n'.join(help_lines)
        
        # Initialize main menu if not exists
        if "main" not in self.config_items:
            self.config_items["main"] = ConfigItem(
                name="main",
                type=ItemType.MENU,
                prompt="Main Menu"
            )
    
    def load_saved_config(self, config_file: str):
        """Load saved configuration"""
        if not os.path.exists(config_file):
            return
        
        try:
            with open(config_file, 'r') as f:
                for line in f:
                    line = line.strip()
                    if not line or line.startswith('#'):
                        continue
                    
                    if '=' in line:
                        key, value = line.split('=', 1)
                        key = key.strip()
                        value = value.strip().strip('"')
                        
                        if key in self.config_items:
                            item = self.config_items[key]
                            if item.type == ItemType.BOOL:
                                item.value = value == 'y'
                            elif item.type == ItemType.TRISTATE:
                                item.value = value
                            elif item.type == ItemType.INT:
                                item.value = int(value)
                            elif item.type == ItemType.HEX:
                                item.value = value
                            else:
                                item.value = value
        except Exception as e:
            self.show_error(f"Error loading saved config: {e}")
    
    def save_config(self, config_file: str):
        """Save configuration"""
        try:
            with open(config_file, 'w') as f:
                f.write("# Zirvium Kernel Configuration\n")
                f.write("# Generated by menuconfig\n\n")
                
                for name, item in sorted(self.config_items.items()):
                    if item.type == ItemType.MENU:
                        continue
                    
                    if item.value is None:
                        f.write(f"# {name} is not set\n")
                    elif item.type == ItemType.BOOL:
                        if item.value:
                            f.write(f"{name}=y\n")
                        else:
                            f.write(f"# {name} is not set\n")
                    elif item.type == ItemType.TRISTATE:
                        if item.value == 'y':
                            f.write(f"{name}=y\n")
                        elif item.value == 'm':
                            f.write(f"{name}=m\n")
                        else:
                            f.write(f"# {name} is not set\n")
                    elif item.type == ItemType.STRING:
                        f.write(f'{name}="{item.value}"\n')
                    else:
                        f.write(f"{name}={item.value}\n")
            
            self.modified = False
            return True
        except Exception as e:
            self.show_error(f"Error saving config: {e}")
            return False
    
    def check_dependencies(self):
        """Check and update dependencies"""
        changed = True
        iterations = 0
        max_iterations = 10
        
        while changed and iterations < max_iterations:
            changed = False
            iterations += 1
            
            for name, item in self.config_items.items():
                if item.type == ItemType.MENU:
                    continue
                
                # Check dependencies
                old_visible = item.visible
                old_enabled = item.enabled
                
                item.visible = True
                item.enabled = True
                
                for dep in item.depends:
                    dep_item = self.config_items.get(dep)
                    if dep_item:
                        if not dep_item.value or dep_item.value == 'n':
                            item.enabled = False
                            break
                
                # Check conflicts
                for conflict in item.conflicts:
                    conflict_item = self.config_items.get(conflict)
                    if conflict_item and conflict_item.value and conflict_item.value != 'n':
                        item.enabled = False
                        break
                
                # Handle selects
                if item.value and item.value != 'n':
                    for select in item.selects:
                        select_item = self.config_items.get(select)
                        if select_item and not select_item.value:
                            select_item.value = 'y' if select_item.type == ItemType.BOOL else item.value
                            changed = True
                
                if old_visible != item.visible or old_enabled != item.enabled:
                    changed = True
    
    def get_visible_items(self) -> List[Tuple[str, ConfigItem]]:
        """Get visible items for current menu"""
        current_menu = self.menu_stack[-1]
        if current_menu not in self.config_items:
            return []
        
        menu_item = self.config_items[current_menu]
        items = []
        
        for child_name in menu_item.children:
            if child_name in self.config_items:
                child = self.config_items[child_name]
                
                # Apply filters
                if self.filter_mode == 'changed':
                    if child.value == child.default:
                        continue
                elif self.filter_mode == 'enabled':
                    if not child.enabled:
                        continue
                
                # Apply search
                if self.search_term:
                    if self.search_term.lower() not in child.prompt.lower() and \
                       self.search_term.lower() not in child.name.lower():
                        continue
                
                if child.visible:
                    items.append((child_name, child))
        
        return items
    
    def draw_title_bar(self):
        """Draw title bar"""
        height, width = self.stdscr.getmaxyx()
        title = "╔═══ Zirvium Kernel Configuration ═══╗"
        subtitle = " Professional TUI MenuConfig "
        
        try:
            self.stdscr.attron(curses.color_pair(1) | curses.A_BOLD)
            self.stdscr.addstr(0, 0, " " * width)
            self.stdscr.addstr(0, (width - len(title)) // 2, title)
            self.stdscr.addstr(1, 0, " " * width)
            self.stdscr.addstr(1, (width - len(subtitle)) // 2, subtitle)
            self.stdscr.attroff(curses.color_pair(1) | curses.A_BOLD)
        except:
            pass
    
    def draw_status_bar(self):
        """Draw status bar with instructions"""
        height, width = self.stdscr.getmaxyx()
        
        status_items = [
            "↑↓:Navigate",
            "←→:Back/Enter",
            "Space:Toggle",
            "/:Search",
            "?:Help",
            "F:Filter",
            "S:Save",
            "Q:Quit"
        ]
        
        status_line = " │ ".join(status_items)
        
        try:
            self.stdscr.attron(curses.color_pair(4) | curses.A_BOLD)
            self.stdscr.addstr(height - 2, 0, " " * width)
            self.stdscr.addstr(height - 2, 2, status_line[:width-4])
            
            # Show current path
            path = " > ".join([self.config_items[m].prompt for m in self.menu_stack if m in self.config_items])
            self.stdscr.addstr(height - 1, 0, " " * width)
            self.stdscr.addstr(height - 1, 2, f"Path: {path[:width-10]}")
            
            # Show modified indicator
            if self.modified:
                self.stdscr.addstr(height - 1, width - 12, "[MODIFIED]", curses.color_pair(10))
            
            self.stdscr.attroff(curses.color_pair(4) | curses.A_BOLD)
        except:
            pass
    
    def draw_menu(self):
        """Draw menu items"""
        height, width = self.stdscr.getmaxyx()
        visible_items = self.get_visible_items()
        
        # Calculate visible area
        menu_start = 3
        menu_height = height - 5
        
        # Adjust scroll offset
        if self.current_pos < self.scroll_offset:
            self.scroll_offset = self.current_pos
        elif self.current_pos >= self.scroll_offset + menu_height:
            self.scroll_offset = self.current_pos - menu_height + 1
        
        # Draw items
        for i in range(menu_height):
            item_index = self.scroll_offset + i
            y = menu_start + i
            
            try:
                self.stdscr.move(y, 0)
                self.stdscr.clrtoeol()
                
                if item_index >= len(visible_items):
                    continue
                
                name, item = visible_items[item_index]
                is_selected = (item_index == self.current_pos)
                
                # Determine colors
                if is_selected:
                    color = curses.color_pair(2) | curses.A_BOLD
                elif not item.enabled:
                    color = curses.color_pair(6)
                elif item.type == ItemType.MENU:
                    color = curses.color_pair(8) | curses.A_BOLD
                elif item.value and item.value != 'n':
                    if item.value == 'm':
                        color = curses.color_pair(7)
                    else:
                        color = curses.color_pair(5)
                else:
                    color = curses.color_pair(3)
                
                self.stdscr.attron(color)
                
                # Draw indicator
                indicator = "  "
                if item.type == ItemType.MENU:
                    indicator = "├─"
                elif item.type == ItemType.BOOL:
                    indicator = "[●]" if item.value else "[ ]"
                elif item.type == ItemType.TRISTATE:
                    if item.value == 'y':
                        indicator = "[*]"
                    elif item.value == 'm':
                        indicator = "[M]"
                    else:
                        indicator = "[ ]"
                elif item.type in (ItemType.STRING, ItemType.INT, ItemType.HEX):
                    indicator = f"({item.value or ''})"[:10].ljust(10)
                
                # Format line
                prompt = item.prompt[:width-20]
                line = f" {indicator} {prompt}"
                
                # Add selection marker
                if is_selected:
                    line = f"▶{line[1:]}"
                
                self.stdscr.addstr(y, 0, line.ljust(width-1))
                
                # Show depends/conflicts indicator
                if item.depends or item.conflicts:
                    info = ""
                    if item.depends:
                        info += "⊕"
                    if item.conflicts:
                        info += "⊗"
                    try:
                        self.stdscr.addstr(y, width - 5, info)
                    except:
                        pass
                
                self.stdscr.attroff(color)
            except:
                pass
        
        # Draw scrollbar
        if len(visible_items) > menu_height:
            scroll_pos = int((self.scroll_offset / len(visible_items)) * menu_height)
            scroll_size = max(1, int((menu_height / len(visible_items)) * menu_height))
            
            for i in range(menu_height):
                try:
                    if scroll_pos <= i < scroll_pos + scroll_size:
                        self.stdscr.addstr(menu_start + i, width - 1, "█", curses.color_pair(1))
                    else:
                        self.stdscr.addstr(menu_start + i, width - 1, "│", curses.color_pair(3))
                except:
                    pass
    
    def draw_help(self, item: ConfigItem):
        """Draw help window"""
        height, width = self.stdscr.getmaxyx()
        
        # Create help window
        help_height = min(height - 10, 20)
        help_width = min(width - 10, 80)
        help_y = (height - help_height) // 2
        help_x = (width - help_width) // 2
        
        help_win = curses.newwin(help_height, help_width, help_y, help_x)
        help_win.bkgd(' ', curses.color_pair(9))
        help_win.box()
        
        try:
            # Title
            title = f" Help: {item.prompt} "
            help_win.addstr(0, (help_width - len(title)) // 2, title, curses.A_BOLD)
            
            # Content
            y = 2
            help_win.addstr(y, 2, f"Symbol: {item.name}", curses.A_BOLD)
            y += 1
            help_win.addstr(y, 2, f"Type: {item.type.value}")
            y += 2
            
            if item.depends:
                help_win.addstr(y, 2, "Depends on:", curses.A_BOLD)
                y += 1
                for dep in item.depends:
                    help_win.addstr(y, 4, f"• {dep}")
                    y += 1
                y += 1
            
            if item.selects:
                help_win.addstr(y, 2, "Selects:", curses.A_BOLD)
                y += 1
                for sel in item.selects:
                    help_win.addstr(y, 4, f"• {sel}")
                    y += 1
                y += 1
            
            if item.conflicts:
                help_win.addstr(y, 2, "Conflicts with:", curses.color_pair(10) | curses.A_BOLD)
                y += 1
                for conf in item.conflicts:
                    help_win.addstr(y, 4, f"• {conf}", curses.color_pair(10))
                    y += 1
                y += 1
            
            if item.help_text:
                help_win.addstr(y, 2, "Description:", curses.A_BOLD)
                y += 1
                for line in item.help_text.split('\n'):
                    if y >= help_height - 2:
                        break
                    help_win.addstr(y, 4, line[:help_width-6])
                    y += 1
            
            help_win.addstr(help_height - 1, (help_width - 20) // 2, "Press any key to close", curses.A_BOLD)
            help_win.refresh()
            help_win.getch()
        except:
            pass
        
        del help_win
    
    def show_error(self, message: str):
        """Show error message"""
        height, width = self.stdscr.getmaxyx()
        
        msg_width = min(len(message) + 10, width - 10)
        msg_x = (width - msg_width) // 2
        msg_y = height // 2
        
        msg_win = curses.newwin(5, msg_width, msg_y, msg_x)
        msg_win.bkgd(' ', curses.color_pair(10))
        msg_win.box()
        
        try:
            msg_win.addstr(1, 2, "ERROR", curses.A_BOLD)
            msg_win.addstr(2, 2, message[:msg_width-4])
            msg_win.addstr(3, (msg_width - 16) // 2, "Press any key")
            msg_win.refresh()
            msg_win.getch()
        except:
            pass
        
        del msg_win
    
    def show_input_dialog(self, title: str, default: str = "") -> Optional[str]:
        """Show input dialog"""
        height, width = self.stdscr.getmaxyx()
        
        dialog_width = min(60, width - 10)
        dialog_x = (width - dialog_width) // 2
        dialog_y = height // 2
        
        dialog_win = curses.newwin(5, dialog_width, dialog_y, dialog_x)
        dialog_win.bkgd(' ', curses.color_pair(4))
        dialog_win.box()
        
        try:
            dialog_win.addstr(0, 2, f" {title} ", curses.A_BOLD)
            dialog_win.addstr(3, 2, "Enter: Confirm | Esc: Cancel")
            
            curses.echo()
            curses.curs_set(1)
            
            dialog_win.addstr(1, 2, " " * (dialog_width - 4))
            dialog_win.addstr(1, 2, default)
            dialog_win.refresh()
            
            input_str = dialog_win.getstr(1, 2, dialog_width - 4).decode('utf-8')
            
            curses.noecho()
            curses.curs_set(0)
            
            del dialog_win
            return input_str if input_str else default
        except:
            curses.noecho()
            curses.curs_set(0)
            del dialog_win
            return None
    
    def toggle_item(self, item: ConfigItem):
        """Toggle item value"""
        if not item.enabled:
            return
        
        if item.type == ItemType.BOOL:
            item.value = not item.value
            self.modified = True
            
        elif item.type == ItemType.TRISTATE:
            if item.value == 'n':
                item.value = 'm'
            elif item.value == 'm':
                item.value = 'y'
            else:
                item.value = 'n'
            self.modified = True
            
        elif item.type == ItemType.STRING:
            new_value = self.show_input_dialog(f"Enter value for {item.prompt}", str(item.value or ""))
            if new_value is not None:
                item.value = new_value
                self.modified = True
                
        elif item.type == ItemType.INT:
            new_value = self.show_input_dialog(f"Enter integer for {item.prompt}", str(item.value or "0"))
            if new_value is not None:
                try:
                    val = int(new_value)
                    if item.range_min is not None and val < item.range_min:
                        val = item.range_min
                    if item.range_max is not None and val > item.range_max:
                        val = item.range_max
                    item.value = val
                    self.modified = True
                except ValueError:
                    self.show_error("Invalid integer value")
                    
        elif item.type == ItemType.HEX:
            new_value = self.show_input_dialog(f"Enter hex value for {item.prompt}", str(item.value or "0x0"))
            if new_value is not None:
                try:
                    if not new_value.startswith('0x'):
                        new_value = '0x' + new_value
                    int(new_value, 16)  # Validate
                    item.value = new_value
                    self.modified = True
                except ValueError:
                    self.show_error("Invalid hex value")
        
        self.check_dependencies()
    
    def run(self):
        """Main loop"""
        while True:
            self.stdscr.clear()
            self.check_dependencies()
            
            self.draw_title_bar()
            self.draw_menu()
            self.draw_status_bar()
            
            self.stdscr.refresh()
            
            try:
                key = self.stdscr.getch()
            except KeyboardInterrupt:
                key = ord('q')
            
            visible_items = self.get_visible_items()
            
            if key == curses.KEY_UP or key == ord('k'):
                self.current_pos = max(0, self.current_pos - 1)
                
            elif key == curses.KEY_DOWN or key == ord('j'):
                self.current_pos = min(len(visible_items) - 1, self.current_pos + 1)
                
            elif key == curses.KEY_RIGHT or key == ord('\n') or key == ord('l'):
                if visible_items and self.current_pos < len(visible_items):
                    name, item = visible_items[self.current_pos]
                    if item.type == ItemType.MENU:
                        self.menu_stack.append(name)
                        self.current_pos = 0
                        self.scroll_offset = 0
                    else:
                        self.toggle_item(item)
                        
            elif key == curses.KEY_LEFT or key == ord('h'):
                if len(self.menu_stack) > 1:
                    self.menu_stack.pop()
                    self.current_pos = 0
                    self.scroll_offset = 0
                    
            elif key == ord(' '):
                if visible_items and self.current_pos < len(visible_items):
                    name, item = visible_items[self.current_pos]
                    if item.type != ItemType.MENU:
                        self.toggle_item(item)
                        
            elif key == ord('?'):
                if visible_items and self.current_pos < len(visible_items):
                    name, item = visible_items[self.current_pos]
                    self.draw_help(item)
                    
            elif key == ord('/'):
                search = self.show_input_dialog("Search", self.search_term)
                if search is not None:
                    self.search_term = search
                    self.current_pos = 0
                    self.scroll_offset = 0
                    
            elif key == ord('f') or key == ord('F'):
                # Cycle through filter modes
                if self.filter_mode is None:
                    self.filter_mode = 'changed'
                elif self.filter_mode == 'changed':
                    self.filter_mode = 'enabled'
                else:
                    self.filter_mode = None
                self.current_pos = 0
                self.scroll_offset = 0
                
            elif key == ord('s') or key == ord('S'):
                if self.save_config('.config'):
                    # Show success message briefly
                    height, width = self.stdscr.getmaxyx()
                    self.stdscr.addstr(height // 2, (width - 20) // 2, 
                                     "Configuration saved!", 
                                     curses.color_pair(5) | curses.A_BOLD)
                    self.stdscr.refresh()
                    curses.napms(1000)
                    
            elif key == ord('q') or key == ord('Q'):
                if self.modified:
                    # Ask to save
                    response = self.show_input_dialog("Save before exit? (y/n)", "y")
                    if response and response.lower() == 'y':
                        self.save_config('.config')
                break

def main():
    if len(sys.argv) < 2:
        print("Usage: menuconfig_pro.py <Kconfig> [--debug]")
        sys.exit(1)
    
    kconfig_file = sys.argv[1]
    debug_mode = "--debug" in sys.argv
    
    if debug_mode:
        # Debug mode - print configuration structure without curses
        print("Debug mode: Loading configuration...")
        
        class DummyScreen:
            def getmaxyx(self): return (24, 80)
        
        menu = MenuConfig(DummyScreen(), init_curses=False)
        menu.load_config(kconfig_file)
        
        print(f"\nLoaded {len(menu.config_items)} configuration items:")
        main_item = menu.config_items.get('main')
        if main_item:
            print(f"Main menu: '{main_item.prompt}' with {len(main_item.children)} children")
            for child_name in main_item.children[:10]:
                child = menu.config_items.get(child_name)
                if child:
                    print(f"  - {child.prompt} ({child.type.value})")
            if len(main_item.children) > 10:
                print(f"  ... and {len(main_item.children) - 10} more")
        else:
            print("ERROR: No main menu found!")
        return
    
    def run_menuconfig(stdscr):
        try:
            # Initialize curses properly
            curses.start_color()
            curses.use_default_colors()
            curses.curs_set(0)
            stdscr.keypad(True)
            stdscr.nodelay(False)
            
            menu = MenuConfig(stdscr)
            menu.load_config(kconfig_file)
            menu.load_saved_config('.config')
            menu.run()
        except Exception as e:
            curses.endwin()
            print(f"Error: {str(e)}")
            import traceback
            traceback.print_exc()
            sys.exit(1)
    
    try:
        curses.wrapper(run_menuconfig)
    except Exception as e:
        print(f"Failed to initialize curses: {e}")
        print("Try running in a proper terminal (not through pipes)")
        sys.exit(1)

if __name__ == "__main__":
    main()
