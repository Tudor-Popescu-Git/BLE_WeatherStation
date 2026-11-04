# BLE Weather Station — handover for a new chat

_Updated 2026-11-04 (previous version 2026-11-01). Paste or attach this file at the start of the new chat._

Items marked **(to confirm)** were not verified when this file was written.

---

## 1. Rules for the assistant (set by the owner, still in force)

1. Altered files are given **only as downloads**, and only after the owner explicitly agrees to an attachment.
2. Read-only access to files and repos. **No commits, pushes, branch actions or any interaction outside the chat.** Never write to the Claude project.
3. Nothing left behind when the chat closes. Never change the project layout (instructions, attached files, settings).
4. Critical but not to a fault. No metaphors, no idioms, no figures of speech. Plain English.
5. Exceptions only when the owner explicitly says so.
6. If a search result is unclear, search again instead of giving a vague answer.

Working habits that helped:
- **Say what is verified and what is a guess.**
- **Read the actual files and logs before answering.**
- **Check the CubeMX diff after every regeneration.**
- **Keep feedback short:** the main point plus one fix, not long lists. The owner asked for this explicitly.
- Don't assume the owner should already know something. Explain it once, briefly, without "you already met this" framing.
- The JD PDF in the project is marked "Sensitivity: C1-Internal" and contains a recruiter's contact. Read only; never copy it or send it anywhere.

---

## 2. Hardware and tools

