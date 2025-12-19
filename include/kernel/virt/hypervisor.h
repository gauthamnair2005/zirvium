#ifndef _HYPERVISOR_H
#define _HYPERVISOR_H

#include <kernel/types.h>

/* Hypervisor Types */
#define HYPERV_TYPE_NONE        0
#define HYPERV_TYPE_KVM         1
#define HYPERV_TYPE_VMWARE      2
#define HYPERV_TYPE_HYPERV      3  /* Microsoft Hyper-V */
#define HYPERV_TYPE_XEN         4
#define HYPERV_TYPE_VIRTUALBOX  5
#define HYPERV_TYPE_QEMU        6
#define HYPERV_TYPE_PARALLELS   7
#define HYPERV_TYPE_BHYVE       8  /* FreeBSD bhyve */
#define HYPERV_TYPE_ACRN        9  /* Intel ACRN */

/* CPUID Hypervisor signatures */
#define CPUID_HYPERVISOR_VENDOR  0x40000000

typedef struct {
    uint32_t type;
    const char *name;
    const char *signature;
    uint8_t detected;
    uint8_t paravirt_enabled;
    uint32_t features;
} hypervisor_info_t;

int hypervisor_detect(void);
int hypervisor_init(void);
uint32_t hypervisor_get_type(void);
const char* hypervisor_get_name(void);
int is_virtualized(void);

#endif /* _HYPERVISOR_H */
