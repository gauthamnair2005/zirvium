#!/usr/bin/env python3
"""Simple text-based configuration (no curses required)"""

import sys
import os
import re

class SimpleConfig:
    def __init__(self, kconfig_file):
        self.kconfig_file = kconfig_file
        self.config = {}
        self.items = []
        self.parse_kconfig()
        
    def parse_kconfig(self):
        """Parse Kconfig file"""
        with open(self.kconfig_file, 'r') as f:
            lines = f.readlines()
        
        current_menu = None
        current_item = None
        
        for line in lines:
            stripped = line.strip()
            
            if not stripped or stripped.startswith('#'):
                continue
            
            if stripped.startswith('menu '):
                title = stripped[5:].strip('"')
                current_menu = {'type': 'menu', 'title': title, 'items': []}
                self.items.append(current_menu)
                
            elif stripped.startswith('config '):
                name = stripped.split()[1]
                current_item = {
                    'type': 'config',
                    'name': name,
                    'title': name,
                    'option_type': 'bool',
                    'default': 'n',
                    'help': ''
                }
                if current_menu:
                    current_menu['items'].append(current_item)
                else:
                    self.items.append(current_item)
                    
            elif stripped.startswith('bool '):
                if current_item:
                    current_item['option_type'] = 'bool'
                    current_item['title'] = stripped[5:].strip('"')
                    
            elif stripped.startswith('string '):
                if current_item:
                    current_item['option_type'] = 'string'
                    current_item['title'] = stripped[7:].strip('"')
                    
            elif stripped.startswith('int '):
                if current_item:
                    current_item['option_type'] = 'int'
                    current_item['title'] = stripped[4:].strip('"')
                    
            elif stripped.startswith('default '):
                if current_item:
                    current_item['default'] = stripped[8:].strip('"')
                    
            elif stripped.startswith('help'):
                if current_item:
                    current_item['help'] = 'Help available'
                    
            elif stripped == 'endmenu':
                current_menu = None
                
    def load_config(self, config_file='.config'):
        """Load existing configuration"""
        if os.path.exists(config_file):
            with open(config_file, 'r') as f:
                for line in f:
                    line = line.strip()
                    if line and not line.startswith('#'):
                        if '=' in line:
                            key, value = line.split('=', 1)
                            self.config[key] = value
                            
    def save_config(self, config_file='.config'):
        """Save configuration"""
        with open(config_file, 'w') as f:
            f.write('#\n# Automatically generated configuration\n#\n')
            for key in sorted(self.config.keys()):
                value = self.config[key]
                if value == 'n':
                    f.write(f'# {key} is not set\n')
                else:
                    f.write(f'{key}={value}\n')
                    
    def configure_menu(self, menu, level=0):
        """Configure a menu"""
        while True:
            os.system('clear')
            indent = '  ' * level
            print(f"\n{indent}=== {menu['title']} ===\n")
            
            items = menu.get('items', [])
            for i, item in enumerate(items, 1):
                if item['type'] == 'menu':
                    print(f"{indent}{i}. {item['title']} --->")
                else:
                    name = f"CONFIG_{item['name']}"
                    current = self.config.get(name, item['default'])
                    
                    if item['option_type'] == 'bool':
                        status = '[*]' if current == 'y' else '[ ]'
                        print(f"{indent}{i}. {status} {item['title']}")
                    else:
                        print(f"{indent}{i}. ({current}) {item['title']}")
                        
            print(f"\n{indent}0. Back")
            print(f"{indent}s. Save and Exit")
            print(f"{indent}q. Quit without saving")
            
            choice = input(f"\n{indent}Enter choice: ").strip().lower()
            
            if choice == '0' or choice == 'b':
                break
            elif choice == 's':
                self.save_config()
                print(f"\n{indent}Configuration saved to .config")
                input(f"{indent}Press Enter to continue...")
                break
            elif choice == 'q':
                confirm = input(f"\n{indent}Quit without saving? (y/n): ").strip().lower()
                if confirm == 'y':
                    sys.exit(0)
            elif choice.isdigit():
                idx = int(choice) - 1
                if 0 <= idx < len(items):
                    item = items[idx]
                    
                    if item['type'] == 'menu':
                        self.configure_menu(item, level + 1)
                    else:
                        name = f"CONFIG_{item['name']}"
                        current = self.config.get(name, item['default'])
                        
                        if item['option_type'] == 'bool':
                            # Toggle
                            self.config[name] = 'n' if current == 'y' else 'y'
                        elif item['option_type'] == 'string':
                            new_val = input(f"{indent}Enter value [{current}]: ").strip()
                            if new_val:
                                self.config[name] = f'"{new_val}"'
                        elif item['option_type'] == 'int':
                            new_val = input(f"{indent}Enter value [{current}]: ").strip()
                            if new_val.isdigit():
                                self.config[name] = new_val
                                
    def run(self):
        """Run configuration"""
        root_menu = {'type': 'menu', 'title': 'Main Menu', 'items': self.items}
        self.configure_menu(root_menu)

def main():
    if len(sys.argv) < 2:
        print("Usage: textconfig.py <Kconfig>")
        sys.exit(1)
        
    config = SimpleConfig(sys.argv[1])
    config.load_config()
    config.run()

if __name__ == "__main__":
    main()
