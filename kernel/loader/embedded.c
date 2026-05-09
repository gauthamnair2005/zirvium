#include "embedded.h"
#include <string.h>

extern char zirvinit_bin_start[], zirvinit_bin_end[];
extern char zirvshell_bin_start[], zirvshell_bin_end[];
extern char zirvutil_hello_bin_start[], zirvutil_hello_bin_end[];
extern char zirvutil_cat_bin_start[], zirvutil_cat_bin_end[];
extern char zirvutil_sysinfo_bin_start[], zirvutil_sysinfo_bin_end[];
extern char zirvutil_clear_bin_start[], zirvutil_clear_bin_end[];
extern char zirvutil_echo_bin_start[], zirvutil_echo_bin_end[];

static embedded_binary_t embedded_binaries_data[] = {
    { .path = "/bin/init",    .data = NULL, .size = 0 },
    { .path = "/bin/shell",   .data = NULL, .size = 0 },
    { .path = "/bin/hello",   .data = NULL, .size = 0 },
    { .path = "/bin/cat",     .data = NULL, .size = 0 },
    { .path = "/bin/sysinfo", .data = NULL, .size = 0 },
    { .path = "/bin/clear",   .data = NULL, .size = 0 },
    { .path = "/bin/echo",    .data = NULL, .size = 0 },
    { .path = NULL,           .data = NULL, .size = 0 },
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
}

const void *embedded_find(const char *path, size_t *size_out)
{
    if (!path) return NULL;
    for (const embedded_binary_t *e = embedded_binaries; e->path; e++) {
        if (strcmp(e->path, path) == 0) {
            if (size_out) *size_out = e->size;
            return e->data;
        }
    }
    return NULL;
}
