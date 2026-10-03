# 🚗 CAN-Based Vehicle Monitoring & Driver Assistance System

## 🏗️ System Block Diagram

<p align="center">
  <img src="docs/images/system_block_diagram.png"
       alt="CAN Vehicle Monitoring System Block Diagram"
       width="1000">
</p>
**A distributed automotive embedded system built with NXP LPC2129 ARM7 and CAN communication**

## 📌 Overview

This project implements a **distributed vehicle monitoring and driver-assistance system** using three independent **NXP LPC2129 ARM7** nodes connected through a **Controller Area Network (CAN)** bus.

Instead of implementing every function in a single controller, the system divides responsibilities across multiple nodes:

* **Main Node** — vehicle control, LCD dashboard, temperature monitoring and user inputs
* **Fuel Node** — analog fuel measurement and CAN transmission
* **Indicator & Reverse Alert Node** — indicator control, ultrasonic obstacle detection and reverse safety alerts

The project demonstrates the design principles used in distributed embedded systems, including **modular drivers, CAN message protocols, interrupt-driven inputs, timer-based measurement, sensor interfacing and fault handling**.

---

# 🎯 Project Goals

The primary objective is to build a small-scale automotive embedded system that demonstrates:

* Multi-node embedded architecture
* CAN-based inter-controller communication
* Real-time sensor monitoring
* Vehicle operating-mode management
* Driver warning generation
* Hardware-level peripheral drivers
* Interrupt-based switch handling
* Timer-based distance measurement
* Sensor fault detection
* Modular Embedded C development


# 🏗️ System Architecture

                              ┌────────────────────────┐
                              │        CAN BUS          │
                              │   11-bit CAN Frames     │
                              └────────────┬───────────┘
                                           │
                  ┌────────────────────────┼────────────────────────┐
                  │                        │                        │
                  ▼                        ▼                        ▼
        ┌─────────────────┐      ┌─────────────────┐      ┌─────────────────────┐
        │    MAIN NODE    │      │    FUEL NODE    │      │ INDICATOR / REVERSE │
        │                 │      │                 │      │       NODE          │
        │   LPC2129       │      │   LPC2129       │      │      LPC2129        │
        ├─────────────────┤      ├─────────────────┤      ├─────────────────────┤
        │ 20x4 LCD        │      │ ADC             │      │ HC-SR04             │
        │ DS18B20         │      │ Fuel Sensor     │      │ Indicator LEDs      │
        │ EINT0/EINT1/2   │      │                 │      │ STOP LED             │
        │ Vehicle Mode    │      │                 │      │ Buzzer               │
        │ CAN             │      │ CAN             │      │ CAN                  │
        └─────────────────┘      └─────────────────┘      └─────────────────────┘

# 🧩 Node Responsibilities

## 1️⃣ Main Node

The Main Node acts as the **vehicle control and dashboard node**.

### Responsibilities

* Read engine temperature from DS18B20
* Receive fuel percentage over CAN
* Receive reverse safety information over CAN
* Manage Forward/Reverse mode
* Process user switches through external interrupts
* Send indicator commands over CAN
* Display vehicle information on a 20x4 LCD
* Activate the buzzer when STOP status is received

### Main Node peripherals

```text
DS18B20
    │
    ├── Temperature
    │
    ▼
Main Node
    │
    ├── LCD
    ├── CAN
    ├── EINT0
    ├── EINT1
    ├── EINT2
    └── Buzzer
```

---

# ⛽ 2️⃣ Fuel Node

The Fuel Node is responsible for measuring the fuel level using the LPC2129 ADC.

### Processing pipeline

```text
Fuel Sensor
     │
     ▼
ADC Channel
     │
     ▼
10-bit ADC Value
     │
     ▼
Fuel Percentage
     │
     ▼
CAN ID 0x100
     │
     ▼
Main Node
```

### ADC resolution

The LPC2129 ADC provides a 10-bit digital result:

```text
0 ───────────────────────── 1023
          ADC range
```

Fuel percentage is calculated approximately as:

