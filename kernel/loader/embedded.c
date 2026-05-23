#include "embedded.h"
#include <string.h>

#ifndef VMZIRV

extern char zirvinit_bin_start[], zirvinit_bin_end[];
extern char zirvshell_bin_start[], zirvshell_bin_end[];
extern char zirvutil_hello_bin_start[], zirvutil_hello_bin_end[];
extern char zirvutil_cat_bin_start[], zirvutil_cat_bin_end[];
extern char zirvutil_sysinfo_bin_start[], zirvutil_sysinfo_bin_end[];
extern char zirvutil_clear_bin_start[], zirvutil_clear_bin_end[];
extern char zirvutil_echo_bin_start[], zirvutil_echo_bin_end[];
extern char zirvutil_reboot_bin_start[], zirvutil_reboot_bin_end[];
extern char zirvutil_shutdown_bin_start[], zirvutil_shutdown_bin_end[];
extern char zirvutil_suspend_bin_start[], zirvutil_suspend_bin_end[];
extern char zirvutil_poweroff_bin_start[], zirvutil_poweroff_bin_end[];
extern char zirvutil_ping_bin_start[], zirvutil_ping_bin_end[];
extern char zirvutil_sleep_bin_start[], zirvutil_sleep_bin_end[];
extern char zirvutil_true_bin_start[], zirvutil_true_bin_end[];
extern char zirvutil_false_bin_start[], zirvutil_false_bin_end[];
extern char zirvutil_yes_bin_start[], zirvutil_yes_bin_end[];
extern char zirvutil_uname_bin_start[], zirvutil_uname_bin_end[];
extern char zirvutil_hostname_bin_start[], zirvutil_hostname_bin_end[];
extern char zirvutil_ifconfig_bin_start[], zirvutil_ifconfig_bin_end[];
extern char zirvutil_lspci_bin_start[], zirvutil_lspci_bin_end[];
extern char zirvutil_nokia_bin_start[], zirvutil_nokia_bin_end[];
extern char zirvui_bin_start[], zirvui_bin_end[];

static embedded_binary_t embedded_binaries_data[] = {
    { .path = "/bin/init",     .data = NULL, .size = 0 },
    { .path = "/bin/shell",    .data = NULL, .size = 0 },
    { .path = "/bin/hello",    .data = NULL, .size = 0 },
    { .path = "/bin/cat",      .data = NULL, .size = 0 },
    { .path = "/bin/sysinfo",  .data = NULL, .size = 0 },
    { .path = "/bin/clear",    .data = NULL, .size = 0 },
    { .path = "/bin/echo",     .data = NULL, .size = 0 },
    { .path = "/bin/reboot",   .data = NULL, .size = 0 },
    { .path = "/bin/shutdown", .data = NULL, .size = 0 },
    { .path = "/bin/suspend",  .data = NULL, .size = 0 },
    { .path = "/bin/poweroff", .data = NULL, .size = 0 },
    { .path = "/bin/ping",     .data = NULL, .size = 0 },
    { .path = "/bin/sleep",    .data = NULL, .size = 0 },
    { .path = "/bin/true",     .data = NULL, .size = 0 },
    { .path = "/bin/false",    .data = NULL, .size = 0 },
    { .path = "/bin/yes",      .data = NULL, .size = 0 },
    { .path = "/bin/uname",    .data = NULL, .size = 0 },
    { .path = "/bin/hostname", .data = NULL, .size = 0 },
    { .path = "/bin/ifconfig", .data = NULL, .size = 0 },
    { .path = "/bin/lspci",    .data = NULL, .size = 0 },
    { .path = "/bin/nokia",    .data = NULL, .size = 0 },
    { .path = "/bin/zirvui",   .data = NULL, .size = 0 },
    { .path = NULL,            .data = NULL, .size = 0 },
};

const embedded_binary_t *embedded_binaries = embedded_binaries_data;

