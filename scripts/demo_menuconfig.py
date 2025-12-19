#!/usr/bin/env python3
"""
Demo script showing menuconfig structure without requiring curses
Generates a visual representation of what the menuconfig contains
"""

import sys
import re

def parse_kconfig(filename):
    """Parse Kconfig and return hierarchical structure"""
    with open(filename, 'r') as f:
        lines = f.readlines()
    
    root = {'type': 'root', 'title': 'Main Menu', 'children': []}
    stack = [root]
    current_item = None
    
    for line in lines:
        stripped = line.strip()
        
        if not stripped or stripped.startswith('#'):
            continue
        
        if stripped.startswith('mainmenu '):
            root['title'] = stripped[9:].strip('"')
            
        elif stripped.startswith('menu '):
            title = stripped[5:].strip('"')
            menu = {
                'type': 'menu',
                'title': title,
                'children': []
            }
            stack[-1]['children'].append(menu)
            stack.append(menu)
            current_item = None
            
        elif stripped.startswith('config '):
            name = stripped.split()[1]
            item = {
                'type': 'config',
                'name': name,
                'title': name,
                'option_type': 'bool',
                'default': 'n',
                'help': ''
            }
            stack[-1]['children'].append(item)
            current_item = item
            
        elif stripped.startswith('choice'):
            choice = {
                'type': 'choice',
                'title': 'Choice',
                'children': []
            }
            stack[-1]['children'].append(choice)
            stack.append(choice)
            current_item = None
            
        elif stripped.startswith('endchoice'):
            if stack[-1]['type'] == 'choice':
                stack.pop()
                
        elif stripped.startswith('bool '):
            if current_item:
                current_item['option_type'] = 'bool'
                current_item['title'] = stripped[5:].strip('"')
                
        elif stripped.startswith('tristate '):
            if current_item:
                current_item['option_type'] = 'tristate'
                current_item['title'] = stripped[9:].strip('"')
                
        elif stripped.startswith('string '):
            if current_item:
                current_item['option_type'] = 'string'
                current_item['title'] = stripped[7:].strip('"')
                
        elif stripped.startswith('int '):
            if current_item:
                current_item['option_type'] = 'int'
                current_item['title'] = stripped[4:].strip('"')
                
        elif stripped.startswith('hex '):
            if current_item:
                current_item['option_type'] = 'hex'
                current_item['title'] = stripped[4:].strip('"')
                
        elif stripped.startswith('default '):
            if current_item:
                current_item['default'] = stripped[8:].strip('"')
                
        elif stripped.startswith('prompt '):
            if stack[-1]['type'] == 'choice':
                stack[-1]['title'] = stripped[7:].strip('"')
                
        elif stripped.startswith('help'):
            if current_item:
                current_item['has_help'] = True
                
        elif stripped == 'endmenu':
            if len(stack) > 1 and stack[-1]['type'] == 'menu':
                stack.pop()
                
    return root

def print_tree(node, indent=0, max_depth=3):
    """Print configuration tree"""
    prefix = "  " * indent
    
    if node['type'] == 'root':
        print(f"\n{'='*80}")
        print(f"  {node['title']}")
        print(f"{'='*80}\n")
        for child in node['children']:
            print_tree(child, 0, max_depth)
            
    elif node['type'] == 'menu':
        arrow = "--->" if node['children'] else ""
        print(f"{prefix}[MENU] {node['title']} {arrow}")
        if indent < max_depth:
            for child in node['children']:
                print_tree(child, indent + 1, max_depth)
        elif node['children']:
            print(f"{prefix}  ... ({len(node['children'])} items)")
            
    elif node['type'] == 'choice':
        print(f"{prefix}(choice) {node['title']}")
        if indent < max_depth:
            for child in node['children']:
                print_tree(child, indent + 1, max_depth)
                
    elif node['type'] == 'config':
        if node['option_type'] == 'bool':
            status = "[*]" if node['default'] == 'y' else "[ ]"
            print(f"{prefix}{status} {node['title']}")
        elif node['option_type'] == 'tristate':
            print(f"{prefix}<M> {node['title']}")
        elif node['option_type'] == 'string':
            print(f"{prefix}(\"{node.get('default', '')}\") {node['title']}")
        elif node['option_type'] in ['int', 'hex']:
            print(f"{prefix}({node.get('default', '0')}) {node['title']}")
        else:
            print(f"{prefix}    {node['title']}")

def print_statistics(node):
    """Print configuration statistics"""
    stats = {
        'menus': 0,
        'configs': 0,
        'bool': 0,
        'tristate': 0,
        'string': 0,
        'int': 0,
        'choices': 0
    }
    
    def count(n):
        if n['type'] == 'menu':
            stats['menus'] += 1
        elif n['type'] == 'choice':
            stats['choices'] += 1
        elif n['type'] == 'config':
            stats['configs'] += 1
            stats[n['option_type']] = stats.get(n['option_type'], 0) + 1
            
        for child in n.get('children', []):
            count(child)
            
    count(node)
    
    print(f"\n{'='*80}")
    print("  Configuration Statistics")
    print(f"{'='*80}")
    print(f"  Total Menus:        {stats['menus']}")
    print(f"  Total Choices:      {stats['choices']}")
    print(f"  Total Configs:      {stats['configs']}")
    print(f"    - Boolean:        {stats.get('bool', 0)}")
    print(f"    - Tristate:       {stats.get('tristate', 0)}")
    print(f"    - String:         {stats.get('string', 0)}")
    print(f"    - Integer:        {stats.get('int', 0)}")
    print(f"    - Hex:            {stats.get('hex', 0)}")
    print(f"{'='*80}\n")

def main():
    if len(sys.argv) < 2:
        kconfig_file = 'Kconfig'
    else:
        kconfig_file = sys.argv[1]
        
    print("\nZirvium Kernel Configuration Demo")
    print("=" * 80)
    
    try:
        root = parse_kconfig(kconfig_file)
        print_statistics(root)
        print_tree(root, max_depth=2)
        
        print("\n" + "="*80)
        print("  How to Configure")
        print("="*80)
        print("  In an interactive terminal, run:")
        print("    make menuconfig   # Full TUI interface")
        print("  Or for simple text mode:")
        print("    make config       # Simple text-based config")
        print("\n  Current configuration: .config")
        print("  See MENUCONFIG_USAGE.md for detailed instructions")
        print("="*80 + "\n")
        
    except FileNotFoundError:
        print(f"Error: {kconfig_file} not found")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__":
    main()
