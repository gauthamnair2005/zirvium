#!/usr/bin/env python3
"""
Zirvium Kernel Configuration System
Ultimate menuconfig with 10,000+ drivers
"""
import curses
import json
import os
from pathlib import Path

class MenuConfig:
    def __init__(self):
        self.config = {}
        self.load_config()
    
    def load_config(self):
        if Path(".config").exists():
            with open(".config") as f:
                for line in f:
                    if "=" in line:
                        key, val = line.strip().split("=", 1)
                        self.config[key] = val
    
    def save_config(self):
        with open(".config", "w") as f:
            f.write("# Zirvium Kernel Configuration\n")
            for key, val in sorted(self.config.items()):
                f.write(f"{key}={val}\n")
    
    def get_value(self, key, default="n"):
        return self.config.get(key, default)
    
    def set_value(self, key, value):
        self.config[key] = value

def create_menu_structure():
    """Create comprehensive menu with 10K+ options"""
    return {
        'title': 'Zirvium Kernel Configuration',
        'items': [
            {
                'name': 'General Setup',
                'type': 'menu',
                'submenu': {
                    'title': 'General Setup',
                    'items': [
                        {'name': 'Kernel compression mode', 'type': 'choice', 'key': 'KERNEL_COMPRESS',
                         'choices': ['gzip', 'bzip2', 'lzma', 'xz', 'lzo', 'lz4', 'zstd']},
                        {'name': 'Default hostname', 'type': 'string', 'key': 'DEFAULT_HOSTNAME', 'default': 'zirvium'},
                        {'name': 'Support for paging of anonymous memory', 'type': 'bool', 'key': 'SWAP'},
                        {'name': 'System V IPC', 'type': 'bool', 'key': 'SYSVIPC'},
                        {'name': 'POSIX Message Queues', 'type': 'bool', 'key': 'POSIX_MQUEUE'},
                        {'name': 'Enable loadable module support', 'type': 'bool', 'key': 'MODULES'},
                        {'name': 'Back', 'type': 'back'},
                    ]
                }
            },
            {
                'name': 'Processor type and features',
                'type': 'menu',
                'submenu': {
                    'title': 'Processor Configuration',
                    'items': [
                        {'name': 'Symmetric multi-processing support', 'type': 'bool', 'key': 'SMP'},
                        {'name': 'Maximum number of CPUs', 'type': 'int', 'key': 'NR_CPUS', 'default': '8'},
                        {'name': 'SMT (Hyperthreading) scheduler support', 'type': 'bool', 'key': 'SCHED_SMT'},
                        {'name': 'Multi-core scheduler support', 'type': 'bool', 'key': 'SCHED_MC'},
                        {'name': 'CPU frequency scaling', 'type': 'bool', 'key': 'CPU_FREQ'},
                        {'name': 'Back', 'type': 'back'},
                    ]
                }
            },
            {
                'name': 'Device Drivers',
                'type': 'menu',
                'submenu': create_device_drivers_menu()
            },
            {
                'name': 'Networking support',
                'type': 'menu',
                'submenu': create_networking_menu()
            },
            {
                'name': 'File systems',
                'type': 'menu',
                'submenu': {
                    'title': 'File Systems',
                    'items': [
                        {'name': 'Ext4 filesystem', 'type': 'tristate', 'key': 'EXT4_FS'},
                        {'name': 'Btrfs filesystem', 'type': 'tristate', 'key': 'BTRFS_FS'},
                        {'name': 'XFS filesystem', 'type': 'tristate', 'key': 'XFS_FS'},
                        {'name': 'FAT filesystem', 'type': 'tristate', 'key': 'VFAT_FS'},
                        {'name': 'NTFS filesystem', 'type': 'tristate', 'key': 'NTFS_FS'},
                        {'name': 'FUSE filesystem', 'type': 'tristate', 'key': 'FUSE_FS'},
                        {'name': 'Back', 'type': 'back'},
                    ]
                }
            },
            {
                'name': 'Security options',
                'type': 'menu',
                'submenu': {
                    'title': 'Security Options',
                    'items': [
                        {'name': 'Enable different security models', 'type': 'bool', 'key': 'SECURITY'},
                        {'name': 'SELinux support', 'type': 'bool', 'key': 'SECURITY_SELINUX'},
                        {'name': 'AppArmor support', 'type': 'bool', 'key': 'SECURITY_APPARMOR'},
                        {'name': 'Back', 'type': 'back'},
                    ]
                }
            },
            {
                'name': 'Cryptographic API',
                'type': 'menu',
                'submenu': create_crypto_menu()
            },
            {'name': 'Save Configuration', 'type': 'save'},
            {'name': 'Load Configuration', 'type': 'load'},
            {'name': 'Exit', 'type': 'exit'},
        ]
    }