```text
Fuel % = ADC_Value × 100 / 1023
```

The calculated percentage is transmitted through CAN.

---------------------------------------------------------------------------------------------------------------------------------------

# 🔄 3️⃣ Indicator & Reverse Alert Node

This node has two operating modes.

### Forward Mode

Controls the left/right indicator LED animation.

```text
Main Node
    │
    │ CAN command
    ▼
Indicator Node
    │
    ├── Left indicator
    └── Right indicator
```

### Reverse Mode

Disables normal indicator operation and enables ultrasonic obstacle detection.
HC-SR04
   │
   ▼
Distance Measurement
   │
   ▼
Safety Classification
   │
   ├── SAFE
   ├── WARNING
   ├── STOP
   └── SENSOR FAULT
   │
   ▼
CAN
   │
   ▼
Main Node

# 📡 CAN Communication Architecture

CAN is used as the communication backbone between all three controllers.

The project uses **standard 11-bit CAN identifiers**.

|  CAN ID | Direction                  | Purpose                   |
| :-----: | -------------------------- | ------------------------- |
| `0x100` | Fuel Node → Main Node      | Fuel percentage           |
| `0x200` | Main Node → Indicator Node | Mode / indicator command  |
| `0x300` | Indicator Node → Main Node | Reverse status + distance |

---

# 📦 CAN Protocol

## CAN ID `0x100` — Fuel Level

**Direction:**
Fuel Node → Main Node

### Payload
Byte 0
┌─────────────┐
│ Fuel %      │
└─────────────┘
Example:
ID   = 0x100
DLC  = 1
DATA = 75
Meaning:
Fuel = 75%

# 📦 CAN ID `0x200` — Mode / Indicator Command

**Direction:**
Main Node → Indicator / Reverse Node
### Command bytes

|  Data  | Meaning            |
| :----: | ------------------ |
| `0x01` | Forward Mode       |
| `0x02` | Reverse Mode       |
| `0x10` | Indicators OFF     |
| `0x11` | Left Indicator ON  |
| `0x12` | Right Indicator ON |

Example:
ID   = 0x200
DLC  = 1
DATA = 0x11
Result:
Left Indicator ON

-----------------------------------------------------------------

# 📦 CAN ID 0x300 — Reverse Status

**Direction:**
Indicator / Reverse Node → Main Node

### Status values

|  Value | Status  | Meaning                        |
| :----: | ------- | ------------------------------ |
| `0x01` | SAFE    | No immediate obstacle          |
| `0x02` | WARNING | Obstacle approaching           |
| `0x03` | STOP    | Obstacle too close             |
| `0x04` | FAULT   | HC-SR04 did not return an echo |

### Payload format
Data1

31                  24 23                 8 7       0
┌────────────────────┬────────────────────┬─────────┐
│      Reserved      │ Distance (cm)      │ Status  │
└────────────────────┴────────────────────┴─────────┘
The implementation uses:
Data1 = status | (distance_cm << 8);
# 🚨 Reverse Safety Logic

The reverse system uses distance thresholds defined in the Indicator/Reverse Node:
#define SAFE_DIST_CM 100U
#define WARN_DIST_CM 40U
The resulting behavior is:
        Distance
                       │
          ┌────────────┼────────────┐
          │            │            │
          ▼            ▼            ▼
       >100 cm      40-100 cm     ≤40 cm
          │            │            │
          ▼            ▼            ▼
        SAFE        WARNING        STOP
          │            │            │
       Buzzer OFF   Intermittent   Continuous
                    Buzzer         Buzzer
                                   +
                                  STOP LED

# 📏 HC-SR04 Distance Measurement

The HC-SR04 is used for reverse obstacle detection.

### Interface

| Signal | LPC2129 |
| ------ | ------- |
| TRIG   | `P0.16` |
| ECHO   | `P0.17` |

The measurement process is:
1. Generate 10 µs trigger pulse
              ↓
2. HC-SR04 sends ultrasonic burst
              ↓
3. ECHO goes HIGH
              ↓
4. Measure ECHO pulse width
              ↓
