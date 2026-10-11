# Smart Adaptive Charger HMI — ESP32 + BLE/Bluetooth App Control

An intelligent, retrofit charger add-on that turns any existing certified AC‑DC
charger into a smart, app-controlled, adaptive charging system — without
modifying the charger itself or the device being charged.

Built for a project expo demo. The core idea: most "smart chargers" either
read the OS's own battery API *or* use external current sensing, rarely both,
and almost none offer a longevity-style target (e.g. "charge to 80% and
hold") on a retrofit external unit. This project combines live, phone-reported
battery percentage (ground truth) with independent current/voltage telemetry
(for monitoring, diagnostics, and a safety fallback) in a single compact HMI
unit.

---

## How it works (big picture)

```
230V AC
   |
   v
[Existing certified charger brick — UNMODIFIED]
   |
   v
19V DC output
   |
   v
[THIS UNIT — current/voltage sense, relay, HMI, Bluetooth]
   |
   v
Laptop / device DC input
```

The unit sits inline between an existing, already-certified charger and the
device being charged. It does **not** rebuild the AC-DC/flyback power stage —
that stays inside the sealed, certified brick, which keeps the whole build
safe to demo live with no exposed mains voltage. All of this project's
original engineering sits in the sensing, decision, and HMI layer.

---

## Features

- **Two charging modes**
  - **Default** — charges to a fixed target (80% by default), cutoff
    verified by the phone's real, live battery percentage.
  - **Manual** — user chooses either:
    - a custom **target percentage** (cutoff verified the same way), or
    - a custom **target time** (pure countdown timer, relay cuts off when
      the time elapses).
- **Real battery percentage, not a guess.** The connected phone reports its
  actual OS-level battery percentage over Bluetooth every ~30 seconds via a
  small MIT App Inventor app, and the ESP32 uses that as the source of truth
  for cutoff — rather than inferring state-of-charge from current alone,
  which breaks down under fast-charge protocols that negotiate voltage.
- **Independent current/voltage telemetry (INA226).** Tracked continuously
  for the on-device display and diagnostics, decoupled from the main cutoff
  decision so a sensor misread can't block (or falsely stop) a charge.
- **On-device HMI** — 0.96" I2C OLED display plus four physical buttons
  (Select / Enter / Increment / Decrement) for full menu navigation without
  needing the phone app open at all times.
- **Hard relay cutoff** — physically disconnects the charger output once the
  target is reached, rather than relying on a soft/software-only stop.
- **Two independent I2C buses** (ESP32's dual hardware I2C peripherals) so
  the OLED and the INA226 sensor don't contend for bus bandwidth or
  addressing.

---

## Hardware

| Component | Role | Notes |
|---|---|---|
| ESP32 Dev Board | Main controller | Built-in classic Bluetooth, dual I2C |
| 0.96" I2C OLED (SSD1306) | On-device display | 128x64, address `0x3C` |
| INA226 current/voltage sensor | Live telemetry | Address `0x44`, 0.1Ω shunt |
| Relay module | Hard cutoff switch | Switches the charger's DC output line |
| 4x push buttons | Local HMI input | Select, Enter, Increment, Decrement |
| Existing certified AC-DC charger | Power source | Unmodified, used as-is |

### Pin map

| Signal | ESP32 Pin |
|---|---|
| OLED SDA | GPIO 4 |
| OLED SCL | GPIO 13 |
| INA226 SDA | GPIO 21 |
| INA226 SCL | GPIO 22 |
| Button — Select | GPIO 16 |
| Button — Enter | GPIO 17 |
| Button — Increment | GPIO 18 |
| Button — Decrement | GPIO 19 |
| Relay control | GPIO 5 |

The OLED and INA226 are deliberately placed on **separate I2C buses**
(`Wire1` and `Wire` respectively) rather than shared, since the ESP32 has two
independent hardware I2C peripherals available.

---

## Software

### Firmware (`charger_hmi_oled.ino`)

- Arduino framework for ESP32.
- Libraries used:
  - `Adafruit_GFX` + `Adafruit_SSD1306` — OLED display driver
  - `INA226` (RobTillaart) — current/voltage sensor
  - `BluetoothSerial` — ESP32's built-in classic Bluetooth (no external
    Bluetooth module required)
- Menu system is a simple page-based state machine driven by the four
  buttons, mirroring a classic LCD-style 2-line UI (wrapped so it renders on
  the OLED through a small compatibility shim).
- The charging cutoff logic:
  - **Percentage-target modes** stop once the phone-reported battery
    percentage reaches the target.
  - **Time-target mode** stops on a plain elapsed-time countdown.
- INA226 readings (current in mA, voltage in V) are shown live on-screen at
  all times during a charging session, independent of which cutoff mode is
  active.

### Mobile app (MIT App Inventor)

- Reads the phone's real battery level using the **Taifun Battery Manager**
  extension (`puravidaapps.com/battery.php`).
- Pairs with the ESP32 over classic Bluetooth (device name `ESP32_MIT_APP`).
- A `Clock` component fires every 30 seconds, sending the current battery
  percentage as a plain text line (e.g. `"65\n"`) to the ESP32 over
  `BluetoothClient`.

---

## Setup

1. **Arduino IDE**: install the ESP32 board package, then install the
   `Adafruit SSD1306`, `Adafruit GFX`, and `INA226` (RobTillaart) libraries
   via Library Manager.
2. Flash `charger_hmi_oled.ino` to the ESP32.
3. **MIT App Inventor**: import the Taifun Battery Manager extension, add a
   `BluetoothClient`, a `Clock` (30000 ms interval), and wire up the
   Connect → Timer → Send blocks described in the firmware's companion
   notes.
4. Power the unit from the existing charger's 19V output (tapped inline,
   before the relay/output jack), pair the phone with `ESP32_MIT_APP`, and
   select a charging mode from the OLED menu.

---

## Safety notes

- The AC-DC/isolation stage is intentionally **not** rebuilt from scratch for
  this unit — it remains inside the original, certified charger brick. Only
  the already-isolated, low-voltage DC output is handled by this add-on.
- The relay provides a hard, physical cutoff independent of any software
  state, so a frozen or disconnected app/Bluetooth link does not leave the
  charger permanently energized in time-target mode (which uses a local
  timer regardless of connection state).

---

## Possible future improvements

- Fallback to INA226-based current-taper cutoff estimation if the phone's
  Bluetooth link drops during a percentage-target session (dual-verified
  cutoff).
- Session energy logging (Wh) and basic cost estimation using the INA226's
  continuous readings.
- Over-temperature / over-current protective cutoff, independent of the
  phone-reported percentage.
- Real-time clock (RTC) module for genuine "ready-by" scheduled charging.

---

## Project context

Built as a project-expo submission. Earlier design exploration covered a
full from-scratch AC-DC flyback power stage (UC3844 controller, MOSFET
selection, optocoupler/TL431 isolated feedback) for understanding the
underlying power electronics; the final build instead retrofits onto an
existing certified charger for safety and build-time reasons, concentrating
original engineering effort on the sensing, decision-making, and HMI layers
described above.
