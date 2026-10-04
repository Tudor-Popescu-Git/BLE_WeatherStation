# BLE Weather Station — STM32WB55

Wireless mini weather station on the P-NUCLEO-WB55 pack. A **BME680** (temperature,
humidity, pressure) and a **BH1750FVI** (ambient light, not yet wired) are read over I2C
by the Cortex-M4, and the readings are exposed over BLE GATT.

**This file is a handoff document.** It records what works, what was learned the hard
way, what was decided and why, and what remains — so work can resume in a fresh session
without repeating anything.

_Last updated: 2026-10-04 · repo `main` includes the history service described in §5 (commit `977348c`)_

---

## Repo access — how to read the current code

Repo: **`Tudor-Popescu-Git/BLE_WeatherStation`**, branch `main`, **private**.

There are two ways a Claude session can see it. Use the second when freshness matters.

1. **Project knowledge sync.** Search the project knowledge for `HAL_I2C_MspInit` or
   `Adv_Cancel`; if source comes back, the sync is live. **But it is a snapshot**, refreshed
   only manually from the Context panel. On 2026-10-02 it was two commits behind and missed
   the whole BLE service — conclusions drawn from it were stale.
2. **Attach the repo to the session and clone it** (worked on 2026-10-02/03): the session's
   add-repo tool, then a shallow `git clone --depth 1`. This gives the latest *pushed*
   state. Read access is enough for review.

Either way, **uncommitted or unpushed changes on the PC are invisible.** When a conclusion
depends on a file as it is on disk right now, say "as committed on `main`" and ask.

Checks that give a false "no GitHub" (all three were tried once and misled): listing
`/mnt/project/` (only shows uploaded docs), looking for a GitHub MCP connector (the project
sync is a different mechanism), and anonymous `curl api.github.com` (repo is private).

---

## Status

| Stage | State |
|---|---|
| M0+ wireless coprocessor stack | ✅ FUS v2.2.0, BLE stack v1.24.0.3 — do not touch |
| CubeIDE project from ST `BLE_p2pServer` example | ✅ |
| BLE advertising as `P2PSRV1`, never expires (LP fallback) | ✅ |
| Debug trace over USART1 → PuTTY 115200 8N1 | ✅ |
| I2C1 bus (PB8/PB9, 100 kHz) | ✅ BME680 ACKs at `0x76`, `chip_id=0x61`, `variant=0x00` |
| BME680 driver — compensated T / P / RH | ✅ Hand-written, `Core/Src/bme680.c` + `Core/Inc/bme680.h` |
| BLE Environmental Sensing Service (0x181A) | ✅ Verified in nRF Connect |
| Periodic measurement via timer server + sequencer | ✅ Period `ENV_PERIOD_S` = 10 s (testing value; use 600 in service). First measurement runs at start-up |
| Sensor header soldered / mechanically secure | 🟨 **Not done** — works only while something presses on it |
| History buffer + history BLE service on the board | ✅ Built, flashed and verified on hardware with nRF Connect (see §5). Ring wrap and long unattended runs not yet tested on hardware |
| Android app: download history, upload to server | ⬜ Not started — next step now that §5 is verified |
| BH1750FVI driver (lux) | ⬜ Not wired |
| Cellular (LTE-M) modem | ⬜ Evaluated, not bought (see §6) |
| Battery operation | ⬜ Not started |

Readings on 2026-10-01, board untouched by hand: **T 25.28 °C, RH 34 %, P 102189 Pa**
(1021.9 hPa *station* pressure, not sea-level). Pressure noise ±3 Pa at 16× oversampling.

---

## Hardware

- **P-NUCLEO-WB55**: NUCLEO-WB55RG (STM32WB55RG, 1 MB flash, Rev Y) + USB dongle.
  Neither has an environmental sensor onboard.
- **BME680** breakout — confirmed genuine BME680 (`0xD0`=`0x61`, `0xF0`=`0x00`).
  CSB strapped high (I2C mode), SDO strapped low (address `0x76`). Pull-ups on the
  breakout, ~10 kΩ — **do not add external pull-ups**.
- **BH1750FVI** breakout — not wired yet. Address `0x23` with ADDR to GND. No chip-ID
  register; an address probe is the only check.
- USB power bank.

