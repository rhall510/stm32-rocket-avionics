# STM32-based rocket avionics unit and active tracking ground station

![Status](https://img.shields.io/badge/Status-Early_development-yellow) ![License](https://img.shields.io/badge/License-MIT-blue)

> **⚠️ NOTE: This project is currently in early active development.**
> See the [project plan](docs/PROJECT_PLAN.md) for a detailed roadmap and the [development log](docs/DEVELOPMENT_LOG.md) for regular progress updates.

---

## Overview
This project aims to build a robust avionics unit for high power model rocketry. This system will also include a ground station that mechanically rotates high gain antennas to actively track the rocket in real time during flight and receive critical telemetry data.

The system relies on sensor fusion for onboard state estimation and uses a hybrid RF tracking method combining position/movement telemetry and Phase Difference of Arrival (PDOA) signal analysis.

## Key features
* **Dual-band telemetry:**
    * **2.4GHz (SX1280):** High bandwidth telemetry and command link.
    * **868MHz (SX1262):** Beacon signal for precise PDOA angle tracking.
* **Active ground tracking:** Motorized base with PID control tracks the rocket's azimuth and elevation automatically.
* **Live updating GUI:** See all relevant statistics and flight path updates in real time.
* **Advanced firmware:** Built on STM32 using FreeRTOS for concurrent sensor data acquisition, data logging, RF transmission, and parachute deployment.
* **Safety first:** Robust pre-flight check routine, redundant parachute deployment logic, and failsafe state management.


## System architecture
The project is divided into three main subsystems:

1.  **Avionics (Rocket):** Onboard PCB with flight sensors, NAND flash logging, and RF transceivers controlled by a STM32 NUCLEO-G474RE microcontroller.
2.  **Ground station:** Antenna array with motorised tracking base utilizing position telemetry and PDOA data to maintain a high gain link with the rocket.
3.  **Control center:** PC based GUI for live data visualization and controlling the ground station and avionics unit.

```mermaid
---
config:
  look: neo
  theme: neo-dark
---
flowchart TB
 subgraph Rocket["Avionics Unit"]
        MCU_R["MCU<br>STM32G474"]
        Sensors["Sensors<br>IMU, GPS, Baro"]
        NAND["NAND flash"]
        RKT_24["SX1280<br>2.4GHz telemetry"]
        RKT_868["SX1262<br>868MHz beacon"]
        Parachute["Parachute<br>deployment"]
  end
 subgraph Air["Ground station"]
        GS_24["Ground RX<br>SX1280"]
        GS_PDOA["PDOA array<br>AD8302"]
        MCU_G["MCU<br>STM32G431"]
        Motors["Stepper/Servo<br>tracker base"]
  end
 subgraph PC["Control center"]
        GUI["Qt GUI<br>Data display and<br> mission control"]
  end
    Sensors -- I2C --> MCU_R
    MCU_R -- SPI --> RKT_868
    MCU_R <-- SPI --> RKT_24
    MCU_R -- GPIO --> Parachute
    MCU_R -- SPI --> NAND
    RKT_24 -. Telemetry .-> GS_24
    GS_24 -. Cmd .-> RKT_24
    RKT_868 -. CW tone .-> GS_PDOA
    GS_24 <-- SPI --> MCU_G
    GS_PDOA -- Analog phase --> MCU_G
    MCU_G -- GPIO/PWM --> Motors
    MCU_G <-- UART --> GUI
```

## Repository structure
* `docs/`: Progress logs, planning documents, and testing data.
* `firmware/`: Source code for the avionics and ground station MCUs as well as other prototyping code.
* `hardware/`: KiCad schematics and PCB layouts.
* `mechanical/`: Model files for making various non-electrical parts of the project.
* `software/`: Source code for the control center GUI application.

## Current status
Goals from the [project plan](docs/PROJECT_PLAN.md) that are currently being worked on:
##### 1\. RF link validation and range tests
* [ ] Assemble SX1262 (LAMBDA62 module - 868MHz) and SX1280 (LAMBDA80 module - 2.4GHz) test rigs with 1/4 wave ground plane monopole antennas (driven by Arduinos while STM32s are on backorder) and perform range tests to validate feasibility and collect field data for RSSI and SNR at distance.

##### 2\. Avionics unit
* [ ] Acquire STM32 NUCLEO-G474RE, NAND flash chip (W25N01GV), and sensors - IMU (LSM6DSO), high range accelerometer (ADXL375) pressure and temperature (BMP390), magnetic heading (QMC5883L), GPS (MAX-M10S-00B).
* [ ] Design avionics unit development PCB with convenience features like line test pads, 0 ohm resistors, spaced out components etc. Assemble all components on to it. STM32 and transceivers are used as modules plugged into the PCB (later to be soldered permanently) while all sensors and NAND flash are used as bare chips soldered directly to the PCB via hotplate reflow. Also include power management IC and safety arming switch for parachute deployment system.

*For an up to date breakdown of progress, check the [development log](docs/DEVELOPMENT_LOG.md).*