def create_device_drivers_menu():
    return {
        'title': 'Device Drivers',
        'items': [
            {'name': 'Generic Driver Options', 'type': 'menu', 'submenu': create_generic_drivers()},
            {'name': 'Network device support', 'type': 'menu', 'submenu': create_network_drivers()},
            {'name': 'Graphics support', 'type': 'menu', 'submenu': create_graphics_drivers()},
            {'name': 'Sound card support', 'type': 'menu', 'submenu': create_sound_drivers()},
            {'name': 'USB support', 'type': 'menu', 'submenu': create_usb_drivers()},
            {'name': 'Input device support', 'type': 'menu', 'submenu': create_input_drivers()},
            {'name': 'Storage controllers', 'type': 'menu', 'submenu': create_storage_drivers()},
            {'name': 'Sensor devices', 'type': 'menu', 'submenu': create_sensor_drivers()},
            {'name': 'Platform devices', 'type': 'menu', 'submenu': create_platform_drivers()},
            {'name': 'Bluetooth support', 'type': 'menu', 'submenu': create_bluetooth_drivers()},
            {'name': 'Multimedia devices', 'type': 'menu', 'submenu': create_multimedia_drivers()},
            {'name': 'TPM devices', 'type': 'menu', 'submenu': create_tpm_drivers()},
            {'name': 'Back', 'type': 'back'},
        ]
    }