### Wiring (confirmed on hardware)

| Signal | Wire | Nucleo | MCU pin | Header | Sensor |
|---|---|---|---|---|---|
| I2C1_SCL | blue | D15 | PB8 | CN5-10 | SCL |
| I2C1_SDA | yellow | D14 | PB9 | CN5-9 | SDA |
| 3V3 | orange | 3V3 | — | CN6-4 | VCC |
| GND | black | GND | — | CN6-6 | GND |

CN6-7 is a second GND. 3.3 V only — WB55 I2C pins are not 5 V tolerant. The HAL wants
I2C addresses shifted left by one (`0x76` → `0xEC`).

### ⚠️ The BME680 pin header is NOT soldered

The pins sit loose in the plated holes. This was the root cause of every I2C failure in
this project (two months of debugging). Contact only holds while something presses on the
header. A plastic/insulated object resting on it works and doesn't heat the sensor — a
**finger does**: it drove the temperature reading to 33 °C and makes humidity read low.

**Fix options, best first:** solder it (temperature-controlled station ~200 lei, leaded
60/40 solder is easiest); clinch the pins (bend each protruding tip flat at the board
surface, alternating directions, needle-nose pliers, support the board); wedge a snipped
resistor lead beside each pin. **Never** liquid-metal CPU paste (gallium alloys into
copper). A breadboard or the Nucleo's own headers do **not** help — they grip the wrong
end of the pin. Breakouts with pre-fitted JST connectors (STEMMA QT / Qwiic) avoid this.

Diagnosing it: `HAL_I2C_GetError()` = `0x20` (timeout, START never happened) means the
lines don't reach the MCU — the joint. `0x04` (NACK) means the bus works but the sensor
didn't answer. Always check the HAL return value too, not only the error code.

### Reference documents

- BME680 datasheet BST-BME680-DS001, rev 1.6 —
  <https://download.pi3g.com/bosch/bst-bme680-ds001.pdf>. p. 28 Table 20 is the register
  map; pp. 17–20 calibration registers and formulas.
- ST UM2819 (NUCLEO-WB55 user manual) — header pin mapping, power options.

---

## Toolchain

- Windows 11, STM32CubeIDE + standalone STM32CubeMX, STM32CubeProgrammer v2.23.0
  standalone (needed for the FUS panel), STM32Cube_FW_WB **V1.24.0**.
- Project folder:
  `C:\Users\popes\STM32Cube\Repository\STM32Cube_FW_WB_V1.24.0\Projects\P-NUCLEO-WB55.Nucleo\Applications\BLE\BLE_WeatherStation`
- The Eclipse project lives in the `STM32CubeIDE\` subfolder and is named `BLE_p2pServer`
  (cosmetic, deliberate). Sources in `Core\Src` are **linked** into it individually under
  `Application/User/Core`; a new `.c` file must be added as a linked file
  (drag onto that folder → *Link to files*), as was done for `bme680.c`.
- Headers in `Core\Inc` (`main.h`, `app_conf.h`, `bme680.h`) don't appear as normal files
  in Project Explorer — find them under the project's `Includes` node, or Ctrl+click an
  `#include`.
- If F3 / Ctrl+click can't find a HAL function: right-click project → Index → Rebuild;
  check Paths and Symbols has `USE_HAL_DRIVER` and `STM32WB55xx`. Workaround: Ctrl+Shift+R
  to open `stm32wbxx_hal_i2c.c`, then Ctrl+O.

### Never repeat: coprocessor (M0+) firmware

Verified via CubeProgrammer FUS panel: FUS v2.2.0.0, STACK v1.24.0.3 at `0x080D0000`.
Already the newest; **do not run `-fwdelete` / `-fwupgrade`**. FUS CLI commands wipe the
M4 app; afterwards restore option bytes `nSWboot0=1 nboot1=1 nboot0=1`. `mode=UR` always.
Install addresses change per package version — read `Release_Notes.html` every time.

---

## Firmware architecture (`main`, including the history service in §5)

```
RTC-driven timer server (HW_TS)
  ├─ EnvPeriodTimer (repeating, ENV_PERIOD)  → task ENV_TRIGGER: write BME680 config, start conversion
  └─ EnvConvTimer  (single-shot, 100 ms)    → task ENV_READ:    read, compensate, update GATT, add to history, trace
Sequencer (UTIL_SEQ) runs the tasks from MX_APPE_Process() in the main loop.
```

