#ifndef ZIRVIUM_KERNEL_AUDIO_AUDIO_H
#define ZIRVIUM_KERNEL_AUDIO_AUDIO_H

#include <stdint.h>
#include <stdbool.h>

#define AUDIO_SAMPLE_RATE  48000
#define AUDIO_CHANNELS     2
#define AUDIO_BUF_FRAMES   4096

typedef struct audio_driver {
    const char *name;
    uint32_t (*write_pcm)(const void *buf, uint32_t frames);
    void  (*set_volume)(uint8_t vol);
    void  (*start)(void);
    void  (*stop)(void);
    bool   ready;
} audio_driver_t;

void audio_init(void);
void audio_register(audio_driver_t *drv);
audio_driver_t *audio_get_output(void);

#endif