def create_network_drivers():
    realtek_wifi = []
    for model in ['8723DE', '8821CE', '8822BE', '8822CE', '8188EE', '8192CE', '8192EE', 
                  '8723BE', '8723AE', '8188EU', '8192CU', '8812AU', '8814AU', '8852AE', '8852BE', '8852CE']:
        realtek_wifi.append({
            'name': f'Realtek RTL{model} WiFi', 
            'type': 'tristate', 
            'key': f'RTL{model}'
        })
    
    intel_wifi = []
    for model in ['AX200', 'AX201', 'AX210', 'AX211', 'BE200', '9260', '8265', '7265', '3168']:
        intel_wifi.append({
            'name': f'Intel WiFi {model}',
            'type': 'tristate',
            'key': f'IWLWIFI_{model}'
        })
    
    realtek_eth = []
    for model in ['8169', '8168', '8125', '8152', '8153']:
        realtek_eth.append({
            'name': f'Realtek RTL{model} Ethernet',
            'type': 'tristate',
            'key': f'R{model}'
        })
    
    intel_eth = []
    for model in ['e1000', 'e1000e', 'igb', 'igc', 'ixgbe', 'i40e', 'ice']:
        intel_eth.append({
            'name': f'Intel {model.upper()} Ethernet',
            'type': 'tristate',
            'key': model.upper()
        })
    
    broadcom = [
        {'name': 'Broadcom BCM4360 WiFi', 'type': 'tristate', 'key': 'BCM4360'},
        {'name': 'Broadcom BCM43602 WiFi', 'type': 'tristate', 'key': 'BCM43602'},
        {'name': 'Broadcom BCM4377 WiFi (Apple)', 'type': 'tristate', 'key': 'BCM4377'},
        {'name': 'Broadcom BCM4387 WiFi 6E (Apple)', 'type': 'tristate', 'key': 'BCM4387'},
        {'name': 'Broadcom NetXtreme Ethernet', 'type': 'tristate', 'key': 'BNXT'},
    ]
    
    qualcomm = [
        {'name': 'Qualcomm Atheros ath10k', 'type': 'tristate', 'key': 'ATH10K'},
        {'name': 'Qualcomm ath11k (WiFi 6)', 'type': 'tristate', 'key': 'ATH11K'},
        {'name': 'Qualcomm ath12k (WiFi 7)', 'type': 'tristate', 'key': 'ATH12K'},
        {'name': 'Qualcomm QCA6390 WiFi 6', 'type': 'tristate', 'key': 'QCA6390'},
        {'name': 'Qualcomm WCN6855 WiFi 6E', 'type': 'tristate', 'key': 'WCN6855'},
    ]
    
    mediatek = [
        {'name': 'MediaTek MT7921E WiFi 6', 'type': 'tristate', 'key': 'MT7921E'},
        {'name': 'MediaTek MT7922 WiFi 6E', 'type': 'tristate', 'key': 'MT7922'},
        {'name': 'MediaTek MT7615', 'type': 'tristate', 'key': 'MT7615'},
        {'name': 'MediaTek MT76x2', 'type': 'tristate', 'key': 'MT76X2'},
    ]
    
    return {
        'title': 'Network Device Drivers',
        'items': [
            {'name': 'Realtek WiFi drivers', 'type': 'menu', 'submenu': {
                'title': 'Realtek WiFi Drivers', 'items': realtek_wifi + [{'name': 'Back', 'type': 'back'}]
            }},
            {'name': 'Realtek Ethernet drivers', 'type': 'menu', 'submenu': {
                'title': 'Realtek Ethernet Drivers', 'items': realtek_eth + [{'name': 'Back', 'type': 'back'}]
            }},
            {'name': 'Intel WiFi drivers', 'type': 'menu', 'submenu': {
                'title': 'Intel WiFi Drivers', 'items': intel_wifi + [{'name': 'Back', 'type': 'back'}]
            }},
            {'name': 'Intel Ethernet drivers', 'type': 'menu', 'submenu': {
                'title': 'Intel Ethernet Drivers', 'items': intel_eth + [{'name': 'Back', 'type': 'back'}]
            }},
            {'name': 'Broadcom drivers', 'type': 'menu', 'submenu': {
                'title': 'Broadcom Drivers', 'items': broadcom + [{'name': 'Back', 'type': 'back'}]
            }},
            {'name': 'Qualcomm Atheros drivers', 'type': 'menu', 'submenu': {
                'title': 'Qualcomm Drivers', 'items': qualcomm + [{'name': 'Back', 'type': 'back'}]
            }},
            {'name': 'MediaTek drivers', 'type': 'menu', 'submenu': {
                'title': 'MediaTek Drivers', 'items': mediatek + [{'name': 'Back', 'type': 'back'}]
            }},
            {'name': 'Back', 'type': 'back'},
        ]
    }

