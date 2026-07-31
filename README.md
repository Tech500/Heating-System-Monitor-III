# Heating System Monitor IV

**AI collaboration: Claude (Anthropic) — lead, with GitHub Copilot, ChatGPT (OpenAI), and Gemini (Google).**

No electrical connection to the heating system — monitors temperatures and blower runtime
using vibration detection only. Works in both cooling and heating seasons — year round!

---

## Why use Heating System Monitor IV?

**Answer questions your thermostat can't.** A thermostat knows the setpoint. HSM IV knows
what the system actually *did*: how many minutes the blower ran today, how long each cycle
lasted, how long the house held temperature between cycles, and what the indoor and outdoor
conditions were at the time. Over days and weeks, that data answers real questions:

- Is my system short-cycling?
- Which thermostat setpoint gives long, efficient cycles instead of frequent short ones?
- How does runtime track outdoor temperature? (Runtime per degree-day is your building's
  thermal signature — a sudden change flags a filter, refrigerant, or duct problem before
  the utility bill does.)
- Is the system degrading over time?

**No electrical connection to your HVAC system.** Blower detection is purely mechanical —
an MPU-6050 IMU sensing vibration from outside the blower enclosure. Nothing is wired into
the furnace or air handler. Nothing voids a warranty, nothing touches line voltage, and the
whole monitor installs and removes without a trace — ideal for renters and apartments.

**Owns its data.** Every event is logged twice: locally to the ESP32's flash (LittleFS,
retrievable over FTP) and to a perpetual Google Sheet that rolls month-to-month and
year-to-year automatically. No cloud subscription, no vendor account, no app.

**Survives real-world conditions.** Runtime totals persist through power failures via
NVS flash storage. A reset-reason log distinguishes normal power cycles from brownouts and
watchdog resets. If the outdoor sensor node goes silent, its columns log "Offline" instead
of silently repeating stale data. The outside node itself runs for years on battery power
alone — see **Power Optimization** below.

---

## Overview

Heating System Monitor IV is a three-node system combining **LoRa** (outside node ↔ receiver)
and **ESP-NOW** (blower node ↔ receiver) for wireless communication.

**How blower detection works (variance-based thresholding):**
Blower state is detected with an MPU-6050 6-axis IMU attached to the outside of the blower
enclosure. The sketch computes the statistical variance of accelerometer samples over a
short window. A running blower produces mechanical vibration (high variance); a stopped
blower produces almost none (low variance). Hysteresis between the ON and OFF thresholds
prevents chatter at the transitions. No electrical hookup to the heating/cooling system is
required, and no microphone is involved — it is immune to room noise.

**Logged data includes:** NTP timestamp, outside temperature, inside temperature, inside
humidity, thermostat setpoint, elapsed blower minutes (per cycle), daily total blower
minutes, outside pressure, inside pressure, pressure difference (out − in), cycles today,
coast minutes (hold time between cycles), and average cycle minutes.

Each record is written to both a local LittleFS log file and a perpetual Google Sheet
(month-to-month, year-to-year).

[Inspiration for the perpetual Google Sheet](https://iotdesignpro.com/articles/esp32-data-logging-to-google-sheets-with-google-scripts)

---

## Cycle tracking (thermostat optimization)

The receiver tracks blower ON/OFF transitions to build per-cycle statistics:

| Metric            | Meaning                                                        |
|-------------------|------------------------------------------------------------------|
| `cyclesToday`     | Blower starts since midnight                                   |
| `coastMinutes`    | How long the house held temperature before this cycle started  |
| `avgCycleMinutes` | Daily total runtime ÷ cycles today                              |

Comparing these across different thermostat setpoints (held for a few days each,
normalized by inside–outside temperature difference) reveals the setpoint that yields
long, lazy cycles — the efficient operating point — instead of frequent short-cycling.
`coastMinutes ÷ (coastMinutes + elapsedMinutes)` per row gives duty cycle directly.

---

## Hardware

### Receiver Node (`ESP_NOW__Receiver`)
- **EoRa-S3-900TB** (ESP32-S3 + SX1262 LoRa), mains-powered
- BME280 sensor — inside temperature, humidity, barometric pressure (I²C on GPIO48/47)
- Write-status LED on GPIO23 — ON while flash writes are in progress ("safe to pull power" indicator)
- Connects to Wi-Fi; posts each record to Google Sheets over HTTPS
- FTP server for retrieving LittleFS log files
- NVS (Preferences) state persistence: daily runtime and cycle count survive power loss
- Reset-reason logging to `/reset_log.txt`
- Daily totals appended to `/daily_totals.csv` at the nightly rollover
- Receives outdoor readings via LoRa from the outside node; receives blower events via ESP-NOW

### Outside Node (`ESP_NOW_BME280`)
- **EoRa-S3-900TB** (ESP32-S3 + SX1262 LoRa), battery-powered (3000 mAh LiPo)
- BME280 sensor — outside temperature, humidity, barometric pressure (I²C on GPIO48/47)
- Housed in a Stevenson screen
- Deep sleep between LoRa duty-cycle sniff windows; wakes on a long-preamble
  Wake-on-Radio (WOR) trigger sent from the hub — see **Power Optimization** below
- Uses EBYTE's `boards.h` and `utilities.h` board support files (included in the
  `ESP_NOW_BME280/` sketch folder) for EoRa-S3-900TB pin mappings and board config

