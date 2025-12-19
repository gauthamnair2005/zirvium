# Nokia and Spreadtrum/UNISOC Chipset Support

## Overview

Zirvium now includes comprehensive support for Nokia smartphones and Spreadtrum/UNISOC chipsets, which power millions of budget and mid-range devices worldwide, particularly in emerging markets.

---

## Spreadtrum/UNISOC Chipsets

### Company History

**Spreadtrum Communications** (now **UNISOC**)
- Founded: 2001 in Shanghai, China
- Merged with RDA Microelectronics in 2018 → became **UNISOC**
- Focus: Budget and mid-range mobile processors
- Market: Primarily Asia, Africa, Latin America
- Annual shipments: 400+ million chips (2023)

### Chipset Architecture

Spreadtrum/UNISOC uses ARM-based designs:
- **ARM9** - Feature phones (SC6531 series)
- **ARM Cortex-A7** - Entry Android (SC7715)
- **ARM Cortex-A53** - Budget 4G smartphones (SC9820, SC9832E)
- **ARM Cortex-A55/A53** - Mid-range (SC9863A)
- **ARM Cortex-A75/A55** - Performance tier (T606/T610/T616/T618)
- **ARM Cortex-A76/A55** - 5G flagships (T700/T760/T820)

---

## Supported Chipsets (14 models)

### Feature Phone Chipsets

#### 1. Spreadtrum SC6531
- **Architecture**: ARM9
- **CPU**: Single-core @ 312 MHz
- **GPU**: None
- **Modem**: 2G (GSM/GPRS/EDGE)
- **RAM**: Up to 8 MB
- **Display**: QVGA (240×320)
- **Use cases**: Basic feature phones, ultra-budget phones

#### 2. Spreadtrum SC6531E
- **Architecture**: ARM9
- **CPU**: Single-core @ 312 MHz
- **GPU**: None
- **Modem**: 2G (GSM/GPRS/EDGE)
- **RAM**: Up to 16 MB
- **Features**: WiFi support added
- **Use cases**: Enhanced feature phones, KaiOS devices

---

### Android Smartphone Chipsets

#### 3. Spreadtrum SC7715
- **Architecture**: ARM Cortex-A7
- **CPU**: Single-core @ 1.0 GHz
- **GPU**: ARM Mali-400
- **Modem**: 3G (HSPA+)
- **RAM**: Up to 512 MB
- **Camera**: Up to 5 MP
- **Display**: FWVGA (480×854)
- **Use cases**: Entry-level Android smartphones

#### 4. Spreadtrum SC9820
- **Architecture**: ARM Cortex-A53
- **CPU**: Dual-core @ 1.2 GHz
- **GPU**: ARM Mali-T820 MP1
- **Modem**: 4G LTE Cat.4
- **RAM**: Up to 1 GB
- **Camera**: Up to 8 MP
- **Display**: HD (720×1280)
- **Process**: 28nm
- **Use cases**: Budget 4G smartphones

#### 5. Spreadtrum SC9832E (Popular)
- **Architecture**: ARM Cortex-A53
- **CPU**: Quad-core @ 1.4 GHz
- **GPU**: ARM Mali-T820 MP1
- **Modem**: 4G LTE Cat.4
- **RAM**: Up to 2 GB
- **Camera**: Up to 13 MP
- **Display**: HD+ (720×1440)
- **Process**: 28nm
- **Devices**: Nokia C1, C2, C5 Endi
- **Use cases**: Budget smartphones, emerging markets

#### 6. Spreadtrum SC9863A (Most Popular)
- **Architecture**: ARM Cortex-A55 (4×1.6 GHz) + Cortex-A53 (4×1.2 GHz)
- **CPU**: Octa-core
- **GPU**: PowerVR GE8322
- **Modem**: 4G LTE Cat.7 (DL 300 Mbps)
- **RAM**: Up to 6 GB
- **Camera**: Up to 16 MP
- **Display**: FHD+ (1080×2340)
- **Process**: 28nm
- **Features**: Dual 4G SIM, AI camera
- **Devices**: Nokia C3, C20, C21, C30, C31
- **Use cases**: Mid-budget smartphones, good battery life

