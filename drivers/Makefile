# Zirvium Drivers Main Makefile
# Enhanced with additional driver categories

# Core System Drivers
obj-$(CONFIG_SERIAL_8250_DRIVER) += serial/
obj-$(CONFIG_RTC_CMOS_DRIVER) += rtc/
obj-$(CONFIG_I6300ESB_WDT_DRIVER) += watchdog/
obj-$(CONFIG_SENSORS_CORETEMP_DRIVER) += hwmon/
obj-$(CONFIG_GPIO_INTEL_DRIVER) += gpio/
obj-$(CONFIG_INTEL_IDMA64_DRIVER) += dma/
obj-$(CONFIG_PCIE_CONTROLLER_DRIVER) += pcie/
obj-$(CONFIG_MDIO_BUS_DRIVER) += mdio/

# Communication Buses
obj-$(CONFIG_CAN_MCP251X_DRIVER) += can/
obj-$(CONFIG_THUNDERBOLT_DRIVER) += thunderbolt/
obj-$(CONFIG_NFC_PN533_DRIVER) += nfc/

# Network Drivers
obj-y += ethernet/
obj-y += wifi/
obj-y += bluetooth/

# Display and Graphics
obj-y += gpu/
obj-y += display/

# USB Subsystem
obj-y += usb_host/
obj-y += usb_storage/
obj-y += usb_hid/
obj-y += usb_audio/
obj-y += usb_video/
obj-y += usb_network/
obj-y += usb_cdc/
obj-y += usb_hub/

# Audio Drivers
obj-y += audio/

# Input Devices
obj-y += input_keyboard/
obj-y += input_mouse/
obj-y += input_touchpad/
obj-y += input_gamepad/

# Sensors and IIO
obj-y += sensor_accelerometer/
obj-y += sensor_gyroscope/
obj-y += sensor_magnetometer/
obj-y += sensor_proximity/
obj-y += sensor_light/
obj-y += sensor_pressure/
obj-y += sensor_temperature/
obj-y += sensor_humidity/
obj-y += sensor_fingerprint/
obj-y += sensor_heart_rate/
obj-y += sensor_depth/
obj-y += sensor_ir_blaster/
obj-y += sensor_hall_effect/

# Camera and Video
obj-y += camera/
obj-y += camera_sensor/

# Storage Controllers
obj-y += storage_sata/
obj-y += storage_nvme/
obj-y += storage_scsi/
obj-y += storage_sas/
obj-y += storage_sas2/
obj-y += storage_sas3/
obj-y += storage_mmc/
obj-y += storage_ide/

# Power Management
obj-y += power_battery/
obj-y += power_charger/
obj-y += power_pmu/
obj-y += power_regulator/
obj-y += power_thermal/

# Virtualization
obj-y += virtualization/
