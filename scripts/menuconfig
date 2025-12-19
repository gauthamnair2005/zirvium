#!/bin/bash
# Simple menuconfig for Zirvium

CONFIG_FILE=".config"

show_menu() {
    clear
    echo "╔════════════════════════════════════════════════════════╗"
    echo "║         Zirvium Kernel Configuration Menu            ║"
    echo "╚════════════════════════════════════════════════════════╝"
    echo ""
    source "$CONFIG_FILE" 2>/dev/null || true
    
    echo "[*] General: Compress=$CONFIG_COMPRESS_KERNEL"
    echo "[*] Processor: 64-bit=$CONFIG_X86_64 SMP=$CONFIG_SMP"
    echo "[*] Memory: PMM=$CONFIG_PHYSICAL_MEMORY_MANAGER VMM=$CONFIG_VIRTUAL_MEMORY_MANAGER"
    echo "[*] Process: Mgmt=$CONFIG_PROCESS_MANAGEMENT Max=$CONFIG_MAX_PROCESSES"
    echo "[*] FS: VFS=$CONFIG_VFS DevFS=$CONFIG_DEVFS ProcFS=$CONFIG_PROCFS"
    echo "[*] Drivers: Console=$CONFIG_CONSOLE_DRIVER Serial=$CONFIG_SERIAL_DRIVER"
    echo ""
    echo "[1] Toggle compression [2] Toggle SMP [3] Toggle DevFS"
    echo "[4] Toggle ProcFS [5] Toggle Serial [S] Save [Q] Quit"
}

toggle_option() {
    local opt=$1
    local val=$(grep "^$opt=" "$CONFIG_FILE" | cut -d= -f2)
    [ "$val" = "y" ] && sed -i "s/^$opt=y/$opt=n/" "$CONFIG_FILE" || sed -i "s/^$opt=n/$opt=y/" "$CONFIG_FILE"
}

while true; do
    show_menu
    read -p "Select: " choice
    case $choice in
        1) toggle_option "CONFIG_COMPRESS_KERNEL" ;;
        2) toggle_option "CONFIG_SMP" ;;
        3) toggle_option "CONFIG_DEVFS" ;;
        4) toggle_option "CONFIG_PROCFS" ;;
        5) toggle_option "CONFIG_SERIAL_DRIVER" ;;
        s|S) echo "Saved!" ; sleep 1 ;;
        q|Q) exit 0 ;;
    esac
done