---

### UNISOC Rebranded Chipsets

#### 7. UNISOC T310
- **Architecture**: ARM Cortex-A75 (1×2.0 GHz) + Cortex-A55 (3×1.8 GHz)
- **CPU**: Quad-core
- **GPU**: PowerVR GE8300
- **Modem**: 4G LTE Cat.4
- **RAM**: Up to 4 GB
- **Process**: 22nm
- **Use cases**: Entry 4G smartphones

#### 8. UNISOC T606 (Budget King)
- **Architecture**: ARM Cortex-A75 (2×1.6 GHz) + Cortex-A55 (6×1.6 GHz)
- **CPU**: Octa-core
- **GPU**: ARM Mali-G57 MP1
- **Modem**: 4G LTE Cat.7
- **RAM**: Up to 8 GB
- **Camera**: Up to 48 MP
- **Display**: FHD+ (1080×2400)
- **Process**: 12nm
- **Features**: AI acceleration, triple ISP
- **Devices**: Nokia G21
- **AnTuTu Score**: ~210,000
- **Use cases**: Budget gaming, daily driver

#### 9. UNISOC T610
- **Architecture**: ARM Cortex-A75 (2×1.8 GHz) + Cortex-A55 (6×1.8 GHz)
- **CPU**: Octa-core
- **GPU**: ARM Mali-G52 MP2
- **Modem**: 4G LTE Cat.7
- **RAM**: Up to 8 GB
- **Camera**: Up to 64 MP
- **Process**: 12nm
- **AnTuTu Score**: ~240,000
- **Use cases**: Mid-range smartphones

#### 10. UNISOC T616
- **Architecture**: ARM Cortex-A75 (2×2.0 GHz) + Cortex-A55 (6×1.8 GHz)
- **CPU**: Octa-core
- **GPU**: ARM Mali-G57 MC1
- **Modem**: 4G LTE Cat.7
- **RAM**: Up to 8 GB
- **Camera**: Up to 64 MP
- **Display**: FHD+ (1080×2520)
- **Process**: 12nm
- **Features**: Dual-frequency GPS (L1+L5)
- **AnTuTu Score**: ~250,000
- **Use cases**: Performance budget phones

#### 11. UNISOC T618 (Gaming)
- **Architecture**: ARM Cortex-A75 (2×2.0 GHz) + Cortex-A55 (6×2.0 GHz)
- **CPU**: Octa-core
- **GPU**: ARM Mali-G52 MP2
- **Modem**: 4G LTE Cat.7
- **RAM**: Up to 8 GB
- **Camera**: Up to 64 MP
- **Process**: 12nm
- **Features**: HyperEngine game optimization
- **AnTuTu Score**: ~260,000
- **Use cases**: Budget gaming phones

---

### 5G Chipsets

#### 12. UNISOC T700
- **Architecture**: ARM Cortex-A76 (2×1.8 GHz) + Cortex-A55 (6×1.8 GHz)
- **CPU**: Octa-core
- **GPU**: ARM Mali-G52 MP2
- **Modem**: 5G (Sub-6 GHz)
- **RAM**: Up to 12 GB
- **Camera**: Up to 108 MP
- **Process**: 6nm
- **5G**: SA/NSA, 2.3 Gbps DL
- **Use cases**: Entry 5G smartphones

#### 13. UNISOC T760
- **Architecture**: ARM Cortex-A76 (4×2.2 GHz) + Cortex-A55 (4×2.0 GHz)
- **CPU**: Octa-core
- **GPU**: ARM Mali-G57 MC4
- **Modem**: 5G (Sub-6 GHz)
- **RAM**: Up to 16 GB
- **Camera**: Up to 108 MP
- **Process**: 6nm
- **AnTuTu Score**: ~450,000
- **Use cases**: Mid-range 5G phones

