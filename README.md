# BLE Weather Station — handover for a new chat

_Updated 2026-11-01 (previous version 2026-10-09). Covers only the weather-station project. Paste or attach this file at the start of the new chat._

Items marked **(to confirm)** were not verified when this file was written.

---

## 1. Rules for the assistant (set by the owner, still in force)

1. Altered files are given **only as downloads**, and only after the owner explicitly agrees to an attachment.
2. Read-only access to files and repos. **No commits, pushes, branch actions or any interaction outside the chat.**
3. Nothing left behind when the chat closes. Never change the project layout (instructions, attached files, settings).
4. Critical but not to a fault. No metaphors, no idioms, no figures of speech. Plain English.
5. Exceptions only when the owner explicitly says so.

Working habits that helped:
- **Say what is verified and what is a guess.**
- **Read the actual files and logs before answering.**
- **Check the CubeMX diff after every regeneration.**
- Keep feedback short: the main point plus one fix, not long lists.

---

## 2. Hardware and tools

| Item | Details |
|---|---|
| Board | **P-NUCLEO-WB55** (Nucleo-68, MB1355, STM32WB55RG). Cortex-M4 64 MHz (application) + Cortex-M0+ 32 MHz (radio, closed ST firmware). On-board ST-LINK. User manual: UM2435 |
| Board history | Given away as a gift, returned 2026-11-01. Works (see section 4, "Check after return") |
| Wireless firmware | FUS v2.2.0, BLE stack **full** v1.24.0.3. Do not touch. (Zephyr BLE would need the "HCI only" binary instead; see section 8) |
| Cube package | STM32Cube_FW_WB V1.24.0. Project lives under `…/Projects/P-NUCLEO-WB55.Nucleo/Applications/BLE/BLE_WeatherStation`, project name `BLE_p2pServer` |
| Sensor in use | **BME680** on I2C1 (PB8/PB9), address `0x76`. Generic purple 6-pin breakout, printed only "BME680" (no maker name). Header soldered (confirmed 2026-11-01). Wiring below |
| Sensor not used | BH1750FVI: not wired, all code removed on purpose |
| Power | USB battery or PC USB |
| PC | Windows 11, STM32CubeIDE, STM32CubeProgrammer, Sourcetree, PuTTY (USART1, 115200 8N1). WSL2 with Ubuntu 26.04 for Zephyr (section 9) |
| Phone | Android, nRF Connect for Mobile |
| Other boards owned | Raspberry Pi 4, Raspberry Pi Pico 2 WH (not part of this project so far) |
| Tools | Digital oscilloscope, multimeter |

### BME680 wiring (verified on hardware 2026-11-01, direct wires, no breadboard)

| Wire | Nucleo socket | Signal | BME680 pin |
|---|---|---|---|
| Teal / green | CN6:4 | 3V3 | VCC |
| White | CN5:7 | GND | GND |
| Blue | CN5:10 | D15 / PB8, I2C1 SCL | SCL |
| Yellow | CN5:9 | D14 / PB9, I2C1 SDA | SDA |
| Black | CN6:6 | GND | SDO (I2C address 0x76) |
| Orange | CN6:2 | IOREF (3V3) | CS (I2C mode) |

