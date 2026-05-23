#include "audio.h"
#include "drivers/serial/serial.h"
#include <string.h>

#define MAX_AUDIO_DRIVERS  8

static audio_driver_t *g_drivers[MAX_AUDIO_DRIVERS];
static int g_num_drivers = 0;
static audio_driver_t *g_output = NULL;

void audio_register(audio_driver_t *drv)
{
    if (g_num_drivers >= MAX_AUDIO_DRIVERS) return;
    g_drivers[g_num_drivers++] = drv;
    if (!g_output && drv->ready) {
        g_output = drv;
        serial_puts(SERIAL_COM1, "[audio] Default output: ");
        serial_puts(SERIAL_COM1, drv->name);
        serial_puts(SERIAL_COM1, "\n");
    }
}

audio_driver_t *audio_get_output(void)
{
    return g_output;
}

void audio_init(void)
{
    serial_puts(SERIAL_COM1, "[audio] Initialising audio subsystem\n");
    extern void hda_init(void);
    extern void ac97_init(void);
    hda_init();
    ac97_init();
    if (g_output) {
        serial_puts(SERIAL_COM1, "[audio] Audio subsystem ready, driver: ");
        serial_puts(SERIAL_COM1, g_output->name);
        serial_puts(SERIAL_COM1, "\n");
    } else {
        serial_puts(SERIAL_COM1, "[audio] No audio hardware found\n");
    }
}
