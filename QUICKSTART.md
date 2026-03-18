# Quick Start Guide — PIC32CZ CA70 PX4 Port

## 1. Clone the Repository

```bash
git clone --recursive git@github.com:Vigneshjr1/px4_pic32czca70.git
cd px4_pic32czca70
git checkout pic32cz-ca70-port
git submodule update --init --recursive
```

> NuttX is fetched automatically from `Vigneshjr1/NuttX.git` (branch `pic32cz-ca70-port`).

---

## 2. Install Prerequisites

```bash
# ARM toolchain
sudo apt install gcc-arm-none-eabi

# Python dependencies
pip3 install -r requirements.txt
```

---

## 3. Build

```bash
make microchip_pic32czca70-curiosity_default
```

Build output:
- ELF: `build/microchip_pic32czca70-curiosity_default/microchip_pic32czca70-curiosity_default.elf`
- BIN: `build/microchip_pic32czca70-curiosity_default/microchip_pic32czca70-curiosity_default.bin`
- PX4: `build/microchip_pic32czca70-curiosity_default/microchip_pic32czca70-curiosity_default.px4`

---

## 4. Generate HEX (optional)

```bash
arm-none-eabi-objcopy -O ihex \
  build/microchip_pic32czca70-curiosity_default/microchip_pic32czca70-curiosity_default.elf \
  build/microchip_pic32czca70-curiosity_default/microchip_pic32czca70-curiosity_default.hex
```

---

## 5. Flash the Board

Use MPLAB IPE or any compatible programmer with the generated `.hex` file to flash the **PIC32CZ CA70 Curiosity** board.

---

## 6. Connect to the Board

| Port | Connector | Host Device | Purpose |
|------|-----------|-------------|---------|
| PKOB4 debug USB | J700 | `/dev/ttyACM0` | NSH console |
| TARGET USB | J200 | `/dev/ttyACM1` | MAVLink |

Open NSH console:
```bash
minicom -D /dev/ttyACM0 -b 115200
```

---

## 7. HITL Simulation (jMAVSim)

Required parameters (set once via NSH or QGC):
```
SYS_HITL=1
SYS_AUTOSTART=1001
CBRK_IO_SAFETY=22027
CBRK_SUPPLY_CHK=894281
COM_ARM_WO_GPS=1
COM_DISARM_PRFLT=-1
SDLOG_MODE=-1
```

Run jMAVSim (requires Java 11):
```bash
Tools/simulation/jmavsim/jmavsim_run.sh -d /dev/ttyACM1 -b 57600 -q
```

Connect QGC via UDP `localhost:14550`.

---

## Hardware

- **Board:** PIC32CZ CA70 Curiosity (144-pin)
- **QSPI Flash:** SST26VF032B 4MB
  - `/fs/mtd_params` — parameters (128 KB)
  - `/fs/mtd_caldata` — calibration data (64 KB)
  - `/fs/mtd_waypoints` — dataman / waypoints (512 KB)
