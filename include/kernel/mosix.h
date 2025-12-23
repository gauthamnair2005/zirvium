#ifndef KERNEL_MOSIX_H
#define KERNEL_MOSIX_H

/* MOSIX File System Hierarchy Standard (FSH) */

/* Core System Paths */
#define MOSIX_ROOT      "/"
#define MOSIX_BIN       "/bin"      /* Strictly Executable Binaries */
#define MOSIX_LIB       "/lib"      /* Non-executable Libraries */
#define MOSIX_CONFIG    "/config"   /* System Configuration (replaces /etc) */
#define MOSIX_BOOT      "/boot"     /* Boot files */
#define MOSIX_HOME      "/home"     /* User home directories */
#define MOSIX_USER      "/user"     /* User specific data (non-program) */
#define MOSIX_MOUNTS    "/mounts"   /* Mount points (replaces /mnt, /media) */

/* Virtual Device Paths (Zirv Subsystem) */
#define MOSIX_ZIRV          "/zirv"
#define MOSIX_ZIRV_SATA     "/zirv/sata"
#define MOSIX_ZIRV_NVME     "/zirv/nvme"
#define MOSIX_ZIRV_USB      "/zirv/sb"      /* Simple Bus / PCIE? or just SB for Universal Serial Bus variants */
#define MOSIX_ZIRV_PCH      "/zirv/pch"
#define MOSIX_ZIRV_IDE      "/zirv/ide"

/* Device Types for /zirv structure */
typedef enum {
    ZIRV_TYPE_SATA,
    ZIRV_TYPE_NVME,
    ZIRV_TYPE_USB,
    ZIRV_TYPE_IDE,
    ZIRV_TYPE_PCH
} zirv_dev_type_t;

/* Subtypes */
typedef enum {
    ZIRV_SUBTYPE_HDD,
    ZIRV_SUBTYPE_SSD,
    ZIRV_SUBTYPE_CDROM,
    ZIRV_SUBTYPE_BATTERY,
    ZIRV_SUBTYPE_THERMAL
} zirv_dev_subtype_t;

void mosix_init(void);
int mosix_register_device(const char *name, zirv_dev_type_t type, zirv_dev_subtype_t subtype);

#endif