| File | What's there |
|---|---|
| `Core/Src/bme680.c`, `Core/Inc/bme680.h` | Driver: `BME680_Init()`, `BME680_Trigger(hi2c)`, `BME680_Read(Env_t*)` |
| `Core/Src/main.c` | `Export_hi2c()` in `USER CODE 4` (returns `&hi2c1`, error if not initialised). Main loop is just `MX_APPE_Process()`. `Error_Handler()` now disables interrupts, lights the red LED and halts |
| `STM32_WPAN/App/p2p_server_app.c` | ESS service, history service, timers, tasks — all inside USER CODE blocks |
| `Core/Inc/app_conf.h` | Task IDs `CFG_TASK_ENV_TRIGGER_ID`, `CFG_TASK_ENV_READ_ID`, `CFG_TASK_HIST_SEND_ID` in the *with-HCI-command* list |
| `STM32_WPAN/App/app_ble.c` | `Adv_Cancel()` patch (LP advertising fallback), and a `BLUE_EVT` case that forwards `ACI_GATT_ATTRIBUTE_MODIFIED` and `ACI_GATT_TX_POOL_AVAILABLE` to `HIST_OnGattEvent(ecode, data)` |

**Rules this structure depends on:**
- Timer-server callbacks run in **interrupt context** — they may only `UTIL_SEQ_SetTask()`.
- **Never call `aci_*` BLE functions from the main loop or an interrupt.** Only from
  sequencer tasks registered in the "with HCI command" list in `app_conf.h`; the stack uses
  that list to avoid two BLE commands in flight.
- `HAL_Delay()` is overridden in `app_entry.c` (sleeps with `__WFI`, millisecond only) and
  blocks the sequencer — don't use it in tasks; use a timer.
- Every edit must be inside a `/* USER CODE BEGIN … END */` block or CubeMX regeneration
  deletes it. Commit before regenerating, `git diff` after.

### BME680 driver — key facts (hand-written, not Bosch's API)

- Calibration read once: 10 bytes from `0xE1` (humidity coefficients + `par_t1`) and
  24 bytes from `0x89` (`par_t2/t3`, `par_p1..p10`). Unique per chip, never changes.
- Per measurement, in this order: `0x70`=`0x08` (heater off), `0x71`=`0x00` (no gas),
  `0x72`=`0x01` (humidity 1×), **then** `0x74`=`0x55` (T 2×, P 16×, forced mode).
  `ctrl_hum` only takes effect when `ctrl_meas` is written after it.
- Wait ≥ ~50 ms (16× pressure); 100 ms used. Read 10 bytes from `0x1D`; bit 7 of byte 0 =
  new data (driver returns `HAL_BUSY` if not set).
- Integer compensation per Bosch; temperature first (`t_fine` feeds P and RH).
- **Traps that give plausible but wrong numbers:** `par_p7` is at `0x98` and `par_p6` at
  `0x99` (swapped vs numeric order); `0xE2` is shared — low nibble → `par_h1`, high nibble
  → `par_h2`; `I2C_MEMADD_SIZE_8BIT` (16-bit would read the wrong registers silently).
- Gas heater deliberately unused: it warms the die and corrupts temperature; meaningful
  air-quality output needs Bosch's closed BSEC library.
- Temperature reads above ambient by an unknown amount (board heat); calibrate against a
  reference thermometer once the header is fixed.

### BLE GATT

- **Environmental Sensing Service 0x181A** (read + notify):
  Temperature 0x2A6E sint16 0.01 °C · Humidity 0x2A6F uint16 0.01 % · Pressure 0x2A6D
  uint32 0.1 Pa. Little-endian. Notifications only go out after the phone enables them.
- The original ST P2P service is still present (unused).
- Limits in `app_conf.h`: 8 GATT services, 68 attributes, 6 timer-server timers — ample.
- `ble_conf.h` allows **one** service event handler (`BLE_CFG_SVC_MAX_NBR_CB 1`), already
  taken by P2P, and that define is outside a USER CODE block. Route events for new
  services through `SVCCTL_App_Notification()` in `app_ble.c` (`USER CODE BEGIN BLUE_EVT`).