void embedded_init(void)
{
    embedded_binaries_data[0].data = zirvinit_bin_start;
    embedded_binaries_data[0].size = (size_t)(zirvinit_bin_end - zirvinit_bin_start);
    embedded_binaries_data[1].data = zirvshell_bin_start;
    embedded_binaries_data[1].size = (size_t)(zirvshell_bin_end - zirvshell_bin_start);
    embedded_binaries_data[2].data = zirvutil_hello_bin_start;
    embedded_binaries_data[2].size = (size_t)(zirvutil_hello_bin_end - zirvutil_hello_bin_start);
    embedded_binaries_data[3].data = zirvutil_cat_bin_start;
    embedded_binaries_data[3].size = (size_t)(zirvutil_cat_bin_end - zirvutil_cat_bin_start);
    embedded_binaries_data[4].data = zirvutil_sysinfo_bin_start;
    embedded_binaries_data[4].size = (size_t)(zirvutil_sysinfo_bin_end - zirvutil_sysinfo_bin_start);
    embedded_binaries_data[5].data = zirvutil_clear_bin_start;
    embedded_binaries_data[5].size = (size_t)(zirvutil_clear_bin_end - zirvutil_clear_bin_start);
    embedded_binaries_data[6].data = zirvutil_echo_bin_start;
    embedded_binaries_data[6].size = (size_t)(zirvutil_echo_bin_end - zirvutil_echo_bin_start);
    embedded_binaries_data[7].data = zirvutil_reboot_bin_start;
    embedded_binaries_data[7].size = (size_t)(zirvutil_reboot_bin_end - zirvutil_reboot_bin_start);
    embedded_binaries_data[8].data = zirvutil_shutdown_bin_start;
    embedded_binaries_data[8].size = (size_t)(zirvutil_shutdown_bin_end - zirvutil_shutdown_bin_start);
    embedded_binaries_data[9].data = zirvutil_suspend_bin_start;
    embedded_binaries_data[9].size = (size_t)(zirvutil_suspend_bin_end - zirvutil_suspend_bin_start);
    embedded_binaries_data[10].data = zirvutil_poweroff_bin_start;
    embedded_binaries_data[10].size = (size_t)(zirvutil_poweroff_bin_end - zirvutil_poweroff_bin_start);
    embedded_binaries_data[11].data = zirvutil_ping_bin_start;
    embedded_binaries_data[11].size = (size_t)(zirvutil_ping_bin_end - zirvutil_ping_bin_start);
    embedded_binaries_data[12].data = zirvutil_sleep_bin_start;
    embedded_binaries_data[12].size = (size_t)(zirvutil_sleep_bin_end - zirvutil_sleep_bin_start);
    embedded_binaries_data[13].data = zirvutil_true_bin_start;
    embedded_binaries_data[13].size = (size_t)(zirvutil_true_bin_end - zirvutil_true_bin_start);
    embedded_binaries_data[14].data = zirvutil_false_bin_start;
    embedded_binaries_data[14].size = (size_t)(zirvutil_false_bin_end - zirvutil_false_bin_start);
    embedded_binaries_data[15].data = zirvutil_yes_bin_start;
    embedded_binaries_data[15].size = (size_t)(zirvutil_yes_bin_end - zirvutil_yes_bin_start);
    embedded_binaries_data[16].data = zirvutil_uname_bin_start;
    embedded_binaries_data[16].size = (size_t)(zirvutil_uname_bin_end - zirvutil_uname_bin_start);
    embedded_binaries_data[17].data = zirvutil_hostname_bin_start;
    embedded_binaries_data[17].size = (size_t)(zirvutil_hostname_bin_end - zirvutil_hostname_bin_start);
    embedded_binaries_data[18].data = zirvutil_ifconfig_bin_start;
    embedded_binaries_data[18].size = (size_t)(zirvutil_ifconfig_bin_end - zirvutil_ifconfig_bin_start);
    embedded_binaries_data[19].data = zirvutil_lspci_bin_start;
    embedded_binaries_data[19].size = (size_t)(zirvutil_lspci_bin_end - zirvutil_lspci_bin_start);
    embedded_binaries_data[20].data = zirvutil_nokia_bin_start;
    embedded_binaries_data[20].size = (size_t)(zirvutil_nokia_bin_end - zirvutil_nokia_bin_start);
    embedded_binaries_data[21].data = zirvui_bin_start;
    embedded_binaries_data[21].size = (size_t)(zirvui_bin_end - zirvui_bin_start);
}

#else
/* VMZIRV mode: empty embedded binary table — no user-space utilities */

static embedded_binary_t embedded_binaries_data[] = {
    { .path = NULL, .data = NULL, .size = 0 },
};

const embedded_binary_t *embedded_binaries = embedded_binaries_data;

void embedded_init(void)
{
}

#endif

const void *embedded_find(const char *path, size_t *size_out)
{
    for (const embedded_binary_t *eb = embedded_binaries; eb->path; eb++) {
        if (strcmp(eb->path, path) == 0) {
            if (size_out) *size_out = eb->size;
            return eb->data;
        }
    }
    if (size_out) *size_out = 0;
    return NULL;
}
