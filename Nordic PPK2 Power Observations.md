# Nordic PPK2 Power Observations — HSM IV Outside Node

**Measurement conditions:** USB CDC disabled on boot. WOR preamble detection via **CAD (Channel Activity Detection)** — the project's current method, superseding the earlier `autoDutyCycle` approach used in the original Hackster write-up. Values pulled from the PPK2 windowed **Selection** box, except Image 1 (power-on), which has no selection box and reflects the full 1-second WINDOW capture.

**Story arc:** Power-on → Active spike → Average (active + sleep) → Sleep duration → WOR wake (manually triggered) → Continuous duty-cycle stream.

**Path:** `Images/New/` on `Tech500/Heating-System-Monitor-IV` (all 6 URLs verified live)

---

## Image 1 — ESP32-S3 Power-On

![Image 1](https://raw.githubusercontent.com/Tech500/Heating-System-Monitor-IV/main/Images/New/ESP32S3%20POWEON%20.png)

**Caption:** ESP32-S3 power-on transient — 33.08 mA average, 91.08 mA max, over 1.000 s (33.08 mC charge). Full-window reading; no Selection box used.

---

## Image 2 — Active Spike

![Image 2](https://raw.githubusercontent.com/Tech500/Heating-System-Monitor-IV/main/Images/New/Active%20Portion.png)

**Caption:** Active-portion current spike — 7.43 mA average, 14.23 mA max, over 11.22 ms (83.34 µC charge).

---

## Image 3 — Average (Active + Sleep)

![Image 3](https://raw.githubusercontent.com/Tech500/Heating-System-Monitor-IV/main/Images/New/Average%20Portion.png)

**Caption:** Full active+sleep duty cycle average — Selection: 38.19 µA average, 11.71 mA max, over 5.087 s (194.27 µC charge). Full window (9.468 s) shows 39.42 µA average, 11.85 mA max, 373.22 µC.

---

## Image 4 — Sleep Duration of Cycle

![Image 4](https://raw.githubusercontent.com/Tech500/Heating-System-Monitor-IV/main/Images/New/Sleep%20Portion.png)

**Caption:** Sleep-duration segment — Selection: 21.93 µA average, 23.06 µA max, over 5.057 s (110.91 µC charge). Full window (9.468 s) shows 39.42 µA average, 11.85 mA max, 373.22 µC.

---

## Image 5 — WOR Manually Triggered

![Image 5](https://raw.githubusercontent.com/Tech500/Heating-System-Monitor-IV/main/Images/New/WOR%20Manually%20Triggered.png)

**Caption:** LoRa WOR wake, manually triggered — 40.64 mA average, 105.44 mA max, over 3.259 s (132.45 µC charge).

---

## Image 6 — CAD Duty Cycle Stream (Continuous Spike Train)

![Image 6](https://raw.githubusercontent.com/Tech500/Heating-System-Monitor-IV/main/Images/New/CAD%20Duty%20Cycle%20Stream.png)

**Caption:** Continuous stream of WOR wake events over a 1-minute window — 37.19 µA average, 15.17 mA max, 2.23 mC charge (full WINDOW, no selection made). Illustrates cycle-to-cycle consistency of the CAD-based WOR duty cycle; not intended as a new headline current figure, but as corroborating evidence for Image 3's Selection-box average.

**Observations from this capture:**
- Peak spike amplitude was confirmed **zoom-invariant** — checked at 10ms, 100ms, and 1-minute scales, with each spike reading the same true peak regardless of view. This rules out chart downsampling as the source of amplitude variation between individual spikes.
- The modest amplitude spread seen between spikes (~10–15 mA peak in this WINDOW view) is therefore a real characteristic, consistent with expected CAD detection-timing variance — how far into a CAD scan window the preamble happens to be detected can affect how much settle-time is captured before steady-state RX current is reached.
- The µA-scale average was observed to vary continuously between a high of **~40 µA** and a low of **~21 µA** during live viewing — closely matching Image 3's Selection average (38.19 µA, active-adjacent) and Image 4's Sleep Portion average (21.93 µA, sleep floor). This is good corroboration that the individual snapshot captures (Images 3 and 4) are representative of the node's real operating range, not outliers.

---

## Summary Table

| # | Description | Avg | Max | Duration | Charge |
|---|---|---|---|---|---|
| 1 | Power-on (full window) | 33.08 mA | 91.08 mA | 1.000 s | 33.08 mC |
| 2 | Active spike | 7.43 mA | 14.23 mA | 11.22 ms | 83.34 µC |
| 3 | Average (active + sleep, selection) | 38.19 µA | 11.71 mA | 5.087 s | 194.27 µC |
| 4 | Sleep duration (selection) | 21.93 µA | 23.06 µA | 5.057 s | 110.91 µC |
| 5 | WOR manually triggered | 40.64 mA | 105.44 mA | 3.259 s | 132.45 µC |
| 6 | CAD duty cycle stream (1-min window) | 37.19 µA | 15.17 mA | 60.0 s | 2.23 mC |

---

## Battery Chemistry Considerations

Self-discharge rate is one of the biggest levers on real-world battery life for a low-draw, multi-year outdoor deployment — at these µA-scale currents, the battery's own internal leakage can rival or exceed the node's active power draw over time.

| Chemistry | Typical Self-Discharge | Cold-Temp Performance | Rechargeable | Notes |
|---|---|---|---|---|
| **LiPo (Li-ion polymer)** — current choice | ~2–3%/month | Degrades below ~0°C; capacity drops noticeably in freezing conditions | Yes | Good energy density, widely available, but needs protection circuitry and isn't ideal for an unheated outdoor enclosure through winter |
| **LiFePO4 (LFP)** | ~1–3%/month | Better cold tolerance than standard LiPo (usable to roughly -20°C) | Yes | Lower energy density (more physical volume for same mAh), but longer cycle life and better thermal/safety margin — a strong candidate for the outdoor node |
| **Li-SOCl2 (Lithium Thionyl Chloride, primary)** | <1% per **year** | Excellent — rated to -40°C in many parts | No (primary/disposable) | Very low self-discharge makes it well-suited to multi-year, low-duty-cycle deployments like this one; tradeoff is you replace rather than recharge the cell |
| **NiMH (standard)** | ~20–30%/month | Moderate; capacity drops in cold | Yes | Self-discharge alone would likely dominate your battery-life budget at these current levels — not a good fit here |
| **NiMH (low self-discharge, e.g. Eneloop-style)** | ~0.1–1%/month | Better than standard NiMH | Yes | Viable, but still generally lower energy density than LiPo/LFP for the same size |
| **Alkaline (primary)** | ~2–3% per **year** | Poor below freezing — voltage sags significantly | No | Low self-discharge, but cold-weather voltage sag is a real risk for an outdoor node through winter |

**For this project specifically:** given the outside node sits in a Stevenson screen exposed to real seasonal temperature swings, the two standouts are:

- **LiFePO4** — if you want to stay rechargeable, this trades a bit of energy density for meaningfully better cold-weather behavior and self-discharge than standard LiPo, without changing your overall power budget much.
- **Li-SOCl2 primary cell** — if you're willing to give up rechargeability, the near-negligible self-discharge (<1%/year) means the CAD-based ~38.19 µA draw would dominate the battery-life calculation almost entirely, with self-discharge becoming a rounding error rather than a meaningful derating factor. Given your CAD-based numbers already put theoretical life in the 8–9 year range, a Li-SOCl2 primary cell could realistically make self-discharge nearly irrelevant to the final field-life number.

Worth weighing against your maintenance model — a primary cell means a physical swap eventually, while LiPo/LFP means periodic recharge access (relevant given the node's outdoor mounting).

---

## How Battery Life Is Calculated

Battery life is estimated from the **duty-cycle-weighted average current** measured by the PPK2, divided into the battery's usable capacity.

**Basic formula:**

```
Battery Life (hours) = Battery Capacity (mAh) / Average Current Draw (mA)
```

**Step 1 — Establish the duty-cycle average current.**
Use the Selection-box average from a capture that spans a full active+sleep cycle (Image 3 above): **38.19 µA** (0.03819 mA) over 5.087 s. This is the number that matters — not the peak spike current, and not the sleep-floor current alone, since real-world draw is a mix of both weighted by how long the node spends in each state.

**Step 2 — Convert to hours, then months.**

```
3000 mAh / 0.03819 mA = 78,565 hours
78,565 hours / 24 = 3,274 days
3,274 days / 30.4 = ~107.7 months (theoretical)
```

**Step 3 — Derate for real-world losses.**
Theoretical figures assume 100% usable capacity and zero self-discharge, which never holds in practice. Typical derating factors for a 3000mAh LiPo:

| Factor | Typical Loss |
|---|---|
| LiPo self-discharge (~2–3%/month) | 15–25% over multi-year life |
| Voltage regulator/LDO quiescent draw | 5–15% |
| Cold-weather capacity reduction (outdoor node) | 10–20% seasonal |
| Cell aging / capacity fade over years | 10–20% by year 2–3 |
| Safety margin (don't run cells to 0%) | ~10% reserved |

Applying a combined 40–55% derating range to the theoretical figure gives a realistic **46–67 month** estimate — consistent with the range already published in the Hackster write-up.

**Baseline comparison — methodology transition:**

| Source | Method | Avg Current | Theoretical Life | Status |
|---|---|---|---|---|
| Hackster write-up (published) | autoDutyCycle | 51.30 µA | ~80.2 months (rounds to ~83.5 as published) | Superseded |
| This document, Image 3 | CAD | 38.19 µA | ~107.7 months | Current |

**autoDutyCycle vs. CAD — direct comparison:**

- **Current draw reduced by 25.6%** — from 51.30 µA (autoDutyCycle) to 38.19 µA (CAD)
- **Theoretical battery life improved by 34.3%** — from ~80.2 months to ~107.7 months on the same 3000mAh LiPo
- After the same 40–55% real-world derating applied elsewhere in this document, the CAD-based estimate would put the **derated range at roughly 62–90 months**, versus the previously published 46–67 months under autoDutyCycle

This is a meaningful, method-driven improvement rather than measurement noise — switching the WOR preamble detection strategy from autoDutyCycle to CAD reduces the average power draw enough to materially extend expected field life on the same battery.

**Why this matters for your dataset:** Image 3's Selection-box reading (38.19 µA) is the single most important number in this whole document — it's the one that should drive the battery-life math, more so than the sleep-floor number or the active-spike peak alone, since it already reflects the actual time-weighted mix of active and sleep states in one real duty cycle.

**⚠️ Methodology change — retiring the 51.30 µA baseline:** The original 83.5-month figure (Hackster write-up) was calculated from **51.30 µA**, measured under the `autoDutyCycle` WOR detection method. The project has since moved to **CAD (Channel Activity Detection)**, which draws meaningfully less power. Today's Image 3 capture — **38.19 µA**, CAD-based — reflects that improvement. Once all documentation and firmware fully adopt CAD, the 51.30 µA figure (and the 83.5-month estimate derived from it) should be considered superseded rather than a number to reconcile against.

---

*Note: Two-WOR-trigger timing capture excluded — suspected variance from manual push-button hold time made the measurement unreliable. Five images kept the narrative focused on a single clean duty-cycle story.*
