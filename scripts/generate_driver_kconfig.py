#!/usr/bin/env python3
"""
Zirvium Driver Kconfig Generator
Automatically generates Kconfig entries from driver_database.json
"""
import json
import os

def load_driver_database():
    with open('drivers/driver_database.json', 'r') as f:
        return json.load(f)

def generate_driver_kconfig(db):
    """Generate comprehensive Kconfig for all drivers"""
    
    kconfig_content = """# Auto-generated driver Kconfig
# DO NOT EDIT MANUALLY - Generated from driver_database.json

menu "Zirvium Comprehensive Driver Support"

"""
    
    category_names = {
        'ethernet': 'Ethernet Controllers',
        'wifi': 'WiFi Adapters',
        'bluetooth': 'Bluetooth Controllers',
        'gpu': 'Graphics Processing Units',
        'usb_host': 'USB Host Controllers',
        'usb_storage': 'USB Storage Devices',
        'usb_hid': 'USB Human Interface Devices',
        'usb_audio': 'USB Audio Devices',
        'usb_video': 'USB Video Devices',
        'usb_network': 'USB Network Adapters',
        'usb_cdc': 'USB CDC Devices',
        'usb_hub': 'USB Hub Controllers',
        'audio': 'Audio Controllers',
        'input_keyboard': 'Keyboard Controllers',
        'input_mouse': 'Mouse Controllers',
        'input_touchpad': 'Touchpad Controllers',
        'input_gamepad': 'Gamepad Controllers',
        'sensor_accelerometer': 'Accelerometers',
        'sensor_gyroscope': 'Gyroscopes',
        'sensor_magnetometer': 'Magnetometers',
        'sensor_proximity': 'Proximity Sensors',
        'sensor_light': 'Ambient Light Sensors',
        'sensor_pressure': 'Pressure/Barometric Sensors',
        'sensor_temperature': 'Temperature Sensors',
        'sensor_humidity': 'Humidity Sensors',
        'sensor_fingerprint': 'Fingerprint Sensors',
        'sensor_heart_rate': 'Heart Rate Sensors',
        'sensor_depth': 'Depth Sensors',
        'sensor_ir_blaster': 'IR Blasters',
        'sensor_hall_effect': 'Hall Effect Sensors',
        'camera': 'Camera Controllers',
        'camera_sensor': 'Camera Sensors',
        'display': 'Display Controllers',
        'storage_sata': 'SATA Controllers',
        'storage_nvme': 'NVMe Controllers',
        'storage_scsi': 'SCSI Controllers',
        'storage_sas': 'SAS Controllers',
        'storage_sas2': 'SAS2 Controllers',
        'storage_sas3': 'SAS3 Controllers',
        'storage_mmc': 'MMC/SD Controllers',
        'storage_ide': 'IDE/PATA Controllers',
        'power_battery': 'Battery Controllers',
        'power_charger': 'Charger Controllers',
        'power_pmu': 'Power Management Units',
        'power_regulator': 'Voltage Regulators',
        'power_thermal': 'Thermal Management',
        'virtualization': 'Virtualization Drivers'
    }
    
    for category, cat_name in category_names.items():
        if category not in db['categories']:
            continue
            
        drivers = db['categories'][category]
        if not drivers:
            continue
            
        kconfig_content += f'menu "{cat_name}"\n\n'
        
        # Add master enable option
        cat_upper = category.upper()
        kconfig_content += f'''config DRIVERS_{cat_upper}
\tbool "Enable {cat_name}"
\tdefault y
\thelp
\t  Enable all {cat_name.lower()} drivers for Zirvium.

'''
        
        # Group by vendor if available
        vendors = {}
        for driver in drivers:
            vendor = driver.get('vendor', 'Other')
            if vendor not in vendors:
                vendors[vendor] = []
            vendors[vendor].append(driver)
        
        # Generate entries for each vendor
        for vendor, vend_drivers in sorted(vendors.items()):
            if len(vend_drivers) > 10:
                kconfig_content += f'menu "{vendor} {cat_name}"\n'
                kconfig_content += f'\tdepends on DRIVERS_{cat_upper}\n\n'
            
            for driver in vend_drivers[:100]:  # Limit to prevent too large files
                driver_id = driver['id']
                driver_name = driver['name']
                dtype = driver.get('type', 'tristate')
                
                kconfig_content += f'config {driver_id}\n'
                kconfig_content += f'\t{dtype} "{driver_name}"\n'
                kconfig_content += f'\tdefault m\n'
                kconfig_content += f'\tdepends on DRIVERS_{cat_upper}\n'
                
                if 'description' in driver:
                    kconfig_content += f'\thelp\n'
                    kconfig_content += f'\t  {driver["description"]}\n'
                
                kconfig_content += '\n'
            
            if len(vend_drivers) > 10:
                kconfig_content += 'endmenu\n\n'
        
        kconfig_content += 'endmenu\n\n'
    
    kconfig_content += 'endmenu\n'
    
    return kconfig_content

def main():
    db = load_driver_database()
    print(f"Generating Kconfig for {db['total_drivers']} drivers...")
    
    kconfig = generate_driver_kconfig(db)
    
    with open('drivers/Kconfig.drivers', 'w') as f:
        f.write(kconfig)
    
    print(f"Generated drivers/Kconfig.drivers")
    print(f"Total lines: {len(kconfig.splitlines())}")

if __name__ == '__main__':
    main()