5. Convert time to distance
The driver uses:
Distance(cm) = Echo pulse width(µs) / 58
---

# ⏱️ Hardware Timer-Based Measurement

An important implementation detail is that the HC-SR04 driver does **not rely on repeated software delay calls to measure the ECHO pulse**.

Timer0 is configured as a free-running **1 µs time base**.
PCLK = 15 MHz

Timer Prescaler
      ↓
1 µs timer tick
      ↓
ECHO rising edge → timestamp
      ↓
ECHO falling edge → timestamp
      ↓
Pulse width
      ↓
Distance
This avoids the timing inaccuracies caused by counting software delay loops.

# 🌡️ DS18B20 Temperature Monitoring

The Main Node continuously reads the DS18B20 temperature sensor.

### Connection

DS18B20 DQ → P0.11
A pull-up resistor is required on the 1-Wire data line.

The temperature is displayed on the first LCD row:
TEMP:32 C

# 🖥️ LCD Dashboard

The Main Node uses a 20x4 LCD.

### Display concept

┌────────────────────┐
│ TEMP:32 C          │
│ FUEL:75%  [████]   │
│ MODE:FWD           │
│ L:<       R:>      │
└────────────────────┘

During Reverse mode:

┌────────────────────┐
│ TEMP:32 C          │
│ FUEL:75%  [████]   │
│ REV:65CM WARNING   │
│ L:         R:      │
└────────────────────┘
The project also uses LCD CGRAM to generate custom:

* Fuel-level icons
* Left arrow
* Right arrow

---

# ⚡ External Interrupt Architecture

Three external interrupts are used on the Main Node.

| Interrupt | Pin    | Function             |
| --------- | ------ | -------------------- |
| EINT0     | `P0.1` | Forward/Reverse mode |
| EINT1     | `P0.3` | Left indicator       |
| EINT2     | `P0.7` | Right indicator      |

### Interrupt flow
Physical Switch
      │
      ▼
External Interrupt
      │
      ▼
VIC
      │
      ▼
ISR
      │
      ▼
Update System State
      │
      ▼
CAN Command

The project uses **vectored interrupts through the LPC2129 VIC**.

# 🔘 Switch Debouncing

Mechanical switches can produce multiple transitions when pressed.

The project applies a software debounce delay:
delay_ms(200);
This prevents one physical press from being interpreted as multiple events.

> A production-grade version could replace blocking ISR debounce with timer-based or state-machine debounce.

---

# 🔊 Buzzer & Warning Strategy

The reverse node controls:
P0.20 → Buzzer
P0.19 → STOP LED
### SAFE
Buzzer = OFF
STOP LED = OFF
### WARNING
Buzzer = intermittent
STOP LED = OFF

### STOP
Buzzer = continuously ON
STOP LED = ON
### SENSOR FAULT

If the HC-SR04 does not return an ECHO:
Status = FAULT

The system does not incorrectly interpret the missing sensor response as a very large safe distance.

# 🧠 Software Architecture

The project follows a modular driver-based structure.
┌──────────────────────────────────────┐
│          APPLICATION LAYER           │
│                                      │
│ Vehicle Mode                         │
│ Fuel Processing                       │
│ Reverse Safety Logic                  │
│ LCD Dashboard                         │
└──────────────────┬───────────────────┘
                   │
┌──────────────────▼───────────────────┐
│           DRIVER LAYER               │
│                                      │
│ CAN Driver                            │
│ ADC Driver                            │
│ LCD Driver                            │
│ DS18B20 Driver                        │
│ HC-SR04 Driver                        │
│ Delay Driver                          │
│ GPIO / Pin Configuration              │
└──────────────────┬───────────────────┘
                   │
┌──────────────────▼───────────────────┐
│            HARDWARE                  │
│                                      │
│ LPC2129 ARM7                         │
│ GPIO / ADC / Timer / CAN / VIC       │
└──────────────────────────────────────┘
---