#### 14. UNISOC T820
- **Architecture**: ARM Cortex-A76 (1×2.7 GHz + 3×2.3 GHz) + Cortex-A55 (4×2.1 GHz)
- **CPU**: Octa-core
- **GPU**: ARM Mali-G57 MC6
- **Modem**: 5G (Sub-6 GHz + mmWave)
- **RAM**: Up to 16 GB
- **Camera**: Up to 200 MP
- **Display**: QHD+ (1440×3200)
- **Process**: 6nm
- **AnTuTu Score**: ~500,000
- **Use cases**: Flagship 5G smartphones

---

## Nokia Smartphone Series

### Nokia C-Series (Budget)

#### Nokia C1
- **Chipset**: Spreadtrum SC9832E
- **Year**: 2020
- **RAM**: 1 GB
- **Storage**: 16 GB
- **Battery**: 2500 mAh
- **Display**: 5.4" FWVGA
- **Camera**: 5 MP (rear), 5 MP (front)
- **OS**: Android 9.0 Go Edition
- **Price**: ~$70

#### Nokia C2
- **Chipset**: Spreadtrum SC9832E
- **Year**: 2020
- **RAM**: 1 GB
- **Storage**: 16 GB
- **Battery**: 2800 mAh
- **Display**: 5.7" HD+
- **Camera**: 5 MP (rear), 5 MP (front)
- **OS**: Android 9.0 Go Edition
- **Price**: ~$80

#### Nokia C3 (Popular)
- **Chipset**: UNISOC SC9863A
- **Year**: 2020
- **RAM**: 3 GB
- **Storage**: 32 GB
- **Battery**: 3040 mAh
- **Display**: 5.99" HD+
- **Camera**: 8 MP (rear), 5 MP (front)
- **OS**: Android 10
- **Features**: Rear fingerprint, dedicated Google Assistant button
- **Price**: ~$110
- **Target**: Emerging markets, budget users

#### Nokia C5 Endi
- **Chipset**: Spreadtrum SC9832E
- **Year**: 2019
- **RAM**: 2 GB
- **Storage**: 16 GB
- **Battery**: 3000 mAh
- **Display**: 5.9" HD+
- **Camera**: 8 MP (rear), 5 MP (front)
- **OS**: Android 9.0
- **Price**: ~$95

#### Nokia C10
- **Chipset**: UNISOC SC7731E
- **Year**: 2021
- **RAM**: 1 GB / 2 GB
- **Storage**: 16 GB / 32 GB
- **Battery**: 3000 mAh
- **Display**: 6.5" HD+
- **Camera**: 5 MP (rear), 5 MP (front)
- **OS**: Android 11 Go Edition
- **Price**: ~$75

#### Nokia C20
- **Chipset**: UNISOC SC9863A
- **Year**: 2021
- **RAM**: 2 GB
- **Storage**: 32 GB
- **Battery**: 3000 mAh
- **Display**: 6.5" HD+
- **Camera**: 5 MP (rear), 5 MP (front)
- **OS**: Android 11 Go Edition
- **Price**: ~$90

#### Nokia C21
- **Chipset**: UNISOC SC9863A
- **Year**: 2022
- **RAM**: 3 GB
- **Storage**: 32 GB / 64 GB
- **Battery**: 3000 mAh
- **Display**: 6.5" HD+
- **Camera**: 8 MP (rear), 5 MP (front)
- **OS**: Android 11 Go Edition
- **Features**: Rear fingerprint sensor
- **Price**: ~$100

#### Nokia C30
- **Chipset**: UNISOC SC9863A
- **Year**: 2021
- **RAM**: 3 GB
- **Storage**: 64 GB
- **Battery**: 6000 mAh (huge!)
- **Display**: 6.82" HD+
- **Camera**: 13 MP + 2 MP (rear), 5 MP (front)
- **OS**: Android 11
- **Features**: 2-day battery life, fingerprint
- **Price**: ~$130

#### Nokia C31
- **Chipset**: UNISOC SC9863A
- **Year**: 2022
- **RAM**: 3 GB / 4 GB
- **Storage**: 32 GB / 64 GB
- **Battery**: 5050 mAh
- **Display**: 6.75" HD+
- **Camera**: 13 MP (rear), 5 MP (front)
- **OS**: Android 12
- **Features**: Side fingerprint, fast charging
- **Price**: ~$120