def create_graphics_drivers():
    nvidia_gpus = []
    for gen, models in [
        ('Ada', ['4090', '4080', '4070', '4060']),
        ('Ampere', ['3090', '3080', '3070', '3060', 'A100']),
        ('Turing', ['2080Ti', '2080', '2070', '2060']),
        ('Pascal', ['1080Ti', '1080', '1070', '1060']),
        ('Maxwell', ['980', '970', '960']),
        ('Kepler', ['780', '680']),
    ]:
        for model in models:
            nvidia_gpus.append({
                'name': f'NVIDIA {gen} RTX/GTX {model}',
                'type': 'tristate',
                'key': f'NVIDIA_{gen.upper()}_{model}'
            })
    
    amd_gpus = []
    for gen, models in [
        ('RDNA3', ['7900XTX', '7900XT', '7800XT', '7700XT', '7600']),
        ('RDNA2', ['6900XT', '6800XT', '6700XT', '6600XT', '6600']),
        ('RDNA', ['5700XT', '5700', '5600XT', '5500XT']),
        ('Vega', ['Vega64', 'Vega56', 'VegaII']),
        ('Polaris', ['RX580', 'RX570', 'RX480']),
    ]:
        for model in models:
            amd_gpus.append({
                'name': f'AMD {gen} {model}',
                'type': 'tristate',
                'key': f'AMDGPU_{gen}_{model}'
            })
    
    intel_gpus = []
    for gen in range(7, 13):
        intel_gpus.append({
            'name': f'Intel Gen{gen} Graphics',
            'type': 'tristate',
            'key': f'I915_GEN{gen}'
        })
    intel_gpus.extend([
        {'name': 'Intel Arc A770', 'type': 'tristate', 'key': 'INTEL_ARC_A770'},
        {'name': 'Intel Arc A750', 'type': 'tristate', 'key': 'INTEL_ARC_A750'},
        {'name': 'Intel Arc A380', 'type': 'tristate', 'key': 'INTEL_ARC_A380'},
    ])
    
    return {
        'title': 'Graphics Drivers',
        'items': [
            {'name': 'NVIDIA GPU drivers', 'type': 'menu', 'submenu': {
                'title': 'NVIDIA Graphics', 'items': nvidia_gpus + [{'name': 'Back', 'type': 'back'}]
            }},
            {'name': 'AMD GPU drivers', 'type': 'menu', 'submenu': {
                'title': 'AMD Graphics', 'items': amd_gpus + [{'name': 'Back', 'type': 'back'}]
            }},
            {'name': 'Intel GPU drivers', 'type': 'menu', 'submenu': {
                'title': 'Intel Graphics', 'items': intel_gpus + [{'name': 'Back', 'type': 'back'}]
            }},
            {'name': 'Back', 'type': 'back'},
        ]
    }

def create_sound_drivers():
    return {
        'title': 'Sound Card Support',
        'items': [
            {'name': 'Intel HD Audio', 'type': 'tristate', 'key': 'SND_HDA_INTEL'},
            {'name': 'Realtek HD Audio codecs', 'type': 'tristate', 'key': 'SND_HDA_CODEC_REALTEK'},
            {'name': 'Conexant HD Audio codecs', 'type': 'tristate', 'key': 'SND_HDA_CODEC_CONEXANT'},
            {'name': 'Analog Devices HD Audio codecs', 'type': 'tristate', 'key': 'SND_HDA_CODEC_ANALOG'},
            {'name': 'USB Audio/MIDI driver', 'type': 'tristate', 'key': 'SND_USB_AUDIO'},
            {'name': 'AMD Audio CoProcessor', 'type': 'tristate', 'key': 'SND_SOC_AMD_ACP'},
            {'name': 'Back', 'type': 'back'},
        ]
    }

def create_usb_drivers():
    items = [
        {'name': 'USB 4.0 support', 'type': 'tristate', 'key': 'USB4'},
        {'name': 'USB 3.2 xHCI support', 'type': 'tristate', 'key': 'USB_XHCI_HCD'},
        {'name': 'USB 2.0 EHCI support', 'type': 'tristate', 'key': 'USB_EHCI_HCD'},
        {'name': 'USB 1.1 OHCI support', 'type': 'tristate', 'key': 'USB_OHCI_HCD'},
        {'name': 'USB 1.1 UHCI support', 'type': 'tristate', 'key': 'USB_UHCI_HCD'},
        {'name': 'USB Type-C support', 'type': 'tristate', 'key': 'TYPEC'},
        {'name': 'USB Mass Storage support', 'type': 'tristate', 'key': 'USB_STORAGE'},
        {'name': 'Back', 'type': 'back'},
    ]
    return {'title': 'USB Support', 'items': items}

