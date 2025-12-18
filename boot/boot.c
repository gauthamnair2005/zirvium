#include <stdint.h>

typedef struct {
    uint64_t Signature;
    uint32_t Revision;
    uint32_t HeaderSize;
    uint32_t CRC32;
    uint32_t Reserved;
} EFI_TABLE_HEADER;

typedef struct {
    EFI_TABLE_HEADER Hdr;
} EFI_BOOT_SERVICES;

typedef struct {
    EFI_TABLE_HEADER Hdr;
} EFI_RUNTIME_SERVICES;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    uint16_t *FirmwareVendor;
    uint32_t FirmwareRevision;
    void *ConsoleInHandle;
    void *ConIn;
    void *ConsoleOutHandle;
    void *ConOut;
    void *StandardErrorHandle;
    void *StdErr;
    EFI_RUNTIME_SERVICES *RuntimeServices;
    EFI_BOOT_SERVICES *BootServices;
    uint64_t NumberOfTableEntries;
    void *ConfigurationTable;
} EFI_SYSTEM_TABLE;

typedef struct {
    void *_pad;
    void (*OutputString)(void *This, uint16_t *String);
} SIMPLE_TEXT_OUTPUT_INTERFACE;

typedef void *EFI_HANDLE;
typedef uint64_t EFI_STATUS;

extern void kernel_entry(void);

static void print_string(EFI_SYSTEM_TABLE *SystemTable, const char *str) {
    SIMPLE_TEXT_OUTPUT_INTERFACE *ConOut = (SIMPLE_TEXT_OUTPUT_INTERFACE*)SystemTable->ConOut;
    uint16_t buffer[128];
    int i = 0;
    
    while (str[i] && i < 127) {
        buffer[i] = (uint16_t)str[i];
        i++;
    }
    buffer[i] = 0;
    
    ConOut->OutputString(ConOut, buffer);
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    (void)ImageHandle;
    
    print_string(SystemTable, L"Zirvium UEFI Bootloader\r\n");
    print_string(SystemTable, L"Loading kernel...\r\n");
    
    kernel_entry();
    
    while(1);
    return 0;
}
