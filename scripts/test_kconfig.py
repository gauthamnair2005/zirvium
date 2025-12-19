#!/usr/bin/env python3
"""Test script to verify Kconfig parsing"""

import sys
import re

class KconfigParser:
    def __init__(self):
        self.items = []
        self.stack = []
        
    def parse(self, filename):
        with open(filename, 'r') as f:
            lines = f.readlines()
        
        current_item = None
        indent_stack = [0]
        
        for line_num, line in enumerate(lines, 1):
            stripped = line.strip()
            
            if not stripped or stripped.startswith('#'):
                continue
            
            # Count leading spaces
            indent = len(line) - len(line.lstrip())
            
            # Menu items
            if stripped.startswith('menu '):
                title = stripped[5:].strip('"')
                item = {
                    'type': 'menu',
                    'title': title,
                    'children': [],
                    'indent': indent
                }
                self.items.append(item)
                current_item = item
                print(f"Found menu: {title}")
                
            elif stripped.startswith('config '):
                name = stripped.split()[1]
                item = {
                    'type': 'config',
                    'name': name,
                    'title': name,
                    'option_type': 'bool',
                    'default': None,
                    'indent': indent
                }
                self.items.append(item)
                current_item = item
                print(f"Found config: {name}")
                
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
                    
            elif stripped == 'endmenu':
                print("Found endmenu")
                
        return self.items

def main():
    parser = KconfigParser()
    items = parser.parse('Kconfig')
    
    print(f"\n\nTotal items parsed: {len(items)}")
    print("\nFirst 20 items:")
    for i, item in enumerate(items[:20]):
        if item['type'] == 'menu':
            print(f"  [{i}] MENU: {item['title']}")
        else:
            print(f"  [{i}] CONFIG: {item['name']} - {item['title']} ({item['option_type']})")

if __name__ == "__main__":
    main()