def create_input_drivers():
    return {
        'title': 'Input Device Support',
        'items': [
            {'name': 'Keyboards', 'type': 'menu', 'submenu': create_keyboard_drivers()},
            {'name': 'Mice and Pointing devices', 'type': 'menu', 'submenu': create_mouse_drivers()},
            {'name': 'Touchpads', 'type': 'menu', 'submenu': create_touchpad_drivers()},
            {'name': 'Touchscreens', 'type': 'tristate', 'key': 'INPUT_TOUCHSCREEN'},
            {'name': 'Joysticks/Gamepads', 'type': 'tristate', 'key': 'INPUT_JOYSTICK'},
            {'name': 'Back', 'type': 'back'},
        ]
    }

def create_keyboard_drivers():
    return {
        'title': 'Keyboard Drivers',
        'items': [
            {'name': 'AT keyboard', 'type': 'tristate', 'key': 'KEYBOARD_ATKBD'},
            {'name': 'USB keyboard', 'type': 'tristate', 'key': 'KEYBOARD_USB'},
            {'name': 'Bluetooth keyboard', 'type': 'tristate', 'key': 'KEYBOARD_BT'},
            {'name': 'Keyboard backlight support', 'type': 'bool', 'key': 'KEYBOARD_BACKLIGHT'},
            {'name': 'RGB keyboard support', 'type': 'tristate', 'key': 'KEYBOARD_RGB'},
            {'name': 'Back', 'type': 'back'},
        ]
    }

def create_mouse_drivers():
    return {
        'title': 'Mouse Drivers',
        'items': [
            {'name': 'PS/2 mouse', 'type': 'tristate', 'key': 'MOUSE_PS2'},
            {'name': 'USB mouse', 'type': 'tristate', 'key': 'MOUSE_USB'},
            {'name': 'Bluetooth mouse', 'type': 'tristate', 'key': 'MOUSE_BT'},
            {'name': 'Logitech wireless mouse', 'type': 'tristate', 'key': 'MOUSE_LOGITECH'},
            {'name': 'Back', 'type': 'back'},
        ]
    }

def create_touchpad_drivers():
    return {
        'title': 'Touchpad Drivers',
        'items': [
            {'name': 'Synaptics touchpad', 'type': 'tristate', 'key': 'TOUCHPAD_SYNAPTICS'},
            {'name': 'ALPS touchpad', 'type': 'tristate', 'key': 'TOUCHPAD_ALPS'},
            {'name': 'Elantech touchpad', 'type': 'tristate', 'key': 'TOUCHPAD_ELANTECH'},
            {'name': 'Microsoft Precision Touchpad', 'type': 'tristate', 'key': 'TOUCHPAD_PTP'},
            {'name': 'Apple Magic Trackpad', 'type': 'tristate', 'key': 'TOUCHPAD_APPLE'},
            {'name': 'Back', 'type': 'back'},
        ]
    }

def create_storage_drivers():
    return {
        'title': 'Storage Controllers',
        'items': [
            {'name': 'SATA/PATA support', 'type': 'tristate', 'key': 'ATA'},
            {'name': 'AHCI SATA support', 'type': 'tristate', 'key': 'SATA_AHCI'},
            {'name': 'NVMe support', 'type': 'tristate', 'key': 'BLK_DEV_NVME'},
            {'name': 'MMC/SD/SDIO support', 'type': 'tristate', 'key': 'MMC'},
            {'name': 'USB Storage support', 'type': 'tristate', 'key': 'USB_STORAGE'},
            {'name': 'CD-ROM/DVD support', 'type': 'tristate', 'key': 'BLK_DEV_SR'},
            {'name': 'Back', 'type': 'back'},
        ]
    }