- Testing: **nRF Connect for Mobile** (Android) → scan → `P2PSRV1` → Environmental Sensing
  → single arrow = read, triple arrow = notifications. If a connection drops instantly,
  clear the pairing on both sides (phone: Forget; board: SW2).

### Open code-quality notes (from review of `960b5e8`, mostly fixed in `b6614ab`)

- ✅ Include guard in `bme680.h`, header no longer includes BLE app headers.
- ✅ `bme680.c` moved to `Core/Src` and linked.
- ✅ Duplicate `USER CODE BEGIN 0` removed.
- ✅ `ENV_Read_Task` reports `HAL_BUSY` as "conversion not finished" and every I2C failure
  includes `HAL_I2C_GetError()`.
- ✅ `Error_Handler()` halts with the red LED on. If it lights after flashing, an init call failed.
- ✅ Negative temperatures print correctly (the old trace printed `-5.-25`).

---

## Next steps

### 1. Fix the header (blocking for anything unattended)

See the hardware section. Until then every reading depends on something pressing on it.

### 2. Measurement period

Done: `ENV_PERIOD_S` define (10 s while testing) and an immediate first measurement at
start-up. Set it to 600 (10 min) before any unattended run.

### 3. Getting data off the board — decision record (2026-10-02)

Requirement stated: **live data viewable from anywhere.** Options evaluated:

| Option | Verdict |
|---|---|
| Phone browser page (Web Bluetooth) | Chrome on Android only. **Firefox and all iPhone browsers don't support Web Bluetooth.** Page must stay open, screen on. |
| Spare phone (Xiaomi Mi 8 Lite) as gateway next to the station | Works, but on battery it needs charging every few days — no better than the main phone. Can't act as a modem for the board (USB tethering needs a USB host, WB55 is device-only; no Wi-Fi; BT tethering is Classic, WB55 is BLE-only). |
| Main phone + history buffer on the board | Free. Complete data, but **live only while the phone is within ~10–30 m**; gaps fill in on return. |
| Wi-Fi | WB55 has no Wi-Fi (BLE + 802.15.4 only). Needs an ESP32 module or gateway — location-bound. |
| **LTE-M cellular modem on the board** | **The only option that is truly live from anywhere.** |

**Current choice: main phone + board history (§5) first**, because it's free and needs no
new hardware. The modem (§6) is the upgrade path if live-from-anywhere is needed.

Server: **ThingSpeak** free tier — non-commercial, 3 M messages/year, 4 channels, **at
most one update per 15 s** → batch uploads. FTP was rejected (file-oriented, plain-text
passwords, not reachable from browsers/apps).

### 4. BH1750FVI (lux)

Commands: `0x01` power on, `0x20` one-shot high-res; wait **180 ms max**, read 2 bytes
MSB-first, `lux = raw / 1.2`. Saturates ~54,612 lx. ESS has Illuminance 0x2AFB (uint24,
0.01 lux) — round to whole lux, the sensor isn't more precise.

### 5. History buffer + history BLE service — implemented and verified on hardware

Board stores each measurement in a RAM ring buffer; the phone requests everything after
the last sequence number it has and the board streams it as notifications.

- Buffer: `HIST_CAPACITY` 2016 records × 16 bytes = 32 KB (14 days at 10 min). 192 KB
  RAM available to the app. Lost on reset/power loss (flash storage is a later option).
- Each record stores the board's **uptime in seconds**; no clock on the board. The phone
  converts using the end marker's uptime: `time = phone_now − (marker.up_s − rec.up_s)`.
- Uptime currently from `HAL_GetTick()` with wrap handling. **Only valid while
  `CFG_LPM_SUPPORTED` is 0** (true while tracing is on); SysTick stops in Stop mode, so
  replace with an RTC-based count before enabling low power. A compiler warning flags this.
- A cycle where the BME680 does not answer adds no record. Sequence numbers stay continuous, so the
  gap shows as a jump in uptime.

**Service** UUID `7a1e0001-5c2b-4f6e-9b3a-1d2c3e4f5a6b` (byte 12 of the little-endian
array selects: `01` service, `03` control, `04` data):