# 📁 Repository Structure
CAN-Vehicle-Monitoring-System/
│
├── MainNode/
│   ├── main.c
│   ├── can.c
│   ├── can.h
│   ├── can_defines.h
│   ├── can_ids.h
│   ├── lcd.c
│   ├── lcd.h
│   ├── lcd_defines.h
│   ├── ds18b20.c
│   ├── ds18b20.h
│   ├── delay.c
│   ├── delay.h
│   ├── pin_connect_block.c
│   ├── pin_connect_block.h
│   ├── Startup.s
│   └── mainupdate.uvproj
│
├── FuelNode/
│   ├── main.c
│   ├── ADC.c
│   ├── ADC.h
│   ├── ADC_defines.h
│   ├── can.c
│   ├── can.h
│   ├── can_defines.h
│   ├── can_ids.h
│   ├── delay.c
│   ├── delay.h
│   ├── pin_connect_block.c
│   ├── pin_connect_block.h
│   ├── Startup.s
│   └── fuel.uvproj
│
├── IndicatorReverseAlertNode/
│   ├── main.c
│   ├── hcsr04.c
│   ├── hcsr04.h
│   ├── can.c
│   ├── can.h
│   ├── can_defines.h
│   ├── can_ids.h
│   ├── delay.c
│   ├── delay.h
│   ├── pin_connect_block.c
│   ├── pin_connect_block.h
│   ├── Startup.s
│   └── indicatorupdate.uvproj
│
├── README.md
└── .gitignore

---

# 🔌 Hardware Pin Mapping

## Main Node

| Peripheral   | LPC2129 Pin    |
| ------------ | -------------- |
| LCD D0-D7    | `P0.8-P0.15`   |
| LCD RS       | `P0.16`        |
| LCD RW       | `P0.17`        |
| LCD EN       | `P0.18`        |
| DS18B20 DQ   | `P0.11`        |
| Mode Switch  | `P0.1 / EINT0` |
| Left Switch  | `P0.3 / EINT1` |
| Right Switch | `P0.7 / EINT2` |
| Buzzer       | `P0.19`        |
| CAN1 RX      | `P0.25`        |

## Fuel Node

| Peripheral | LPC2129 Pin     |
| ---------- | --------------- |
| Fuel ADC   | `P0.28 / AD0.1` |
| CAN1 RX    | `P0.25`         |

## Indicator / Reverse Node

| Peripheral     | LPC2129 Pin |
| -------------- | ----------- |
| Indicator LEDs | `P0.0-P0.7` |
| STOP LED       | `P0.19`     |
| Buzzer         | `P0.20`     |
| HC-SR04 TRIG   | `P0.16`     |
| HC-SR04 ECHO   | `P0.17`     |
| CAN1 RX        | `P0.25`     |

> Verify the physical board wiring before applying these pin assignments to hardware.

---

# ⚙️ Clock Configuration

The project uses a **12 MHz crystal**.

The PLL configuration generates:
Crystal Frequency = 12 MHz
          │
          ▼
        PLL
          │
          ▼
CCLK = 60 MHz

With the peripheral clock divider left at its reset configuration:
PCLK = CCLK / 4
     = 60 MHz / 4
     = 15 MHz
The CAN and timer configurations are based on this peripheral clock.

# 🛠️ Development Environment

### Software

* Keil µVision
* ARM C Compiler
* Embedded C
* LPC2129 device support
* Git
* GitHub

### Hardware

* NXP LPC2129
* CAN transceiver
* 20x4 LCD
* DS18B20
* HC-SR04
* Analog fuel sensor / potentiometer
* LEDs
* Buzzer
* Push buttons
* 12 MHz crystal

---

# 🚀 Build Instructions

Each node is an independent Keil project.

### Main Node

Open:

```text
MainNode/mainupdate.uvproj
```

### Fuel Node

Open:

```text
FuelNode/fuel.uvproj
```

### Indicator / Reverse Node

Open:

```text
IndicatorReverseAlertNode/indicatorupdate.uvproj
```

Build from Keil:

```text
Project → Build Target
```

or press:

```text
F7
```

