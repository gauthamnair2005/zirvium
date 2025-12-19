#include <kernel/types.h>
#include <kernel/kernel.h>
#include <kernel/driver.h>

int npu_google_tpu_init(void) { kprintf("NPU: Google TPU v1/v2/v3/v4\n"); return 0; }
int npu_apple_ane_init(void) { kprintf("NPU: Apple Neural Engine (M1/M2/M3/M4)\n"); return 0; }
int npu_qualcomm_hexagon_init(void) { kprintf("NPU: Qualcomm Hexagon DSP/NPU\n"); return 0; }
int npu_samsung_npu_init(void) { kprintf("NPU: Samsung Exynos NPU\n"); return 0; }
int npu_intel_movidius_init(void) { kprintf("NPU: Intel Movidius VPU\n"); return 0; }
int npu_huawei_ascend_init(void) { kprintf("NPU: Huawei Ascend NPU\n"); return 0; }