| Characteristic | UUID | Properties | Content |
|---|---|---|---|
| Control | `7a1e0003-…` | Write, 4 bytes | uint32 LE: first sequence number wanted (0 = all) |
| Data | `7a1e0004-…` | Notify, 16 bytes | one record per notification |

Record, little-endian, 16 bytes: bytes 0–3 sequence (`0xFFFFFFFF` = end marker) · 4–7 uptime s ·
8–9 temperature 0.01 °C signed · 10–11 RH 0.01 % · 12–15 pressure Pa. In the end marker,
bytes 12–15 hold the board's next sequence number — lower than the phone's last one means the
board rebooted, so request from 0.

Control takes a 4-byte write; any other length is ignored. A request is ignored until
notifications are enabled on `…0004…` (PuTTY: `HIST: request ignored, notifications not enabled`).
Streaming stops on disconnect, and the board forgets the notification setting: **enable
notifications on `…0004…` again after every reconnect**, then write the request.
In nRF Connect the first byte written is the lowest one: sequence 157 is `9D-00-00-00`.

**Integration points:**
- `app_conf.h`: add `CFG_TASK_HIST_SEND_ID` to the with-HCI-command task list.
- `p2p_server_app.h` (`USER CODE BEGIN EF`): `void HIST_OnGattEvent(uint16_t ecode, void *p_data);`
- `app_ble.c` (`USER CODE BEGIN BLUE_EVT`): forward `ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE`
  and `ACI_GATT_TX_POOL_AVAILABLE_VSEVT_CODE` to `HIST_OnGattEvent(p_blecore_evt->ecode,
  p_blecore_evt->data)`. (`evt_blecore_aci` is not visible in `p2p_server_app.c`, so the two
  fields are passed instead of the struct.)
- `p2p_server_app.c`: `HIST_Init()` (service + chars; control char uses
  `GATT_NOTIFY_ATTRIBUTE_WRITE`), `HIST_Add()` called from `ENV_Read_Task`,
  `HIST_Send_Task()` sends until `aci_gatt_update_char_value` returns
  `BLE_STATUS_INSUFFICIENT_RESOURCES`, then resumes on the TX-pool-available event;
  sends the end marker last. Stop streaming on disconnect (`PEER_DISCON_HANDLE_EVT`).
- Written value handle = char handle + 1; CCCD = char handle + 2.

**Test before any app:** nRF Connect → enable notify on `…0004…` → write byte array
`00000000` to `…0003…` → PuTTY shows `HIST: request, sending seq 0 to N (N records)`, `HIST: done`;
phone receives 16-byte values, last one starting `FFFFFFFF`. If PuTTY shows nothing on
the write, the event isn't reaching `HIST_OnGattEvent`.

**Verified on hardware (2026-10-04, 16-byte build, nRF Connect + PuTTY):**
- Request from sequence 157: 30 records (157 to 186) in order, then the end marker
  (uptime 1880 s, next sequence 187). Record values matched the live ESS readings.
- Full transfers (`00-00-00-00`): 275 and 329 records, no gaps or repeats, marker last. The
  second transfer's first 275 records were byte-for-byte identical to the first.
- Request after a disconnect and reconnect works once notifications are enabled again; without
  that step the request is ignored, as designed.
- After a reflash/reset the buffer is empty and the sequence restarts at 0; the marker's next
  sequence (lower than before) and uptime show this.
- Disconnect in the middle of a transfer, using a temporary test build that paused each transfer
  after 10 records for 350 to 360 s (not part of the repo): nothing was sent after the disconnect,
  measurements continued, and a new request restarted cleanly from 0, also while an earlier pause
  was still pending. Staying connected through the pause: the remaining records (10 to 36) and the
  marker arrived 355 s after the request, with a contiguous sequence.

**Not yet verified on hardware:** the ring buffer wrapping at 2016 records (checked only in the
off-target test), whether the wait-for-buffer path (`BLE_STATUS_INSUFFICIENT_RESOURCES`) is ever
reached on the board (the firmware does not log it), runs at `ENV_PERIOD_S` 600, and low-power mode
(uptime needs an RTC-based count).

### 6. Android app (after §5 works)

Battery-efficient design (phone mostly on battery):
- Don't scan continuously. Sync when the board is in range, a few times a day or on a
  periodic wake (Android allows waking from deep sleep about every 9 min).