---

### Nokia G-Series (Mid-range)

#### Nokia G10
- **Chipset**: MediaTek Helio G25
- **Year**: 2021
- **RAM**: 3 GB / 4 GB
- **Storage**: 32 GB / 64 GB
- **Battery**: 5050 mAh
- **Display**: 6.5" HD+
- **Camera**: 13 MP + 2 MP + 2 MP (rear), 8 MP (front)
- **OS**: Android 11
- **Features**: 3-day battery, rear fingerprint
- **Price**: ~$150

#### Nokia G20
- **Chipset**: MediaTek Helio G35
- **Year**: 2021
- **RAM**: 4 GB
- **Storage**: 64 GB / 128 GB
- **Battery**: 5050 mAh
- **Display**: 6.5" HD+ 90Hz
- **Camera**: 48 MP + 5 MP + 2 MP + 2 MP (rear), 8 MP (front)
- **OS**: Android 11
- **Features**: 3 years OS updates, dedicated Google Assistant button
- **Price**: ~$200

#### Nokia G21
- **Chipset**: UNISOC T606
- **Year**: 2022
- **RAM**: 4 GB / 6 GB
- **Storage**: 64 GB / 128 GB
- **Battery**: 5050 mAh
- **Display**: 6.5" FHD+ 90Hz
- **Camera**: 50 MP + 2 MP + 2 MP (rear), 8 MP (front)
- **OS**: Android 11
- **Features**: Side fingerprint, 18W fast charging
- **Price**: ~$190

---

### Nokia X-Series (Premium)

#### Nokia X10
- **Chipset**: Qualcomm Snapdragon 480 5G
- **Year**: 2021
- **RAM**: 6 GB
- **Storage**: 64 GB / 128 GB
- **Battery**: 4470 mAh
- **Display**: 6.67" FHD+
- **Camera**: 48 MP + 5 MP + 2 MP + 2 MP (rear), 8 MP (front)
- **OS**: Android 11
- **Features**: 5G, 3 years updates, recycled materials
- **Price**: ~$350

#### Nokia X20
- **Chipset**: Qualcomm Snapdragon 480 5G
- **Year**: 2021
- **RAM**: 6 GB / 8 GB
- **Storage**: 128 GB
- **Battery**: 4470 mAh
- **Display**: 6.67" FHD+
- **Camera**: 64 MP + 5 MP + 2 MP + 2 MP (rear), 32 MP (front)
- **OS**: Android 11
- **Features**: 5G, wireless charging, eco-friendly
- **Price**: ~$450

#### Nokia X30 5G
- **Chipset**: Qualcomm Snapdragon 695 5G
- **Year**: 2022
- **RAM**: 6 GB / 8 GB
- **Storage**: 128 GB / 256 GB
- **Battery**: 4200 mAh
- **Display**: 6.43" AMOLED FHD+ 90Hz
- **Camera**: 50 MP + 13 MP (rear), 16 MP (front)
- **OS**: Android 12
- **Features**: 100% recycled aluminum, IP67, wireless charging
- **Price**: ~$520

---

## Driver Features

### Spreadtrum/UNISOC Driver (`spreadtrum.c`)

**Capabilities**:
- Automatic chip detection
- CPU frequency and core count detection
- GPU type detection (Mali/PowerVR)
- Modem generation detection (2G/3G/4G/5G)
- Memory and camera specifications
- WiFi, Bluetooth, GPS status

**Supported Peripherals**:
- CPU power management
- GPU acceleration
- 2G/3G/4G/5G modem
- Camera ISP (up to 200 MP)
- Display controller
- WiFi (802.11 b/g/n/ac)
- Bluetooth
- GPS/GNSS

### Nokia Device Driver (`nokia.c`)

**Capabilities**:
- Device model detection
- Hardware specifications reporting
- Power management integration
- Display initialization
- Camera subsystem
- Audio (3.5mm jack + speaker)
- Sensor hub (accelerometer, proximity, ambient light)
- Fingerprint sensor (on supported models)
- FM Radio