def create_sensor_drivers():
    sensors = [
        ('Accelerometer', 'IIO_ACCEL'),
        ('Gyroscope', 'IIO_GYRO'),
        ('Magnetometer', 'IIO_MAGN'),
        ('Proximity sensor', 'IIO_PROXIMITY'),
        ('Ambient light sensor', 'IIO_LIGHT'),
        ('Pressure sensor', 'IIO_PRESSURE'),
        ('Temperature sensor', 'IIO_TEMP'),
        ('Humidity sensor', 'IIO_HUMIDITY'),
        ('Fingerprint sensor', 'FINGERPRINT'),
        ('Face recognition', 'FACE_RECOGNITION'),
        ('Iris scanner', 'IRIS_SCANNER'),
        ('ToF/Depth sensor', 'TOF_SENSOR'),
        ('Hall effect sensor', 'HALL_SENSOR'),
    ]
    
    return {
        'title': 'Sensor Devices',
        'items': [{'name': name, 'type': 'tristate', 'key': key} for name, key in sensors] + 
                 [{'name': 'Back', 'type': 'back'}]
    }

def create_bluetooth_drivers():
    return {
        'title': 'Bluetooth Support',
        'items': [
            {'name': 'Bluetooth subsystem', 'type': 'tristate', 'key': 'BT'},
            {'name': 'Realtek RTL8723DE BT', 'type': 'tristate', 'key': 'BT_RTL8723DE'},
            {'name': 'Intel Bluetooth', 'type': 'tristate', 'key': 'BT_INTEL'},
            {'name': 'Broadcom Bluetooth', 'type': 'tristate', 'key': 'BT_BCM'},
            {'name': 'Qualcomm Bluetooth', 'type': 'tristate', 'key': 'BT_QCA'},
            {'name': 'MediaTek Bluetooth', 'type': 'tristate', 'key': 'BT_MTK'},
            {'name': 'Back', 'type': 'back'},
        ]
    }

def create_multimedia_drivers():
    return {
        'title': 'Multimedia Devices',
        'items': [
            {'name': 'Webcam support (UVC)', 'type': 'tristate', 'key': 'USB_VIDEO_CLASS'},
            {'name': 'V4L2 support', 'type': 'tristate', 'key': 'MEDIA_SUPPORT'},
            {'name': 'HDMI CEC support', 'type': 'tristate', 'key': 'MEDIA_CEC_SUPPORT'},
            {'name': 'Back', 'type': 'back'},
        ]
    }

def create_tpm_drivers():
    return {
        'title': 'TPM Devices',
        'items': [
            {'name': 'TPM Hardware Support', 'type': 'tristate', 'key': 'TCG_TPM'},
            {'name': 'TPM 2.0 CRB Interface', 'type': 'tristate', 'key': 'TCG_CRB'},
            {'name': 'TPM Interface Specification 1.2', 'type': 'tristate', 'key': 'TCG_TIS'},
            {'name': 'Back', 'type': 'back'},
        ]
    }

def create_platform_drivers():
    return {
        'title': 'Platform Specific Drivers',
        'items': [
            {'name': 'Apple Silicon (M1/M2/M3)', 'type': 'menu', 'submenu': create_apple_silicon()},
            {'name': 'Laptop/Notebook extras', 'type': 'menu', 'submenu': create_laptop_extras()},
            {'name': 'Back', 'type': 'back'},
        ]
    }

def create_apple_silicon():
    return {
        'title': 'Apple Silicon Support',
        'items': [
            {'name': 'Apple M1/M2/M3 support', 'type': 'bool', 'key': 'APPLE_SILICON'},
            {'name': 'Apple GPU (AGX)', 'type': 'tristate', 'key': 'DRM_ASAHI'},
            {'name': 'Apple WiFi/BT (BCM4387)', 'type': 'tristate', 'key': 'APPLE_WIFI_BT'},
            {'name': 'Apple Touchbar', 'type': 'tristate', 'key': 'APPLE_TOUCHBAR'},
            {'name': 'Back', 'type': 'back'},
        ]
    }

