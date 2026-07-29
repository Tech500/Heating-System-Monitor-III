# Outside Node – Nordic Power Profiler Kit 2 Measurements

This page documents the four PPK2 captures taken from the EoRa‑S3‑900TB outside node.  
Each image shows a different operating mode of the ESP32‑S3 + SX1262 system.

---

## 1. ESP32S3 + SX1262 DeepSleep Baseline (14.59 µA)

The node is fully asleep:  
- ESP32‑S3 in deep sleep  
- SX1262 in sleep  
- All peripherals off  

[DeepSleep Baseline](https://github.com/Tech500/Heating-System-Monitor-IV/blob/main/IMAGES/PPK2%20Observation-01.png)
---

## 2. SX1262 Auto Duty Cycle RX Window (~14 mA Peak)

Captured during `radio.startReceiveDutyCycleAuto()`:

- SX1262 wakes  
- RX window opens  
- Radio returns to sleep  

[Auto RX Window](https://github.com/Tech500/Heating-System-Monitor-IV/blob/main/IMAGES/PPK2%20Observation-02.png)

---

## 3. Full Receive Duty Cycle Sequence  
**Sleep → Auto RX Window → Sleep**

A complete scheduled receive cycle:

- ESP32‑S3 wakes  
- SX1262 performs RX window  
- Node returns to baseline  

[Duty Cycle Sequence](https://github.com/Tech500/Heating-System-Monitor-IV/blob/main/IMAGES/PPK2%20Observation-03.png)

---

## 4. EoRaS3900TB Full Wake + Radio Activity Cycle (ESP32S3 + SX1262)

This is the node’s full active period:

- ESP32‑S3 wake  
- SX1262 activity  
- Peripherals/LDOs active  
- Return to deep sleep  

Peak ≈ 98 mA  
Average ≈ 25–26 mA  

[Full Wake Cycle](https://github.com/Tech500/Heating-System-Monitor-IV/blob/main/IMAGES/PPK2%20Observation-04.png)

---

## Notes

- All captures taken at 100 ksps  
- PPK2 connected directly to the EoRa‑S3‑900TB DUT rail  
- These measurements demonstrate real, reproducible behavior for the open‑source community