After successful compilation, program the corresponding generated image onto each LPC2129 controller.

---

# 🧪 Functional Test Plan

## Test 1 — Temperature

**Input:** DS18B20

**Expected:**

```text
Temperature → Main Node → LCD
```

---

## Test 2 — Fuel

**Input:** Fuel sensor / potentiometer

**Expected:**

```text
ADC
 ↓
Fuel %
 ↓
CAN 0x100
 ↓
Main Node
 ↓
LCD
```

---

## Test 3 — Forward Mode

Press the Mode switch.

Expected:

```text
EINT0
 ↓
Forward Mode
 ↓
CAN 0x200
 ↓
Indicator Node
```

---

## Test 4 — Left Indicator

Press the left switch.

Expected:

```text
EINT1
 ↓
CAN 0x200 / 0x11
 ↓
Left LED animation
```

---

## Test 5 — Right Indicator

Press the right switch.

Expected:

```text
EINT2
 ↓
CAN 0x200 / 0x12
 ↓
Right LED animation
```

---

## Test 6 — Reverse Safety

Switch to Reverse mode and place an obstacle at different distances.

Expected:

```text
>100 cm       → SAFE
40–100 cm     → WARNING
≤40 cm        → STOP
No ECHO       → FAULT
```

---

# 🧪 Fault Handling

One important safety feature is explicit handling of ultrasonic sensor failure.

The HC-SR04 driver returns:

```c
0xFFFF
```

when an ECHO response is not received within the configured timeout.

The application converts this into:

```text
REV_STATUS_FAULT
```

instead of treating it as:

```text
"Obstacle is very far away"
```

This distinction is important in safety-oriented embedded systems.

---

# 🔍 Engineering Decisions

## Why CAN?

CAN was selected because the application naturally consists of multiple controllers that need to exchange small control/status messages.

Advantages include:

* Multi-node communication
* Message-based architecture
* Identifier-based prioritization
* Robust error detection
* Reduced wiring compared with point-to-point communication
* Automotive industry relevance

---

## Why Separate Nodes?

Separating the system into three nodes demonstrates a distributed architecture:

```text
Main Node
   │
   ├── vehicle control
   └── HMI

Fuel Node
   │
   └── fuel acquisition

Indicator/Reverse Node
   │
   ├── indicators
   └── obstacle detection
```

Each node has a clear responsibility and communicates through a defined CAN protocol.

---

## Why Hardware Timer for HC-SR04?

Measuring the ECHO pulse using software delay loops introduces timing errors because the actual execution time depends on:

* Function-call overhead
* Compiler optimization
* Instruction execution time
* Loop overhead

Using Timer0 provides a deterministic time base:

```text
1 timer tick = 1 µs
```

which gives a much more reliable pulse-width measurement.

---

# 💡 Key Embedded Concepts Demonstrated

This project demonstrates practical implementation of:

### MCU / ARM

* ARM7TDMI-S
* LPC2129
* GPIO
* PINSEL
* PLL
* CCLK/PCLK
* VIC
* External interrupts
* Timer programming

### Communication

* CAN controller
* CAN standard identifiers
* CAN data frames
* DLC
* CAN TX/RX
* Distributed node communication
* CAN application protocol

### Sensors

* ADC
* DS18B20
* HC-SR04

### Embedded C

* Register-level programming
* `volatile`
* Bit manipulation
* Driver abstraction
* Interrupt service routines
* State management
* Modular source organization
* Fault handling

---

# 📊 System Data Flow

```text
                  ┌─────────────┐
                  │ DS18B20     │
                  └──────┬──────┘
                         │
                         ▼
                  ┌─────────────┐
                  │  Main Node  │
                  │             │
                  │ LCD + Mode  │
                  └──────┬──────┘
                         │
                    CAN 0x200
                         │
                         ▼
              ┌─────────────────────┐
              │ Indicator / Reverse │
              │        Node         │
              └──────────┬──────────┘
                         │
                     HC-SR04
                         │
                         ▼
                 SAFE/WARN/STOP
                         │
                    CAN 0x300
                         │
                         ▼
                    Main Node


       ┌─────────────┐
       │ Fuel Sensor │
       └──────┬──────┘
              │
             ADC
              │
              ▼
        ┌───────────┐
        │ Fuel Node │
        └─────┬─────┘
              │
          CAN 0x100
              │
              ▼
         Main Node
```

