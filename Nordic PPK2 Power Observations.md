# Outside Node – Nordic Power Profiler Kit 2 Measurements

This page documents the four PPK2 captures taken from the EoRa‑S3‑900TB outside node.   
Each image shows a different operating mode of the ESP32‑S3 + SX1262 system.

## 1. ESP32S3 + SX1262 Project Average Current

The node is fully asleep:

- ESP32‑S3 in deep sleep
- SX1262 in sleep
- All peripherals off

[Project Average Current](Images/Project%20Average%20Current.png)

---

## 2. SX1262 One Complete Auto Duty Cycle RX Window (5000 Symbols)

Captured during `radio.startReceiveDutyCycleAuto()`:

- SX1262 wakes
- RX window opens
- Radio returns to sleep
- Ebyte, EoRa-S3-900TB Deep Sleeps ~ 99.9% of time

Selection (49.36 µA avg, 1.041 s, 51.42 µC) isolates just the sleep-floor
between RX windows — excludes the wake/RX spike.

[Deep Sleep Duration](Images/Sleep%20Duration.png)

---

## 3. WOR Spike and Deep Sleep

A complete scheduled receive cycle:

- ESP32‑S3 wakes
- SX1262 performs RX window
- Node returns to baseline

[WOR Spike and Deep Sleep](Images/WOR%20Spike%20and%20Deep%20Sleep.png)

---

## 4. EoRa-S3-900TB WOR Spike Duration

This is the node's full active period:

- ESP32‑S3 wake
- SX1262 activity
- Peripherals/LDOs active
- Return to deep sleep

Peak ≈ 98 mA  
Average ≈ 25–26 mA

[WOR Spike Duration](Images/WOR%20Spike%20Duration.png)

---

## Notes

- All captures taken at 100 ksps  
- PPK2 connected in Ampere Mode to the EoRa‑S3‑900TB, in series with LiPo battery and Development Board, JST Battery connector.  
- These measurements demonstrate real, reproducible behavior for the open‑source community
- Sleep single duration happens multiple times during the time in Deep Sleep between LoRa WOR.  Trigger time is unknown; event driven, depends on how long blower runs
before cycling to OFF.
- Grey shaded area are the vaues of interest.
- PPK2 Images are re-derive the baseline number from a stopped/static capture with an explicit SELECTION over a clean deep-sleep-only stretch (excluding WOR spikes), so it's apples-to-apples with the others.




