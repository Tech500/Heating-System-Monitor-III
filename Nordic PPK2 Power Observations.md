# Outside Node – Nordic Power Profiler Kit 2 Measurements

This page documents the four PPK2 captures taken from the EoRa‑S3‑900TB outside node.  
Each image shows a different operating mode of the ESP32‑S3 + SX1262 system.

---

## 1. ESP32S3 + SX1262 DeepSleep Baseline Current

The node is fully asleep:  
- ESP32‑S3 in deep sleep  
- SX1262 in sleep  
- All peripherals off  

[Project Average Current](https://github.com/Tech500/Heating-System-Monitor-IV/blob/main/Images/Project%20Aveage%20Current.png)
---

## 2. SX1262 Auto Duty Cycle RX Window (5000 Symbols)

Captured during `radio.startReceiveDutyCycleAuto()`:

- SX1262 wakes  
- RX window opens  
- Radio returns to sleep  

[Sleep Deep Duration](https://github.com/Tech500/Heating-System-Monitor-IV/blob/main/Images/Sleep%20Duration.png)

---

## 3. Full Receive Duty Cycle Sequence  
**Sleep → Auto RX Window → Sleep**

A complete scheduled receive cycle:

- ESP32‑S3 wakes  
- SX1262 performs RX window  
- Node returns to baseline  

[WOR Spike and Deep Sleep](https://github.com/Tech500/Heating-System-Monitor-IV/blob/main/Images/WOR%20Spike%20and%20Deep%20Sleep.png)
---

## 4. EoRa-S3-900TB Full Wake + Radio Activity Cycle (ESP32S3 + SX1262)   

This is the node’s full active period:

- ESP32‑S3 wake  
- SX1262 activity  
- Peripherals/LDOs active  
- Return to deep sleep  

Peak ≈ 98 mA  
Average ≈ 25–26 mA  

[WOR Spike Duration](https://github.com/Tech500/Heating-System-Monitor-IV/blob/main/Images/WOR%20Spike%20Duration.png)

---

## Notes

- All captures taken at 100 ksps  
- PPK2 connected directly to the EoRa‑S3‑900TB DUT rail  
- These measurements demonstrate real, reproducible behavior for the open‑source community

