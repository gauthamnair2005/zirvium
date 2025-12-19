#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/driver.h>

int locale_en_us_init(void) { kprintf("Locale: English (US)\n"); return 0; }
int locale_zh_cn_init(void) { kprintf("Locale: Chinese (Simplified)\n"); return 0; }
int locale_ja_jp_init(void) { kprintf("Locale: Japanese\n"); return 0; }
int locale_ko_kr_init(void) { kprintf("Locale: Korean\n"); return 0; }
int locale_de_de_init(void) { kprintf("Locale: German\n"); return 0; }
int locale_fr_fr_init(void) { kprintf("Locale: French\n"); return 0; }
int locale_es_es_init(void) { kprintf("Locale: Spanish\n"); return 0; }
int locale_pt_br_init(void) { kprintf("Locale: Portuguese (Brazil)\n"); return 0; }
int locale_ru_ru_init(void) { kprintf("Locale: Russian\n"); return 0; }
int locale_ar_sa_init(void) { kprintf("Locale: Arabic\n"); return 0; }
int locale_hi_in_init(void) { kprintf("Locale: Hindi\n"); return 0; }