---

# 📈 Future Improvements

The current implementation can be extended toward a more production-oriented architecture.

### Software

* Interrupt-driven CAN reception
* Non-blocking CAN driver
* RTOS-based task scheduling
* Message queues
* Event-driven state machines
* Watchdog supervision
* Diagnostic logging
* Unit testing
* Static analysis
* MISRA-C compliance

### CAN

* CAN error-state monitoring
* Bus-off recovery
* Error counters
* CAN interrupt handling
* Message timeout supervision
* Diagnostic CAN messages

### Sensors

* Moving-average filtering
* Sensor plausibility checks
* Redundant sensor validation
* Calibration support

### Automotive Features

* Vehicle speed
* RPM
* Battery voltage
* Engine diagnostics
* Fault memory
* CAN diagnostics
* UDS-style diagnostic architecture

---

# 🧠 Interview Topics Covered by This Project

This project provides discussion points for embedded interviews in:

### C / Embedded C

* `volatile`
* pointers
* structures
* bit manipulation
* memory-mapped registers
* ISR design
* blocking vs non-blocking code

### ARM7

* ARM7 architecture
* VIC
* IRQ handling
* PINSEL
* PLL
* timers
* GPIO

### CAN

* Arbitration
* CAN identifiers
* Data frame
* DLC
* RTR
* ACK
* Error detection
* TEC / REC
* Error active / passive
* Bus-off
* CAN message prioritization

### Sensors

* ADC resolution
* ADC-to-voltage conversion
* DS18B20 1-Wire communication
* Ultrasonic time-of-flight measurement
* Hardware timer measurement

### System Design

* Distributed architecture
* Node responsibilities
* Message protocol design
* Fault handling
* State machines
* Driver/application separation

---

# 🧭 Possible Production-Oriented Architecture

A future version could migrate the application to an RTOS:

```text
                 FreeRTOS
                    │
       ┌────────────┼─────────────┐
       │            │             │
       ▼            ▼             ▼
 Temperature     CAN RX        Display
    Task           Task           Task
       │            │             │
       └────────────┼─────────────┘
                    │
                Message Queues
                    │
       ┌────────────┼─────────────┐
       ▼            ▼             ▼
   Fuel Data    Reverse Data   Vehicle Mode
```

This would make the system more scalable and reduce dependence on blocking delays.

---

# 📌 Project Highlights

```text
3      → Independent LPC2129 nodes

3      → CAN message identifiers

4      → Reverse safety states

3      → External interrupt inputs

10-bit → ADC resolution

60 MHz → CPU clock

15 MHz → Peripheral clock

1 µs   → HC-SR04 timer resolution
```

---

# 🏁 Conclusion

This project demonstrates the implementation of a **distributed automotive embedded system** using three LPC2129 ARM7 controllers communicating over CAN.

The system combines:

```text
Embedded C
    +
ARM7
    +
CAN
    +
ADC
    +
External Interrupts
    +
Timers
    +
DS18B20
    +
HC-SR04
    +
LCD
    +
Fault Handling
```

The architecture provides a practical demonstration of how independent embedded controllers can cooperate through a defined communication protocol while maintaining clear functional responsibilities.

---

# 👨‍💻 Author

## Deevanji Yadav

**Electronics & Communication Engineer**

### Embedded Systems

`Embedded C` · `ARM7` · `LPC2129` · `STM32` · `CAN` · `FreeRTOS`

### Embedded Linux

`Linux` · `System Programming` · `Device Drivers` · `Linux Kernel`

---

## ⭐ Repository

If this project helped you understand distributed embedded systems and CAN communication, consider giving the repository a ⭐.

---

## 📜 License

This project is intended for **educational, experimental and embedded-systems development purposes**.