- **Breakout pin order** (as printed, from one end): VCC, GND, SCL, SDA, SDO, CS. Labels are SCL/SDA (other breakouts may print SCK/SDI). A replacement breakout may have a different order: always follow the printed labels.
- At the sensor end, the six wires sit in **one 6-pin connector housing**, taped together. Keep teal on VCC when plugging it on.
- **Where the sockets are:** with the ST-LINK USB connector at the top, CN6 is the left inner (female) header, pin 1 at the top. CN5 is the upper part of the right inner header, pin 10 (D15) at the top end. The pin names are also printed on the board next to the headers; check them before connecting. (Based on UM2435 and the standard Nucleo-64 layout; the printed labels are the final check.)
- Teal and orange are 3V3; white and black are GND.
- **Never** use CN6:5 (5V) or CN5:8 (AVDD = VDDA, the MCU's analog supply) for the sensor.
- CN6:2 (IOREF) is used only for CS, a logic input. Measure it before use: it should read about 3.3 V.
- There is no 3V3 on CN5 or CN9. Other 3V3 pins: ST morpho CN7:16 (3V3) and CN7:12 (3V3 IOREF), male pins.
- Sensor header: soldered (2026-11-01). The earlier I2C errors (`0x20` timeout, `0x04` NACK) were caused by the unsoldered header.
- Before powering up: continuity check 3V3 to GND (no beep) and SDA to SCL (no beep).

**Arduino headers used (from UM2435 Table 10)**

| Header | Pin | Name | Note |
|---|---|---|---|
| CN6 | 2 | 3V3 (IOREF) | |
| CN6 | 4 | 3V3 | |
| CN6 | 5 | 5V | not for the sensor |
| CN6 | 6, 7 | GND | |
| CN5 | 7 | GND | |
| CN5 | 8 | AVDD (VDDA) | not for the sensor |
| CN5 | 9 | D14 / PB9 | I2C1_SDA |
| CN5 | 10 | D15 / PB8 | I2C1_SCL |

**Board LEDs (UM2435 §7.6)**

| LED | Function |
|---|---|
| LD1 blue (PB5), LD2 green (PB0), LD3 red (PB1) | User LEDs. **LD3 = firmware error** (`Error_Handler`, FreeRTOS hooks) |
| LD4 red | Overcurrent |
| LD5 red | Power OK. Normal |
| LD6 red/green | ST-LINK status. Normal |

**Buttons**

| Button | Function |
|---|---|
| SW1 (PC4) | P2P demo notification |
| SW2 (PD0) | Connection-parameter update request (it does **not** clear pairing) |
| SW3 (PD1) | Free |

PuTTY input: the text `SW1`, `SW2` or `SW3` simulates the button. Any other text gives "NOT RECOGNIZED COMMAND".

---

## 3. Repository state (checked 2026-10-30)

Repo: `Tudor-Popescu-Git/BLE_WeatherStation`. Earlier notes say private, but it could be cloned without credentials on 2026-10-14 and 2026-10-30 (through a proxy that may add access). **Visibility: owner to check on GitHub.**

| Branch | State |
|---|---|
| `main` (on GitHub) | **Verified firmware, sequencer version.** Head `a723e44` (README fix). Code commit `977348c` "Add history buffer and history BLE service, harden measurement path". |
| `freertos-port` (on GitHub) | FreeRTOS port in progress, head `23b6d7d` (2026-10-11). Builds (owner report, warnings not reviewed). **Not tested on hardware.** See section 6 |
| `feature/bh1750-history` | Deleted |

---

## 4. Firmware on `main` (verified)

**Measurement**
- Period `ENV_PERIOD_S` = 10 s for bench testing; use 600 for real use.
- The first measurement runs at start-up.
- Each cycle: trigger the BME680, wait 100 ms, read.
- Sequencer tasks: `ENV_TRIGGER`, `ENV_READ`, `HIST_SEND`.
- Timer-server timers: a repeating period timer and a 100 ms single-shot.
- Advertises as **P2PSRV1**, address `00:80:E1:26:C1:4B`.

**Environmental Sensing Service (ESS, 0x181A)**, read + notify, little-endian:

| Characteristic | UUID | Format |
|---|---|---|
| Temperature | 0x2A6E | sint16, 0.01 °C |
| Humidity | 0x2A6F | uint16, 0.01 % |
| Pressure | 0x2A6D | uint32, 0.1 Pa |

Each value is sent with one `aci_gatt_update_char_value` call (2 or 4 bytes), not byte by byte.

**History buffer**
- RAM ring buffer, `HIST_CAPACITY` 2016 records × 16 bytes ≈ 32 KB. Lost on reset.
- Record format, little-endian:

  | Bytes | Content |
  |---|---|
  | 0–3 | Sequence number (`0xFFFFFFFF` = end marker) |
  | 4–7 | Uptime in seconds |
  | 8–9 | Temperature, 0.01 °C, signed |
  | 10–11 | Humidity, 0.01 % |
  | 12–15 | Pressure, Pa (in the end marker: the next sequence number) |

- **Uptime** comes from `HAL_GetTick`. It is only valid while low power is off (`CFG_LPM_SUPPORTED` 0); needs an RTC-based count before low power.
- **Reboot detection:** if the marker's next sequence is lower than the phone's last one, the board restarted, so request from 0.

**History service** `7a1e0001-5c2b-4f6e-9b3a-1d2c3e4f5a6b`

| Characteristic | UUID | Properties |
|---|---|---|
| Control | `7a1e0003-…` | Write, exactly 4 bytes: uint32 LE, first sequence wanted |
| Data | `7a1e0004-…` | Notify, one 16-byte record per notification |

- In the code, the value handle is the characteristic handle + 1, and the CCCD is the characteristic handle + 2.
- A request is ignored until notifications are enabled on `…0004…`.
- After a disconnect the board forgets that setting: **re-enable after every reconnect.**
- A start number beyond the end is clamped, so only the marker is sent.
- Streaming sends until `BLE_STATUS_INSUFFICIENT_RESOURCES` (0x64), then resumes on `ACI_GATT_TX_POOL_AVAILABLE`.
- Disconnect stops streaming.

**Test in nRF Connect**
1. Enable notifications on `…0004…`.
2. Write a byte array to `…0003…`. The first byte is the lowest: start 157 is `9D-00-00-00`, all records is `00-00-00-00`, marker only is `FF-FF-FF-FF`.
3. PuTTY shows `HIST: request, sending seq A to B (N records)` and `HIST: done`.

**Verified on hardware (2026-10-04)**
- Partial request from 157: 30 records, then the marker.
- Full transfers of 275 and 329 records, with no gaps. The overlapping records were byte-identical.
- Request after reconnect.
- Restart detection after a reflash.
- Disconnect during a transfer, and resuming through a 355 s pause, using a temporary test build that is not in the repo.

**Check after return (2026-11-01)**
- Verified: connects in nRF Connect; ESS and history service present; ESS readings 24.06 °C, 47.83 %, 101528 Pa; notifications enabled on all three ESS characteristics.
- Not yet done: history transfer test; reading FUS and BLE stack versions in STM32CubeProgrammer **(to confirm)**.

**Not verified on hardware**
- The ring buffer wrapping at 2016 records (off-target test only).
- Whether the buffer-full wait path is ever reached on the board.
- Long runs at 600 s.
- Low power.

---

## 5. Things learned about the platform

**How the two cores talk**
- Shared RAM `RAM_SHARED` (10 KB at `0x20030000`), the IPCC channels and HSEM.
- The M4 starts the M0+ by setting the boot bit (C2BOOT). This can't be undone, and the M4 can't halt the M0+.
- A debugger halt of the M4 leaves the M0+ running the link.
- "ACI" = Application Command Interface, ST's command set for the radio core.

**The old scheduler**
- The sequencer (`UTIL_SEQ_*`) is cooperative: one stack, and each task runs to completion.
- Tasks that send BLE commands had to be listed in `CFG_Task_Id_With_HCI_Cmd_t`. They are paused while a command is pending.

**In the FreeRTOS version (ST-generated)**
- SHCI, HCI-event and advertising threads.
- Events from the M0+ wake a thread with `osThreadFlagsSet`.
- Command replies use a semaphore (`SemHciId`).
- One command at a time is enforced by the mutex `MtxHciId` (in `BLE_StatusNot`).
- Interrupts that call FreeRTOS functions need priority number ≥ 5 (`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY`). IPCC is at 5 and the RTC wake-up at 6, which is correct.

**Debugging**
- A GDB breakpoint in CubeIDE (Debug + conditional breakpoint) can stop a transfer to inspect variables.
- After Resume, the send loop continues before the disconnect is processed.
- The HAL tick may lose the halted time.

---

## 6. FreeRTOS port (branch `freertos-port`) — status

**Why it's built this way**
- With FreeRTOS on, CubeMX (this version) offers only the **Heart Rate** server; P2P Server disappears from the list.
- So the base is ST's generated Heart Rate + Device Information app.
- The weather code moves into the owner's own files `STM32_WPAN/App/weather_app.c` and `.h`, which CubeMX never touches.

**Phase 1 checkpoint passed on hardware (2026-10-07)**
- The board advertises as **`P2PSR`** (CubeMX limits the name to 5 characters here), address `00:80:E1:26:C1:4B`.
- It connects and bonds, and shows GATT, GAP, DIS (0x180A) and HRS (0x180D).
- No heart-rate values are sent, because `HrMeas` is unused. That's fine; the service will be removed.

**Settings that matter (CubeMX → FREERTOS)**
- `TOTAL_HEAP_SIZE`: now **65536** (`Core/Inc/FreeRTOSConfig.h` at `23b6d7d`). Earlier 24576; the default 3072 silently stopped the SHCI thread from being created, so there was no BLE at all.
- Malloc-failed hook and stack-overflow check (Option2) are enabled. Their hook functions in `app_freertos.c` turn on LD3 and halt.
- HAL time base is TIM17 (FreeRTOS uses SysTick).

**Check after every regeneration**
1. **SEQUENCER** stays disabled in CubeMX.
2. These stay excluded from the build: `Utilities/stm32_seq.c`, `Middlewares/STM32_WPAN/p2p_stm.c`, `Application/User/STM32_WPAN/App/p2p_server_app.c`. **Note (2026-10-30):** the committed `STM32CubeIDE/.project` links `p2p_server_app.c` and `stm32_seq.c`, and the committed `.cproject` has no exclusion entries. Check in CubeIDE whether the exclusions exist only locally.
3. The linked file `weather_app.c` is still in `Application/User/STM32_WPAN/App`.
4. `app_ble.c` USER CODE is intact. Also: the old `APP_BLE_Key_Button2_Action` still contains `UTIL_SEQ_SetTask` and must be converted or removed.

**Thread stack sizes in bytes** (from `app_conf.h`, `main.c` and `weather_app.c` at `23b6d7d`)

| Thread | Stack |
|---|---|
| SHCI | 2560 |
| HCI | 5120 |
| ADV | 2560 |
| HRS | 2560 |
| HistTask | 5120 (`128 * 40`, defined in `weather_app.c`) |
| SensorTask | 5120 (`128 * 40`, defined in `weather_app.c`) |
| defaultTask | 512 (placeholder; change `osDelay(1)` to `osDelay(osWaitForever)` or reuse it) |

**Target design for `weather_app.c`**

| Part | Behaviour |
|---|---|
| `SensorTask` | `if (ENV_Trigger()==HAL_OK){ osDelay(100); ENV_Read(); }`, then `osDelayUntil` the next period (1 tick = 1 ms) |
| `HistTask` | Waits on thread flags `HF_REQUEST` (0x01), `HF_TXPOOL` (0x02), `HF_DISC` (0x04). On a request it calls `HIST_StartRequest(HistReqFrom)`, then `HIST_SendSome()` (the old send body, unchanged logic) |
| `WEATHER_OnGattEvent(ecode, p_data)` | Runs in ST's HCI thread. **Only** sets state and flags. A control write stores `HistReqFrom` and sets `HF_REQUEST`; TX pool available sets `HF_TXPOOL` |
| `WEATHER_OnDisconnect()` | Sets `HistNotifyOn = 0` and `HF_DISC` |
| `WEATHER_Init()` | `ESS_Init(); HIST_Init();` creates mutex `HistMtx`, then `HistTask` and `SensorTask`, both `osPriorityBelowNormal` |
| Mutex `HistMtx` | Around `HIST_Add`, the `HistNextSeq` read in `HIST_StartRequest`, and each record's oldest-check plus pack. **Never** held during `aci_gatt_update_char_value`. Also protects `UptimeS()`, which both tasks call |
| Shared flags | `HistNotifyOn`, `HistStreaming` declared `volatile` |

**Hooks in `app_ble.c` (USER CODE blocks only) — still missing at `23b6d7d`**

Without these, none of the weather code runs.

| Block | Add |
|---|---|
| `Includes` | `#include "weather_app.h"` |
| `APP_BLE_Init_3` | `WEATHER_Init();` (after `HRSAPP_Init()`, before advertising) |
| `BLUE_EVT` | `WEATHER_OnGattEvent(p_blecore_evt->ecode, p_blecore_evt->data);` for `ACI_GATT_ATTRIBUTE_MODIFIED` and `ACI_GATT_TX_POOL_AVAILABLE` (the old `HIST_OnGattEvent` line is still there, commented out) |
| `EVT_DISCONN_COMPLETE_2` | `WEATHER_OnDisconnect();` |

**State of `weather_app.c` at `23b6d7d` (checked 2026-10-30)**

The 2026-10-08 blocking list is largely addressed by commit `23b6d7d` (per its message: `WEATHER_Init` and `WEATHER_OnDisconnect` written, `ENV_Trigger`/`ENV_Read` connected to the I2C calls with proper return values, `HistTask` logic with thread flags and mutex). Confirmed by reading the file: `WEATHER_Init` creates `HistMtx` and both tasks; `HistTask` waits on the flags. The return-value change was not re-checked.

Open items:
- `HistTask` holds `HistMtx` while calling `HIST_SendSome()`, which calls `aci_gatt_update_char_value`. This breaks the design rule above.
- Return values of `osThreadNew` and `osMutexNew` are not checked.
- `weather_app.h` has no include guard, and includes `p2p_server_app.h`, `stm32_seq.h`, `main.h`, `ble.h` and others it doesn't need.
- Stale comments: line 43 says `/* 100 ms */` for the 10 s period; the file headers still say `p2p_server_app.c`.
- Stray `_Static_assert(1, …)` (line 82), next to the real size check.
- Compiler warnings never reviewed. Likely causes: the `UptimeS()` low-power `#warning`, unused variables when `APP_DBG_MSG` compiles out, a missing `bme680.h` include, leftover includes.

**Retest when it builds (same as `main`)**
1. PuTTY shows ESS and HIST service creation with status `0x00`. The limits are fine: 8 services and 68 attributes allowed, about 6 and 40 used.
2. ESS values update every 10 s.
3. Full transfer `00000000`.
4. Partial request (`9D000000`) and request after reconnect.
5. Disconnect during a transfer (GDB breakpoint).
6. LD3 stays off.

Merge into `main` only when the results match.

**Later, optional**
- Remove HRS and DIS. That should allow a longer advertising name.
- FreeRTOS tickless idle and low power.

---

## 7. Open work, independent of FreeRTOS

1. **Ring-buffer wrap test.** Recommended: a temporary build with `HIST_CAPACITY` 20 (it wraps in about 200 s at 10 s). Alternatives: `ENV_PERIOD_S` 1 (about 34 min), or start the sequence counter at about 2000. Pass checks:
   - `00000000` returns only the newest 20 records, starting at next − 20.
   - A request for an overwritten sequence starts at the oldest record still kept.
   - A request inside the kept range starts exactly there.
2. **Phone or gateway side.**
   - Python + `bleak`: runs on the Windows PC (needs Bluetooth on the PC) or on the Raspberry Pi 4 as a gateway that stores records and serves them over Wi-Fi. Covers Python and embedded Linux.
   - Or a minimal Android app (Android Studio, Kotlin; Nordic Android BLE Library suggested): scan, connect, enable CCCD, write start, list records, save CSV. No background sync at first.
   - Either way, find the board by the history service UUID or its address, not by name.
   - Convert uptime with `time = phone_now − (marker.up_s − rec.up_s)`.
3. Later: RTC-based uptime plus low power; history in flash (needs flash/radio coordination through ST's semaphore and system commands); current measurement (the Nucleo itself is not low-power).

---

## 8. Decisions and options

**Deliberately undecided (owner's choice, not gaps)**
- Where the station goes and whether it runs unattended.
- Cellular modem (no budget now).

**Hardware: stay on the WB55.** Researched 2026-10-29 to 10-31 (prices are snapshots, excl. VAT):
- Optional second board for Zephyr + Nordic experience: **nRF54LM20 DK** (DigiKey part `1490-NRF54LM20-DK-ND`, about €38.90) plus **nRF7002 EBII** Wi-Fi 6 expansion board (`1490-NRF7002-EBII-ND`, about €10.40). Both are boards, not solder-on modules (Fanstel WM02C / WT02C40C are modules, not suitable). Nordic's Wi-Fi examples for the EBII are in the nRF Connect SDK. Set the DK's I/O voltage to 3.3 V for the BME680 (nRF Connect Board Configurator); check the EBII's supported voltage first.
- Pico 2 WH under Zephyr v4.5.0: Wi-Fi supported, **no BLE**. Pi 4 under Zephyr: no peripherals supported. Neither counts as ST/Nordic experience.

**Zephyr on the WB55 (v4.5.0)**
- Board `nucleo_wb55rg` is supported; `i2c1` is already on PB8/PB9 but defaults to 400 kHz (set 100 kHz in an overlay).
- Zephyr has its own BME680 driver (`drivers/sensor/bosch/bme680`). The own driver can also be kept and ported to `i2c_write_read_dt()`.
- BLE under Zephyr needs the **"HCI Only"** radio binary on the M0+ ("full stack" is not compatible since Cube WB 1.13.2). This replaces the current full stack, so do it only deliberately and last. Zephyr without BLE (blinky, BME680 over I2C) works without touching the radio firmware.

---

## 9. Zephyr environment (set up 2026-10-31)

- WSL2, Ubuntu 26.04. Workspace `~/zephyrproject`, Zephyr **v4.5.0**, west 1.5.0, Python 3.14 venv in `~/zephyrproject/.venv` (worked; Python 3.12 via `uv` is the fallback if packages fail). CMake 4.2.3, dtc 1.7.2.
- Every new terminal: `source ~/zephyrproject/.venv/bin/activate` then `cd ~/zephyrproject/zephyr`. If `west` is "not found", the venv is not active. Do not `sudo apt install west`.
- `native_sim` works: `west build -p always -b native_sim samples/hello_world`, then `./build/zephyr/zephyr.exe -stop_at=5`. `-no-rt` runs simulated time as fast as possible.
- Exercise done: `samples/synchronization` extended with a third thread, `K_MSGQ_DEFINE` queue and `K_MUTEX_DEFINE` mutex. Lessons: define kernel objects statically; the mutex must cover reads as well as writes; keep the locked section short; read compiler warnings.
- QEMU host tools are not available on native Windows; use WSL. Flashing from WSL: build in WSL, flash `zephyr.hex` from Windows with STM32CubeProgrammer (or pass USB to WSL with `usbipd-win`).

---

## 10. Gotchas

- **Reflash or reset empties the history buffer.** Mention reflashes when sending logs.
- **Clear the nRF Connect log before each test**, and send each log with one line saying what it is for.
- **Don't edit generated code outside USER CODE blocks.** CubeMX overwrites it (for example `a_LocalName` in `app_ble.c`).
- **In nRF Connect, use the scanner** (Android's Bluetooth settings may not list BLE devices).
- **nRF Connect caches services for bonded devices.** On 2026-11-01 it showed HRS/DIS (from the FreeRTOS test) while `main` was running. Fix: delete the bond in nRF Connect's BONDED tab and "Forget" the device in Android Bluetooth settings, then reconnect from the scanner.