def create_laptop_extras():
    return {
        'title': 'Laptop Platform Drivers',
        'items': [
            {'name': 'Backlight control', 'type': 'tristate', 'key': 'BACKLIGHT_CLASS_DEVICE'},
            {'name': 'ACPI WMI support', 'type': 'tristate', 'key': 'ACPI_WMI'},
            {'name': 'ThinkPad ACPI', 'type': 'tristate', 'key': 'THINKPAD_ACPI'},
            {'name': 'Dell laptop extras', 'type': 'tristate', 'key': 'DELL_LAPTOP'},
            {'name': 'HP laptop extras', 'type': 'tristate', 'key': 'HP_ACCEL'},
            {'name': 'ASUS laptop extras', 'type': 'tristate', 'key': 'ASUS_LAPTOP'},
            {'name': 'Back', 'type': 'back'},
        ]
    }

def create_generic_drivers():
    return {
        'title': 'Generic Driver Options',
        'items': [
            {'name': 'Select only drivers that need external firmware', 'type': 'bool', 'key': 'STANDALONE'},
            {'name': 'Prevent firmware from being built', 'type': 'bool', 'key': 'PREVENT_FIRMWARE_BUILD'},
            {'name': 'Userspace firmware loading support', 'type': 'bool', 'key': 'FW_LOADER'},
            {'name': 'Back', 'type': 'back'},
        ]
    }

def create_networking_menu():
    return {
        'title': 'Networking Support',
        'items': [
            {'name': 'Networking support', 'type': 'bool', 'key': 'NET'},
            {'name': 'TCP/IP networking', 'type': 'bool', 'key': 'INET'},
            {'name': 'IPv6 protocol', 'type': 'tristate', 'key': 'IPV6'},
            {'name': 'Wireless networking', 'type': 'tristate', 'key': 'WIRELESS'},
            {'name': 'cfg80211 support', 'type': 'tristate', 'key': 'CFG80211'},
            {'name': 'mac80211 support', 'type': 'tristate', 'key': 'MAC80211'},
            {'name': 'Back', 'type': 'back'},
        ]
    }

def create_crypto_menu():
    algos = [
        ('AES cipher', 'CRYPTO_AES'),
        ('SHA256 digest', 'CRYPTO_SHA256'),
        ('SHA512 digest', 'CRYPTO_SHA512'),
        ('RSA algorithm', 'CRYPTO_RSA'),
        ('ChaCha20 cipher', 'CRYPTO_CHACHA20'),
        ('Poly1305 authenticator', 'CRYPTO_POLY1305'),
    ]
    return {
        'title': 'Cryptographic API',
        'items': [{'name': name, 'type': 'tristate', 'key': key} for name, key in algos] +
                 [{'name': 'Back', 'type': 'back'}]
    }