- Connect → enable notifications → write start sequence → collect → disconnect.
- Store locally, upload in **batches** (each 4G transmission keeps the radio in high power
  for several seconds; batching saves most of the cost). ThingSpeak bulk upload.
- Exempt the app from battery optimisation; MIUI phones also need Autostart allowed.
- Bluetooth scanning on older Android needs Location switched on.
- Existing no-code option checked: **Theengs** app (BLE → MQTT) — its background mode is
  experimental and unreliable per its own developers. Not suitable unattended.

### 7. Cellular modem — if live-from-anywhere is required

- Choose **LTE-M / NB-IoT**, not 2G (2G modules like SIM800L: ~2 A transmit peaks, and
  Romanian operators are retiring old networks; Vodafone RO lists 2G shutdown as planned).
- Candidate: **Waveshare Pico-SIM7080G** (~$35, antennas included, Li-Po charger, 3.3 V
  UART). Alternative with 2G fallback: Waveshare SIM7000E HAT (~$49).
- **Orange and Vodafone Romania support LTE-M** (NB-IoT per operator not confirmed).
  Ordinary prepaid SIMs are often **not** enabled for LTE-M — ask, or use an IoT SIM
  (1NCE, Onomondo).
- UART: **LPUART1 (PA2/PA3) is free** — configured in the `.ioc` but never started
  (trace uses USART1). Believed to be Arduino D1/D0; confirm in UM2819.
- Power the modem from its own Li-Po, not the Nucleo 3.3 V rail.
- First prove SIM, coverage and the ThingSpeak request by typing AT commands from a PC via
  a USB-serial adapter — before writing firmware.
- Firmware: non-blocking AT state machine (interrupt RX buffer, sequencer tasks); send
  every 10–30 min; modem power-save mode between sends. Keep BLE for local access.

### 8. Battery

- **The Nucleo itself is not low-power**: the on-board ST-LINK and LEDs draw constantly,
  far more than the WB55. Real battery life needs powering only the WB55 part (UM2819) or
  a minimal board.
- **Tracing forces low-power mode off** (`CFG_DEBUG_TRACE != 0` → `CFG_LPM_SUPPORTED 0`).
  Disable traces before measuring current.
- Many power banks switch off at low load — exactly what a sleeping station draws.

---

## Code change kept from setup: advertising no longer expires

`Adv_Cancel()` in `STM32_WPAN/App/app_ble.c`: after the stock 60 s fast advertising ends,
`Adv_Request(APP_BLE_LP_ADV)` is called in `USER CODE BEGIN Adv_Cancel_2`, **after** the
`aci_gap_set_non_discoverable()` block — calling it before that block silently stops all
advertising. LP interval macros: `CFG_LP_CONN_ADV_INTERVAL_MIN/MAX` (1–2.5 s).

---

## Gotchas worth re-reading before debugging

- **Readings that change with no code or wiring change → suspect the mechanics first.**
- Trace output goes to USART1 via DMA queue; trace code must run after `MX_APPE_Init()`
  (i.e. in the main loop or tasks, never in `USER CODE BEGIN 2`). `\r\n` line endings.
  `#include "dbg_trace.h"` in `main.c`.
- **Bonding** survives reflashing the M4. One-sided bond deletion → connections drop
  immediately. Clear both sides. Consider `CFG_BONDING_MODE 0`.
- Build errors about `BSP_LED_*`: `.extSettings` not applied. Missing `..\..\..\..\Drivers`
  headers: project was copied into the workspace (must stay in place).
- Windows 260-char path limit can cause "no such file" build errors.
- Relative humidity depends on the sensor's own temperature: a warmed sensor reads dry.
- Pasting or typing into PuTTY sends the text to the board. Its stock command parser answers
  "NOT RECOGNIZED COMMAND" for anything except exactly `SW1`, `SW2`, `SW3`, which simulate the
  buttons (`SW2` clears the pairing).
- `ENV: BME680 trigger failed, i2c err 0x04` (NACK) was seen once with the header unsoldered.
  The cycle is skipped and the next one works; expect occasional gaps until the header is soldered.
- nRF Connect caches the service list. After flashing new services, refresh or toggle Bluetooth.
