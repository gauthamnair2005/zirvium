#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/virt/hypervisor.h>

static hypervisor_info_t hypervisor_info = {
    .type = HYPERV_TYPE_NONE,
    .name = "Bare Metal",
    .signature = "",
    .detected = 0,
    .paravirt_enabled = 0,
    .features = 0
};

static const hypervisor_info_t hypervisors[] = {
    { HYPERV_TYPE_KVM, "KVM", "KVMKVMKVM", 0, 1, 0 },
    { HYPERV_TYPE_VMWARE, "VMware", "VMwareVMware", 0, 1, 0 },
    { HYPERV_TYPE_HYPERV, "Microsoft Hyper-V", "Microsoft Hv", 0, 1, 0 },
    { HYPERV_TYPE_XEN, "Xen", "XenVMMXenVMM", 0, 1, 0 },
    { HYPERV_TYPE_VIRTUALBOX, "Oracle VirtualBox", "VBoxVBoxVBox", 0, 1, 0 },
    { HYPERV_TYPE_QEMU, "QEMU", "TCGTCGTCGTCG", 0, 1, 0 },
    { HYPERV_TYPE_PARALLELS, "Parallels", " lrpepyh vr", 0, 1, 0 },
    { HYPERV_TYPE_BHYVE, "FreeBSD bhyve", "bhyve bhyve ", 0, 1, 0 },
    { HYPERV_TYPE_ACRN, "Intel ACRN", "ACRNACRNACRN", 0, 1, 0 }
};

static void cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, 
                  uint32_t *ecx, uint32_t *edx) {
    /* Stub - would use actual CPUID instruction */
    *eax = *ebx = *ecx = *edx = 0;
}

int hypervisor_detect(void) {
    uint32_t eax, ebx, ecx, edx;
    char signature[13];
    
    kprintf("Hypervisor Detection: Scanning for virtualization...\n");
    
    /* Check CPUID leaf 0x1, ECX bit 31 (hypervisor present bit) */
    cpuid(1, &eax, &ebx, &ecx, &edx);
    
    if (!(ecx & (1U << 31))) {
        kprintf("  Running on bare metal (no hypervisor)\n");
        return 0;
    }
    
    kprintf("  Hypervisor detected! (CPUID.1.ECX[31]=1)\n");
    
    /* Get hypervisor vendor string from CPUID 0x40000000 */
    cpuid(CPUID_HYPERVISOR_VENDOR, &eax, &ebx, &ecx, &edx);
    
    /* Build signature string from EBX, ECX, EDX */
    signature[0] = (ebx >> 0) & 0xFF;
    signature[1] = (ebx >> 8) & 0xFF;
    signature[2] = (ebx >> 16) & 0xFF;
    signature[3] = (ebx >> 24) & 0xFF;
    signature[4] = (ecx >> 0) & 0xFF;
    signature[5] = (ecx >> 8) & 0xFF;
    signature[6] = (ecx >> 16) & 0xFF;
    signature[7] = (ecx >> 24) & 0xFF;
    signature[8] = (edx >> 0) & 0xFF;
    signature[9] = (edx >> 8) & 0xFF;
    signature[10] = (edx >> 16) & 0xFF;
    signature[11] = (edx >> 24) & 0xFF;
    signature[12] = '\0';
    
    /* Match against known hypervisors */
    for (int i = 0; i < sizeof(hypervisors) / sizeof(hypervisor_info_t); i++) {
        /* Simplified comparison - would use strncmp */
        hypervisor_info = hypervisors[i];
        hypervisor_info.detected = 1;
        
        kprintf("  Hypervisor: %s\n", hypervisor_info.name);
        kprintf("  Signature: %s\n", hypervisor_info.signature);
        kprintf("  Paravirtualization: %s\n", 
                hypervisor_info.paravirt_enabled ? "Available" : "Not available");
        
        return hypervisor_info.type;
    }
    
    kprintf("  Unknown hypervisor (signature: %s)\n", signature);
    return HYPERV_TYPE_NONE;
}

int is_virtualized(void) {
    return hypervisor_info.detected;
}

uint32_t hypervisor_get_type(void) {
    return hypervisor_info.type;
}

const char* hypervisor_get_name(void) {
    return hypervisor_info.name;
}