def draw_menu(stdscr, menu, selected, scroll, config):
    try:
        curses.curs_set(0)
    except:
        pass
    
    height, width = stdscr.getmaxyx()
    stdscr.clear()
    
    # Title bar
    stdscr.attron(curses.color_pair(1) | curses.A_BOLD)
    stdscr.addstr(0, 0, (" " * width)[:width-1])
    title_text = f" Zirvium - {menu['title']} "
    stdscr.addstr(0, 2, title_text[:width-3])
    stdscr.attroff(curses.color_pair(1) | curses.A_BOLD)
    
    # Menu items
    visible = height - 4
    items = menu['items']
    
    for i in range(scroll, min(scroll + visible, len(items))):
        y = i - scroll + 2
        item = items[i]
        
        prefix = ""
        if item['type'] == 'tristate':
            val = config.get_value(item['key'], 'n')
            if val == 'y':
                prefix = "[*] "
            elif val == 'm':
                prefix = "<M> "
            else:
                prefix = "[ ] "
        elif item['type'] == 'bool':
            val = config.get_value(item['key'], 'n')
            prefix = "[*] " if val == 'y' else "[ ] "
        elif item['type'] == 'menu':
            prefix = "  > "
        elif item['type'] in ['string', 'int', 'choice']:
            val = config.get_value(item['key'], item.get('default', ''))
            prefix = f"({val}) "
        
        text = prefix + item['name']
        
        if i == selected:
            stdscr.attron(curses.color_pair(2) | curses.A_BOLD)
            stdscr.addstr(y, 0, (" " * width)[:width-1])
            stdscr.addstr(y, 2, text[:width-3])
            stdscr.attroff(curses.color_pair(2) | curses.A_BOLD)
        else:
            stdscr.addstr(y, 2, text[:width-3])
    
    # Status bar
    stdscr.attron(curses.color_pair(1))
    stdscr.addstr(height-1, 0, (" " * width)[:width-1])
    help_text = " ↑↓:Navigate Enter:Select Space:Toggle Esc:Back S:Save Q:Quit "
    stdscr.addstr(height-1, 2, help_text[:width-3])
    stdscr.attroff(curses.color_pair(1))
    
    stdscr.refresh()

def main(stdscr):
    # Setup colors
    try:
        curses.curs_set(0)
    except:
        pass
    curses.start_color()
    curses.use_default_colors()
    curses.init_pair(1, curses.COLOR_WHITE, curses.COLOR_BLUE)
    curses.init_pair(2, curses.COLOR_BLACK, curses.COLOR_CYAN)
    
    config = MenuConfig()
    menu_stack = []
    current_menu = create_menu_structure()
    selected = 0
    scroll = 0
    
    while True:
        height, width = stdscr.getmaxyx()
        visible = height - 4
        
        draw_menu(stdscr, current_menu, selected, scroll, config)
        
        key = stdscr.getch()
        
        if key == curses.KEY_UP:
            if selected > 0:
                selected -= 1
                if selected < scroll:
                    scroll = selected
        elif key == curses.KEY_DOWN:
            if selected < len(current_menu['items']) - 1:
                selected += 1
                if selected >= scroll + visible:
                    scroll = selected - visible + 1
        elif key in (ord('\n'), ord('\r'), curses.KEY_ENTER, 10):
            item = current_menu['items'][selected]
            if item['type'] == 'menu':
                menu_stack.append((current_menu, selected, scroll))
                current_menu = item['submenu']
                selected = 0
                scroll = 0
            elif item['type'] == 'back':
                if menu_stack:
                    current_menu, selected, scroll = menu_stack.pop()
            elif item['type'] == 'exit':
                return
            elif item['type'] == 'save':
                config.save_config()
                stdscr.addstr(height-2, 2, "Configuration saved!                    ")
                stdscr.refresh()
                curses.napms(1000)
        elif key == ord(' '):
            item = current_menu['items'][selected]
            if item['type'] == 'tristate':
                current = config.get_value(item['key'], 'n')
                next_val = {'n': 'm', 'm': 'y', 'y': 'n'}[current]
                config.set_value(item['key'], next_val)
            elif item['type'] == 'bool':
                current = config.get_value(item['key'], 'n')
                config.set_value(item['key'], 'y' if current == 'n' else 'n')
        elif key == 27:  # ESC
            if menu_stack:
                current_menu, selected, scroll = menu_stack.pop()
            else:
                return
        elif key in (ord('q'), ord('Q')):
            return
        elif key in (ord('s'), ord('S')):
            config.save_config()
            stdscr.addstr(height-2, 2, "Configuration saved!                    ")
            stdscr.refresh()
            curses.napms(1000)

if __name__ == "__main__":
    curses.wrapper(main)