**Supported Models**: 17 devices
- C-Series: C1, C2, C3, C5, C10, C20, C21, C30, C31
- G-Series: G10, G20, G21
- X-Series: X10, X20, X30

---

## Usage

### Configuration

```bash
make menuconfig
# Navigate to:
#   Mobile Device Support
#     -> Spreadtrum/UNISOC Chipset Support
#     -> Nokia Smartphone Support
```

### Build

```bash
make clean
make all
```

### Boot Output Example

```
Nokia: Initializing Nokia device driver
Nokia: Detected Nokia C3 (2020)
  Chipset: UNISOC SC9863A
  RAM: 3072 MB
  Storage: 32 GB
  Battery: 3040 mAh
  Display: 5.9"
  Camera: 8 MP (rear), 5 MP (front)
  OS: Android 10

Spreadtrum/UNISOC: Initializing chipset driver
Spreadtrum/UNISOC: Detected Spreadtrum SC9863A
  Architecture: ARM Cortex-A55 + A53
  CPU Cores: 8 @ 1600 MHz
  GPU: PowerVR
  Modem: 4G
  Max RAM: 6144 MB
  Features: WiFi=1, BT=1, GPS=1

Spreadtrum/UNISOC: CPU subsystem initialized
Spreadtrum/UNISOC: PowerVR GPU initialized
Spreadtrum/UNISOC: 4G Modem initialized
Spreadtrum/UNISOC: Camera (up to 16 MP) initialized
Nokia: Display initialized (5.9" IPS LCD)
Nokia: Rear camera initialized (8 MP)
Nokia: Front camera initialized (5 MP)
Nokia: Audio subsystem initialized (3.5mm jack + speaker)
Nokia: Sensor hub initialized (accelerometer, proximity, ambient light)
Nokia: FM Radio initialized
```

---

## Market Impact

### Spreadtrum/UNISOC Market Share
- **Global**: 11% of mobile chipsets (2023)
- **Emerging Markets**: 25-40% in India, Africa, Latin America
- **Annual Shipments**: 400+ million units
- **Key Markets**: India, Indonesia, Pakistan, Bangladesh, Africa

### Nokia Smartphone Market
- **C-Series**: Best-selling in budget segment ($70-$130)
- **Target**: First-time smartphone buyers, emerging markets
- **Durability**: Known for Nokia build quality
- **Software**: Clean Android, 2-3 years of updates

---

## Performance Benchmarks

| Chipset | AnTuTu Score | Use Case |
|---------|--------------|----------|
| SC6531E | N/A | Feature phones |
| SC7715 | ~15,000 | Basic Android |
| SC9820 | ~35,000 | Entry 4G |
| SC9832E | ~60,000 | Budget |
| SC9863A | ~110,000 | Mid-budget (Nokia C3) |
| T606 | ~210,000 | Budget gaming (Nokia G21) |
| T610 | ~240,000 | Mid-range |
| T616 | ~250,000 | Performance |
| T618 | ~260,000 | Gaming |
| T760 | ~450,000 | 5G mid-range |
| T820 | ~500,000 | 5G flagship |

---

## Comparison

### vs Qualcomm
- **Price**: 30-50% cheaper
- **Performance**: 60-70% of equivalent Snapdragon
- **Power efficiency**: Lower (28nm vs 6nm on older chips)
- **Market**: Budget vs Premium

### vs MediaTek
- **Price**: Similar
- **Performance**: Competitive in budget segment
- **Market**: Spreadtrum stronger in ultra-budget

---

## Summary

✅ **14 Spreadtrum/UNISOC chipsets** supported
✅ **17 Nokia smartphone models** supported
✅ **2G/3G/4G/5G modem** support
✅ **Feature phones to 5G flagships**
✅ **400+ million annual shipments**
✅ **Global market coverage**

Zirvium now supports the chipsets powering hundreds of millions of budget smartphones worldwide, making it accessible to users in emerging markets and those with Nokia devices using Spreadtrum/UNISOC processors!
