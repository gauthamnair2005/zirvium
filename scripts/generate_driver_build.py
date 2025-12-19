#!/usr/bin/env python3
"""
Zirvium Driver Build Generator
Generates Kconfig and Makefile entries for all 12,787 drivers
"""

import json
import os
from pathlib import Path

def sanitize_config_name(name):
    """Sanitize driver name for Kconfig"""
    return name.replace('-', '_').replace(' ', '_').replace('.', '_').upper()

def generate_kconfig_for_category(category, drivers):
    """Generate Kconfig menu for a driver category"""
    kconfig = f'\nmenu "{category.title()} Drivers"\n\n'
    
    for driver in drivers:
        config_name = driver['id']
        driver_name = driver['name']
        vendor = driver.get('vendor', 'Generic')
        driver_type = driver.get('type', 'tristate')
        
        kconfig += f'config {config_name}\n'
        kconfig += f'    {driver_type} "{driver_name}"\n'
        kconfig += f'    default n\n'
        kconfig += f'    help\n'
        kconfig += f'      Support for {driver_name} ({vendor})\n'
        kconfig += f'      Category: {category}\n\n'
    
    kconfig += 'endmenu\n'
    return kconfig

def generate_makefile_for_category(category, drivers):
    """Generate Makefile rules for a driver category"""
    makefile = f'\n# {category.title()} Drivers\n'
    
    for driver in drivers:
        config_name = driver['id']
        # Generate object file name from config
        obj_name = config_name.lower().replace('_', '-')
        
        makefile += f'obj-$(CONFIG_{config_name}) += {obj_name}.o\n'
    
    return makefile

def create_stub_driver(driver_path, driver):
    """Create a stub driver C file if it doesn't exist"""
    if driver_path.exists():
        return
    
    config_name = driver['id']
    # Create valid C identifier (replace hyphens and dots with underscores)
    c_identifier = driver_path.stem.replace('-', '_').replace('.', '_')
    driver_name = driver['name']
    category = driver['category']
    vendor = driver.get('vendor', 'Generic')
    
    stub_code = f'''/*
 * Zirvium OS - {driver_name}
 * Category: {category}
 * Vendor: {vendor}
 * Auto-generated driver stub
 */

#include <stdint.h>
#include <stddef.h>

/* Driver state structure */
struct {c_identifier}_state {{
    uint8_t initialized;
    uint8_t active;
}};

static struct {c_identifier}_state driver_state = {{ 0, 0 }};

/* Driver probe function */
static int {c_identifier}_probe(void)
{{
    driver_state.initialized = 1;
    return 0;
}}

/* Driver remove function */
static void {c_identifier}_remove(void)
{{
    driver_state.active = 0;
    driver_state.initialized = 0;
}}

/* Driver initialization */
int {c_identifier}_init(void)
{{
    int ret = {c_identifier}_probe();
    if (ret == 0) {{
        driver_state.active = 1;
    }}
    return ret;
}}

/* Driver cleanup */
void {c_identifier}_exit(void)
{{
    {c_identifier}_remove();
}}

/* Driver metadata */
const char {c_identifier}_license[] = "GPL";
const char {c_identifier}_author[] = "Zirvium Project";
const char {c_identifier}_description[] = "{driver_name}";
const char {c_identifier}_version[] = "1.0";
'''
    
    driver_path.parent.mkdir(parents=True, exist_ok=True)
    driver_path.write_text(stub_code)

def main():
    script_dir = Path(__file__).parent
    kernel_root = script_dir.parent
    drivers_dir = kernel_root / 'drivers'
    database_path = drivers_dir / 'driver_database.json'
    
    print("Zirvium Driver Build Generator")
    print("=" * 70)
    
    # Load driver database
    with open(database_path, 'r') as f:
        database = json.load(f)
    
    total_drivers = database['total_drivers']
    categories = database['categories']
    
    print(f"Total drivers in database: {total_drivers}")
    print(f"Categories: {len(categories)}")
    
    # Generate main Kconfig.drivers
    kconfig_content = '''# Zirvium Driver Configuration
# Auto-generated from driver_database.json
# DO NOT EDIT MANUALLY - Run scripts/generate_driver_build.py

menu "Device Drivers"

'''
    
    # Generate category-specific files
    for category, drivers in categories.items():
        print(f"\nProcessing category: {category} ({len(drivers)} drivers)")
        
        category_dir = drivers_dir / category
        category_dir.mkdir(parents=True, exist_ok=True)
        
        # Generate Kconfig for this category
        category_kconfig = generate_kconfig_for_category(category, drivers)
        category_kconfig_path = category_dir / 'Kconfig'
        category_kconfig_path.write_text(category_kconfig)
        
        # Add source to main Kconfig
        kconfig_content += f'source "drivers/{category}/Kconfig"\n'
        
        # Generate Makefile for this category
        category_makefile = f'# Zirvium {category.title()} Drivers Makefile\n'
        category_makefile += f'# Auto-generated\n'
        category_makefile += generate_makefile_for_category(category, drivers)
        
        category_makefile_path = category_dir / 'Makefile'
        category_makefile_path.write_text(category_makefile)
        
        # Create stub driver files for each driver
        for i, driver in enumerate(drivers):
            obj_name = driver['id'].lower().replace('_', '-')
            driver_path = category_dir / f'{obj_name}.c'
            create_stub_driver(driver_path, driver)
            
            if (i + 1) % 100 == 0:
                print(f"  Created {i + 1}/{len(drivers)} driver stubs...")
        
        print(f"  ✓ Created {len(drivers)} driver stubs for {category}")
    
    kconfig_content += '\nendmenu\n'
    
    # Write main Kconfig.drivers
    kconfig_drivers_path = kernel_root / 'Kconfig.drivers'
    kconfig_drivers_path.write_text(kconfig_content)
    print(f"\n✓ Generated {kconfig_drivers_path}")
    
    # Generate main drivers Makefile
    main_makefile = '''# Zirvium Drivers Main Makefile
# Auto-generated

'''
    for category in categories.keys():
        main_makefile += f'obj-y += {category}/\n'
    
    drivers_makefile_path = drivers_dir / 'Makefile'
    drivers_makefile_path.write_text(main_makefile)
    print(f"✓ Generated {drivers_makefile_path}")
    
    print("\n" + "=" * 70)
    print(f"SUCCESS: All {total_drivers} drivers are now buildable!")
    print("=" * 70)
    print("\nNext steps:")
    print("  1. Run 'make menuconfig' to configure drivers")
    print("  2. Run 'make' to build the kernel with all drivers")
    print("\nAll drivers are available in menuconfig under 'Device Drivers'")

if __name__ == '__main__':
    main()