Two EBYTE EoRa-S3-900TB boards are required to achieve the Outside Node's microamp results.
[EoRa-S3-900TB — available from EbyteIoT.com](https://ebyteiot.com/products/ebyte-oem-odm-eora-s3-900tb-22dbm-7km-mini-low-power-and-long-distance-sx1262-rf-module-lora-module-915mhz?_pos=1&_sid=af52219e9&_ss=r)

### Blower Node (`ESP_NOW_Blower`)
- ESP32-S3 Super Mini module
- MPU-6050 IMU — vibration-based blower detection, mounted on the blower enclosure
- NVS persistence of daily total across power cycles
- Communicates to the receiver via ESP-NOW

---

## Power Optimization (Outside Node)

The outside node underwent extensive power profiling using a Nordic PPK2 to minimize deep-sleep
current while maintaining reliable LoRa wake capability.

**Key findings and fixes:**

- **DC-DC regulator mode** — switched from LDO to DC-DC on the SX1262 for lower quiescent current.
- **Explicit peripheral shutdown before deep sleep** — added `shutdownWiFi()`, `shutdownEspNow()`,
  and `shutdownI2C()` calls. Diagnosed a **60 mA current stall** traced to peripherals (WiFi/ESP-NOW)
  left initialized but unused; explicit teardown resolved it.
- **ESP32 Arduino Core 2.x → 3.3.10 compatibility fix** — legacy Core 2.x-style code compiled
  cleanly under 3.3.10 but caused runtime BME280 read failures (NaN) after wake. No compiler error;
  found via PPK2 profiling and code review.
- **WOR preamble margin fix** — measured actual sniff interval (~1.04 s) was longer than the
  originally assumed 200 ms. Increased the hub's WOR trigger preamble from 4096 → **5000 symbols**
  (~1.28 s), restoring a comfortable ~240 ms wake margin at zero battery cost (hub is mains-powered).

**Measured results (PPK2, post-fix):**

| Metric                                  | Value            |
|------------------------------------------|-------------------|
| Deep sleep floor                         | ~7.3 µA           |
| Sniff spike duration                     | ~3.6 ms           |
| Sniff spike peak current                 | ~11–14 mA         |
| Measured sniff interval                  | ~1.04 s           |
| Average current (full cycle)             | ~50 µA            |
| Projected battery life (3000 mAh LiPo)   | **~80+ months**   |

PPK2 capture screenshots and raw observations: [PPK2 Observations](images/) *(placeholder — populate `images/` folder with captures)*

---

## ESP-NOW Configuration (Blower Node ↔ Receiver)

| Parameter       | Value                                        |
|-----------------|------------------------------------------------|
| Channel         | 0 (follows the receiver's home Wi-Fi channel)  |
| Blower Node MAC | E4:65:B8:20:20:A0                              |
| ESP32 Core      | 3.3.10                                          |
| Wi-Fi Mode      | WIFI_MODE_APSTA                                 |

> **Note:** MAC address shown is an example from this build. Replace it with the actual
> MAC address of your ESP32 module. Each ESP32 has a unique MAC — run a brief sketch
> calling `WiFi.macAddress()` on the board to find it. Channel 0 means the ESP-NOW peer
> uses whatever channel the receiver's Wi-Fi connection lands on, so it stays in sync
> with the home router automatically.

## LoRa Configuration (Outside Node ↔ Receiver)

| Parameter               | Value                              |
|---------------------------|--------------------------------------|
| Spreading Factor         | SF7                                  |
| Bandwidth                | 500 kHz                               |
| TX Power                 | 2 dBm                                 |
| Preamble length (WOR trigger) | 5000 symbols (~1.28 s)          |
| Sync word                | Private                               |
| Regulator mode           | DC-DC                                 |

---

## Google Apps Script Setup

1. In Google Sheets, create a new Google Sheet.
2. Note the **Sheet ID** from the URL:
   `https://docs.google.com/spreadsheets/d/`**`<YOUR_SHEET_ID>`**`/edit`
3. Open the Script Editor: **Extensions → Apps Script**.
4. Delete the default `myFunction()` stub entirely.
5. Copy the full text contents of `Code.gs` from this repository.
6. Paste into the Script Editor.
7. Replace the placeholder Sheet ID in the script with the Sheet ID noted in Step 2.
8. **Save** (Ctrl+S or the save icon).
9. Click **Deploy → New Deployment**.
10. Select type: **Web App**.
11. Set **Execute as:** Me.
12. Set **Who has access:** Anyone.
13. Click **Authorize** → **Advanced** → click your Gmail account → **Allow**.
14. Copy the deployment ID and paste it into the Receiver sketch as the Google Script endpoint.

> **Note:** If you redeploy after changes, create a **New Deployment** each time and update
> the deployment ID in the Receiver sketch to match. (Editing an existing deployment to a
> new version can preserve the ID, but a fresh deployment always works.)

Numeric fields pass through a `numOrText()` helper in `Code.gs`: numbers are stored as
numbers, and the receiver's "Offline" sentinel (sent when the outdoor node fails to reply)
is preserved as text. Sheets functions like AVERAGE skip text cells automatically; in
pandas, load with `na_values=['Offline']`.

---

## Logged Data Format

```
["MM/DD/YYYY HH:MM:SS", outsideTemp, insideTemp, insideHumidity, thermostat,
 elapsedMinutes, dailyTotalMinutes, outsidePressure, insidePressure, pressureDiff,
 cyclesToday, coastMinutes, avgCycleMinutes]
```

Pressures are sea-level corrected (temperature-compensated hypsometric formula) and
reported in inHg. The pressure difference is absolute (both sensors at the same elevation).

---

## Repository Contents

| Folder / File                 | Description                             |
|---------------------------------|--------------------------------------------|
| `ESP_NOW_Blower/`             | Blower detection node sketch            |
| `ESP_NOW_BME280/`             | Outside node sketch (EoRa-S3-900TB, LoRa) |
| `ESP_NOW__Receiver/`          | Receiver / logger / Google Sheets node  |
| [`Schematics/`](Schematics/) | Node wiring of components               |
| [`images/`](images/)          | PPK2 capture screenshots and observations |
| `Code.gs`                     | Google Apps Script for Sheets logging   |
| `Heat System Monitor III.mp4` | Project demonstration video, download   |

> **Note:** `ESP_NOW_BME280/` folder name is a holdover from earlier ESP-NOW-based outside
> node hardware; the node itself now runs on EoRa-S3-900TB over LoRa. Consider renaming to
> reflect the current architecture (e.g. `Outside_Node_LoRa/`) at your convenience.

---

## Dependencies

- Arduino ESP32 Core 3.3.10
- RadioLib (SX1262 LoRa)
- EBYTE `boards.h` / `utilities.h` (EoRa-S3-900TB board support — included directly in the
  `ESP_NOW_BME280/` sketch folder, not a separately installed library)
- BME280 — [Tyler Glenn BME280I2C library](https://github.com/finitespace/BME280/tree/master)
- MPU-6050 — [Electronic Cats MPU6050 library](https://github.com/ElectronicCats/mpu6050)
- LittleFS and Preferences (built into ESP32 Arduino Core)

---

## Credits

Developed by William Lucid (AB9NQ), in collaboration with:

- **Claude (Anthropic)** — lead AI collaborator; firmware architecture, debugging,
  power profiling analysis, and documentation.
- **GitHub Copilot** — in-editor code assistance.
- **ChatGPT (OpenAI)** — general development assistance.
- **Gemini (Google)** — power budget optimization.

---

## License

MIT License — see `LICENSE` for details.