| Item | Details |
|---|---|
| Board | **P-NUCLEO-WB55** (Nucleo-68, MB1355, STM32WB55RG). Cortex-M4 64 MHz (application) + Cortex-M0+ 32 MHz (radio, closed ST firmware). On-board ST-LINK. User manual: UM2435 |
| Board history | Given away as a gift, returned 2026-11-01. Works |
| Wireless firmware | FUS v2.2.0, BLE stack **full** v1.24.0.3 (**confirmed from the board's log 2026-11-02**). Do not touch. (Zephyr BLE would need the "HCI only" binary instead; see section 8) |
| Cube package | STM32Cube_FW_WB V1.24.0. Project under `…/Projects/P-NUCLEO-WB55.Nucleo/Applications/BLE/BLE_WeatherStation`, project name `BLE_p2pServer` |
| Sensor in use | **BME680** on I2C1 (PB8/PB9), address `0x76`. Generic purple 6-pin breakout, printed only "BME680". Header soldered 2026-11-01. Wiring below |
| Sensor not used | BH1750FVI: not wired, all code removed on purpose |
| Power | USB battery or PC USB |
| PC | Windows 11, STM32CubeIDE, STM32CubeProgrammer, Sourcetree, PuTTY (USART1, 115200 8N1). WSL2 with Ubuntu 26.04 for Zephyr (section 9) |
| Phone | Android, nRF Connect for Mobile |
| Other boards owned | Raspberry Pi 4, Raspberry Pi Pico 2 WH (not part of this project) |
| Tools | Digital oscilloscope, multimeter, breadboard |
| **On order** | **NXP FRDM-RW612** + a second BME680 (DFRobot Fermion SEN0375). See section 8 |

### BME680 wiring on the Nucleo (verified on hardware 2026-11-01, direct wires)

| Wire | Nucleo socket | Signal | BME680 pin |
|---|---|---|---|
| Teal / green | CN6:4 | 3V3 | VCC |
| White | CN5:7 | GND | GND |
| Blue | CN5:10 | D15 / PB8, I2C1 SCL | SCL |
| Yellow | CN5:9 | D14 / PB9, I2C1 SDA | SDA |
| Black | CN6:6 | GND | SDO (I2C address 0x76) |
| Orange | CN6:2 | IOREF (3V3) | CS (I2C mode) |

- **Breakout pin order** (as printed, from one end): VCC, GND, SCL, SDA, SDO, CS. A different breakout (for example the SEN0375) may have a different order and labels: always follow the printed labels.
- At the sensor end, the six wires sit in **one 6-pin connector housing**, taped together. Keep teal on VCC.
- **Where the sockets are:** with the ST-LINK USB connector at the top, CN6 is the left inner header, pin 1 at the top. CN5 is the upper part of the right inner header, pin 10 (D15) at the top end. Check the printed pin names before connecting.
- Teal and orange are 3V3; white and black are GND.
- **Never** use CN6:5 (5V) or CN5:8 (AVDD = VDDA) for the sensor.
- CN6:2 (IOREF) is used only for CS. Measure it before use: about 3.3 V.
- No 3V3 on CN5 or CN9. Other 3V3 pins: ST morpho CN7:16 (3V3) and CN7:12 (3V3 IOREF), male pins.
- The earlier I2C errors (`0x20` timeout, `0x04` NACK) were caused by the unsoldered header.
- Before powering up: continuity check 3V3 to GND (no beep) and SDA to SCL (no beep).

**Board LEDs (UM2435 §7.6)**

| LED | Function |
|---|---|
| LD1 blue (PB5), LD2 green (PB0), LD3 red (PB1) | User LEDs. **LD3 = firmware error** (`Error_Handler`, FreeRTOS hooks) |
| LD4 red | Overcurrent |
| LD5 red | Power OK. Normal |
| LD6 red/green | ST-LINK status. Normal |

**Buttons**: SW1 (PC4) P2P demo notification; SW2 (PD0) connection-parameter update (does **not** clear pairing); SW3 (PD1) free.

PuTTY: typing `SW1`, `SW2` or `SW3` simulates the button; anything else gives "NOT RECOGNIZED COMMAND". **Don't right-click in PuTTY**: it pastes the clipboard to the board. If lines step to the right, enable "Implicit CR in every LF".

---

## 3. Repository state

Repo: `Tudor-Popescu-Git/BLE_WeatherStation`. **Visibility (public or private): owner to check on GitHub (to confirm).**

| Branch | State |
|---|---|
| `main` | **Verified firmware, sequencer version.** Last code commit `977348c` (later commits change only this README) |
| `freertos-port` | **Last code commit `bce49e3`. Works on hardware (2026-11-02), partially tested.** See section 6 |
| `feature/bh1750-history` | Deleted |

---

## 4. Firmware on `main` (verified)

**Measurement**
- Period `ENV_PERIOD_S` = 10 s for bench testing; use 600 for real use.
- First measurement at start-up. Each cycle: trigger the BME680, wait 100 ms, read.
- Sequencer tasks: `ENV_TRIGGER`, `ENV_READ`, `HIST_SEND`.
- Advertises as **P2PSRV1**, address `00:80:E1:26:C1:4B`.

**Environmental Sensing Service (ESS, 0x181A)**, read + notify, little-endian:

| Characteristic | UUID | Format |
|---|---|---|
| Temperature | 0x2A6E | sint16, 0.01 °C |
| Humidity | 0x2A6F | uint16, 0.01 % |
| Pressure | 0x2A6D | uint32, 0.1 Pa |

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

- Uptime comes from `HAL_GetTick`; valid only while low power is off.
- Reboot detection: if the marker's next sequence is lower than the phone's last one, the board restarted, so request from 0.

**History service** `7a1e0001-5c2b-4f6e-9b3a-1d2c3e4f5a6b`

| Characteristic | UUID | Properties |
|---|---|---|
| Control | `7a1e0003-…` | Write, exactly 4 bytes: uint32 LE, first sequence wanted |
| Data | `7a1e0004-…` | Notify, one 16-byte record per notification |

- Value handle = characteristic handle + 1; CCCD = characteristic handle + 2.
- A request is ignored until notifications are enabled on `…0004…`. **Re-enable after every reconnect.**
- Streaming sends until `BLE_STATUS_INSUFFICIENT_RESOURCES` (0x64), then resumes on `ACI_GATT_TX_POOL_AVAILABLE`.

**Test in nRF Connect**
1. Enable notifications on `…0004…`.
2. Write a byte array to `…0003…`, lowest byte first: start 157 is `9D-00-00-00`, all is `00-00-00-00`, marker only is `FF-FF-FF-FF`.
3. PuTTY shows `HIST: request, sending seq A to B (N records)` and `HIST: done`.

**Verified on hardware (2026-10-04)**: partial request, full transfers of 275 and 329 records with no gaps, request after reconnect, restart detection, disconnect and resume during a transfer.

**Not verified**: ring-buffer wrap at 2016 records on the board, long runs at 600 s, low power.

---

## 5. Things learned about the platform

**How the two cores talk**
- Shared RAM `RAM_SHARED` (10 KB at `0x20030000`), IPCC channels and HSEM.
- The M4 starts the M0+ (C2BOOT); this can't be undone. A debugger halt of the M4 leaves the M0+ running.
- "ACI" = Application Command Interface, ST's command set for the radio core.

**BLE event path in the FreeRTOS build (traced 2026-11-03)**

IPCC interrupt → `hw_ipcc.c` → `tl_mbox.c` → `hci_tl.c` `TlEvtReceived` → `hci_notify_asynch_evt` (sets a thread flag) → `HciUserEvtProcess` thread → `SVCCTL_UserEvtRx` → registered service handlers → `SVCCTL_App_Notification` (in `app_ble.c`) → `WEATHER_OnGattEvent` → thread flags → `HistTask`.
- `app_conf.h:447` maps `HAL_IPCC_RX_IRQHandler` to `HW_IPCC_Rx_Handler`.
- **Command path:** `hci_send_req` takes mutex `MtxHciId` (one command at a time), sends, waits on semaphore `SemHciId` for the reply.
- `SVCCTL_App_Notification` is the application's single entry point for BLE events that no service handler consumed (connect, disconnect, GATT events).

**GATT basics used here**: server (the board) holds services → characteristics → descriptors; CCCD enables notify/indicate; notify has no acknowledgement, indicate has one.

**The 10 `aci_gatt_*` calls in `weather_app.c` (freertos-port)**: `aci_gatt_add_service` (lines 245, 419), `aci_gatt_add_char` (250, 256, 262, 424, 430), `aci_gatt_update_char_value` (282, 497, 518).

**FreeRTOS on this chip**
- Interrupts that call FreeRTOS functions need priority number ≥ 5. IPCC is 5, RTC wake-up 6: correct.
- `TOTAL_HEAP_SIZE` 65536. The default 3072 silently stopped the SHCI thread from being created.

---

## 6. FreeRTOS port (branch `freertos-port`, head `bce49e3`)

**Why it's built this way**: with FreeRTOS on, CubeMX offers only the Heart Rate server, so the base is ST's Heart Rate + DIS app, and the weather code lives in the owner's files `STM32_WPAN/App/weather_app.c` / `.h`, which CubeMX never touches.

**Done (by the owner, 2026-11-02)**
- Hooks in `app_ble.c` USER CODE blocks:

  | Line | Block | Code |
  |---|---|---|
  | 41 | `Includes` | `#include "weather_app.h"` |
  | 394 | `APP_BLE_Init_3` | `WEATHER_Init();` |
  | 461 | `EVT_DISCONN_COMPLETE_2` | `WEATHER_OnDisconnect();` |
  | 694 | `BLUE_EVT` | `case ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE:` / `case ACI_GATT_TX_POOL_AVAILABLE_VSEVT_CODE:` → `WEATHER_OnGattEvent(p_blecore_evt->ecode, p_blecore_evt->data); break;` |

- Task priorities changed to `osPriorityBelowNormal` (`weather_app.c` lines 62, 70).
- Build file set (fixes `'CFG_SCH_PRIO_NBR' undeclared`): `STM32CubeIDE/.cproject` excludes `stm32_seq.c` and `p2p_server_app.c` (committed, verified 2026-11-04). `p2p_stm.c` is not linked in `.project` on this branch, so it needs no exclusion.

**Verified on hardware (2026-11-02)**
- Services created with status `0x00`; ESS readings appear every 10 s.
- `HIST: notifications enabled`, `HIST: request, sending seq 0 to 32 (32 records)`, `HIST: done`.

**Not tested yet**
1. Count of records received on the phone (should match the log).
2. Partial request (`9D-00-00-00` or another start inside the range).
3. Request after reconnect.
4. Large transfer (about 300 records, about 50 min at 10 s) to exercise the `HF_TXPOOL` path.
5. Disconnect during a transfer.
6. LD3 stays off during all of the above.

Merge into `main` only when the results match `main`.

**Known code issues in `weather_app.c` / `.h`**
- **Main one:** `HistTask` (line 200) holds `HistMtx` while calling `HIST_SendSome()` (line 478), which calls `aci_gatt_update_char_value`. Fix after the tests: take the mutex only to copy each record, release it, then send.
- Return values of `osThreadNew` / `osMutexNew` not checked.
- `weather_app.h` has no include guard and includes `p2p_server_app.h` and `stm32_seq.h`, which are no longer needed.
- Stale `/* 100 ms */` comment at line 43; stray `_Static_assert(1, …)` at line 82; file headers still say `p2p_server_app.c`.
- `APP_BLE_Key_Button2_Action` in `app_ble.c` still contains `UTIL_SEQ_SetTask` (to confirm it is gone or unused).
- Compiler warnings never reviewed.

**Check after every CubeMX regeneration**: SEQUENCER stays disabled; the two `.cproject` exclusions above remain and `p2p_stm.c` is still not linked; `weather_app.c` is still linked; the `app_ble.c` USER CODE hooks are intact.

**Later, optional**: remove HRS and DIS (allows a longer advertising name than `P2PSR`); tickless idle and low power.

---

## 7. Open work, independent of FreeRTOS

1. **Ring-buffer wrap test**: temporary build with `HIST_CAPACITY` 20.
2. **Phone or gateway side**: Python + `bleak` (Windows PC or Raspberry Pi 4 as gateway), or a minimal Android app. Find the board by the history service UUID. Convert uptime with `time = phone_now − (marker.up_s − rec.up_s)`.
3. Later: RTC-based uptime plus low power; history in flash; current measurement.
4. **README split per branch** (`main`, `freertos-port`, and a future Zephyr branch). Offered as downloads; owner has not confirmed.

---

## 8. New board: NXP FRDM-RW612 (decided 2026-11-04)

**Why this board**: Wi-Fi 6 + BLE 5.3 + 802.15.4 on one chip, Cortex-M33 at 260 MHz, 1.2 MB SRAM, on-board MCU-Link debugger, Arduino header, supported by Zephyr (Wi-Fi and BLE need `west blobs fetch hal_nxp`). Kit contents: board, quick start guide, USB-C cable. It also has an on-board **P3T1755** I3C/I2C temperature sensor (temperature only), useful for a first Zephyr I2C test with no wiring.

**Order (to confirm: shop, delivery option, and whether the order was placed)**
- Mouser cart seen 2026-11-04: FRDM-RW612 145.15 lei + DFRobot SEN0375 BME680 82.34 lei = 227.49 lei without VAT. Free delivery at Mouser Romania only above 380 lei.
- Delivery choices shown: **DDP** FedEx International Priority 130 lei (no payment at the door), or **FCA** UPS Express Saver / DHL Express 90 lei (VAT about 21% plus a courier fee collected at the door). "Bill Recipient" options need the buyer's own courier account: not usable.
- Alternative compared: DigiKey (board €25.12 without VAT; free delivery from €50, otherwise €18).
- **Assumed arrival: Tuesday 2026-11-10** (4 working days from 2026-11-04).

**When the board arrives (planned order)**
1. Check the kit contents; connect via USB-C to the MCU-Link port; confirm the PC sees it.
2. Zephyr in WSL: `west blobs fetch hal_nxp`, then build blinky for board `frdm_rw612` (to confirm the exact board name in Zephyr v4.5.0 with `west boards | grep rw612`).
3. Flashing: MCU-Link with NXP LinkServer, from Windows or from WSL via `usbipd-win` (to confirm the runner).
4. Read the on-board P3T1755 over I2C.
5. Connect a BME680 to the Arduino header I2C pins (D14/D15 positions; **check the FRDM-RW612 user manual for the RW612 pin names and the header voltage, 3.3 V expected, before wiring**). Use 3.3 V only.
6. Port the BLE weather service (ESS first, then history) to Zephyr's Bluetooth API. Later: Wi-Fi.

**SEN0375 notes (to confirm on arrival)**: may need its header pins soldered; its default I2C address may be `0x77` instead of `0x76`. Check the DFRobot wiki and the board's labels. The jumper wires considered (SparkFun PRT-09387) are male-to-male: fine with a breadboard; direct sensor-to-board needs female-to-male.

**Other boards researched and not chosen**: nRF54LM20 DK + nRF7002 EBII, nRF54L15 DK, nRF7002 DK, NUCLEO-WBA55CG / WBA25CE1 / WBA65RI. Pico 2 WH (owned) has Wi-Fi but **no BLE** under Zephyr.

**Zephyr on the WB55 (v4.5.0)**: board `nucleo_wb55rg`; `i2c1` on PB8/PB9 at 400 kHz by default (set 100 kHz in an overlay); Zephyr has its own BME680 driver. BLE needs the **"HCI Only"** radio binary on the M0+, which replaces the current full stack: do it only deliberately.

---

## 9. Zephyr environment (set up 2026-10-31)

- WSL2, Ubuntu 26.04. Workspace `~/zephyrproject`, Zephyr **v4.5.0**, west 1.5.0, Python 3.14 venv `~/zephyrproject/.venv`. CMake 4.2.3, dtc 1.7.2.
- Every new terminal: `source ~/zephyrproject/.venv/bin/activate` then `cd ~/zephyrproject/zephyr`. If `west` is "not found", the venv is not active. Do not `sudo apt install west`. (Suggested: add the `source` line to `~/.bashrc`.)
- If Python modules are missing (for example `jsonschema`): `west packages pip --install`, and build from `~/zephyrproject/zephyr`.
- `native_sim`: `west build -p always -b native_sim samples/hello_world`, then `./build/zephyr/zephyr.exe -stop_at=5`. `-no-rt` runs simulated time as fast as possible.
- Windows path to WSL files: `\\wsl$\Ubuntu\home\<user>\zephyrproject` in File Explorer.
- Exercise done: `samples/synchronization` extended with a third thread, a `K_MSGQ_DEFINE` queue and a `K_MUTEX_DEFINE` mutex. Lessons: define kernel objects statically; the mutex must cover reads as well as writes; keep the locked section short (no `printk` inside); initialise variables; read compiler warnings.
- Kconfig: like `FreeRTOSConfig.h` but for the whole OS, with dependencies between options; result in `build/zephyr/.config`; `west build -t menuconfig`. Devicetree describes hardware; boards are adjusted with overlay files.

---

## 10. Interview preparation (Alten Romania, Senior Embedded Software Engineer)

- Date not set yet. Main RTOS in the role: Zephyr. ST and Nordic experience are relevant.
- Owner background: about 9 years embedded; oscilloscope and logic analyser used 2016–2019; no bootloader work; RTOS experience limited to basics before this project.
- **Mock interview**: Q1–Q5 done. Continue from **Q6**. Feedback format: short, main point plus one fix.
- Points to remember from Q1–Q5: use exact API names (`aci_gatt_update_char_value`, not an approximation); describe concrete results from this project (FreeRTOS port, history transfer with flow control, verified on hardware); mutex vs semaphore (ownership, priority inheritance), priority inversion, `volatile` and read-modify-write, ISR-safe queues.
- CV edits: discussed, not finished.

---

## 11. Gotchas

- **Reflash or reset empties the history buffer.** Mention reflashes when sending logs.
- **Clear the nRF Connect log before each test**, and send each log with one line saying what it is for.
- **Don't edit generated code outside USER CODE blocks.**
- **In nRF Connect, use the scanner.**
- **nRF Connect caches services for bonded devices.** Fix: delete the bond in nRF Connect's BONDED tab and "Forget" the device in Android Bluetooth settings, then reconnect.
- **Before switching branches, run `git status`.** A modified `.cproject` carries over to the other branch and can break its build (2026-11-04: a leftover `p2p_stm.c` exclusion caused `undefined reference to P2PS_STM_App_Update_Char` on `main`; fixed with `git restore .cproject`). After switching, press F5 on the project in CubeIDE, then Clean before building.
- At online shops, check the **Incoterms** line (DDP = everything paid at checkout; FCA = VAT and fees paid at the door) and the delivery line on the final checkout page, not the banner.
