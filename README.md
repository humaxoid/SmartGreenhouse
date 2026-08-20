<div align="center">

# 🌱 Smart Greenhouse

### Standalone greenhouse automation on ESP32

[![Platform](https://img.shields.io/badge/platform-ESP32-informational?style=flat-square)](https://www.espressif.com/)
[![Build](https://img.shields.io/badge/build-PlatformIO-orange?style=flat-square)](https://platformio.org/)
[![Framework](https://img.shields.io/badge/framework-Arduino%202.0.17-blue?style=flat-square)](https://github.com/espressif/arduino-esp32)
[![Home Assistant](https://img.shields.io/badge/Home%20Assistant-MQTT%20discovery-41BDF5?style=flat-square)](https://www.home-assistant.io/)

**English** · [Русский](README.ru.md)

</div>

---

<div align="center">

### 📑 Contents

</div>

<table>
<tr><td valign="top" width="50%">

1. [📖 About](#1--about)
2. [✨ Features](#2--features)
3. [🔌 Hardware](#3--hardware)
4. [🛠 Flashing with VS Code](#4--flashing-with-vs-code)
5. [🚀 First connection and setup](#5--first-connection-and-setup)
6. [🔑 Default credentials](#6--default-credentials)
7. [🌡 How the automation works](#7--how-the-automation-works)
8. [🖥 Web panel](#8--web-panel)

</td><td valign="top" width="50%">

9. [🔄 Firmware updates](#9--firmware-updates)
10. [🏠 Home Assistant](#10--home-assistant)
11. [🩺 Diagnostics](#11--diagnostics)
12. [⚠️ Known limitations](#12--known-limitations)
13. [🔧 What to do if](#13--what-to-do-if)
14. [📁 Project layout](#14--project-layout)
15. [📄 License](#15--license)

</td></tr>
</table>

---

## 1. 📖 About

The controller waters the beds on a schedule, ventilates the greenhouse through
vents driven by reversible actuators, maintains humidity, and fills a storage
tank from a well. All of it runs without internet or cloud services: the logic
lives entirely on the device, and the network is only needed for the web panel
and notifications.

There are three ways to control it: buttons on the enclosure, the web panel from
a phone or computer, and Home Assistant. If the network goes away, the
greenhouse keeps running on its last settings.

> [!NOTE]
> The project is designed to run for months without interruption. Automation
> does not depend on network availability, and both vent positions and
> irrigation state survive a reboot.

---

## 2. ✨ Features

<table>
<tr><td width="50%" valign="top">

**🪟 Vents**
- Two sashes on reversible actuators
- Relay handoff: upper → lower when opening
- Stepped ventilation in automatic mode
- Rain closes the sashes immediately

</td><td width="50%" valign="top">

**💧 Irrigation**
- Three independent channels
- Repeat interval up to a week
- Watering resumes after a reboot
- Manual control via buttons

</td></tr>
<tr><td valign="top">

**🌫 Climate**
- Humidifier with hysteresis
- Well pump with dry-run protection
- Fault state on continuous pump operation

</td><td valign="top">

**📊 Monitoring**
- Temperature, humidity, pressure
- Illuminance, water level, rain
- Daily charts that survive a reboot

</td></tr>
<tr><td valign="top">

**🖥 Web panel**
- Six color themes
- English and Russian
- Works on phone and desktop

</td><td valign="top">

**🔄 Updates**
- Single file through the web panel
- Over the air straight from VS Code
- Two app partitions, checksum before flashing

</td></tr>
</table>

---

## 3. 🔌 Hardware

### 3.1 Pinout

| Purpose | GPIO |
|---|---|
| I²C — BME280 `0x76` and BH1750 `0x23` | `21` SDA, `22` SCL |
| DHT22 — outdoor temperature and humidity | `13` |
| YL-83 rain sensor — **analog** AO | `36` (ADC1_CH0) |
| AJ-SR04M ultrasonic | `0` TRIG, `34` ECHO |
| Irrigation 1 / 2 / 3 | `25`, `26`, `27` |
| Upper vent: open / close | `32`, `4` |
| Lower vent: open / close | `33`, `15` |
| Humidifier | `14` |
| Well pump | `5` |
| Irrigation buttons 1 / 2 / 3 | `18`, `17`, `16` |
| Vent Open / Close buttons | `23`, `19` |
| Humidifier button | `35` |
| Pump button | `39` |
| Heartbeat LED | `2` |

The full board-level layout, ready to print on A4 (landscape), is in
[`docs/GPIO_ESP32.xlsx`](docs/GPIO_ESP32.xlsx) — Russian version:
[`docs/GPIO_ESP32.ru.xlsx`](docs/GPIO_ESP32.ru.xlsx).

> [!CAUTION]
> **GPIO12 is deliberately left unused.** It is the strapping pin that
> selects flash voltage: held high at reset, the chip switches to 1.8 V
> flash mode, which on this board's 3.3 V flash means a failed boot or a
> damaged chip. Do not hang a relay or a button on it.

Pins are configured in [`include/config.h`](include/config.h) and nowhere else.

### 3.2 ⚠️ Required external components

> [!WARNING]
> **Pull-down on GPIO15.** This is a strapping pin: during ESP32 boot it is
> pulled high, and the lower vent relay may briefly trigger. A **10 kΩ resistor
> between GPIO15 and GND** is required.

> [!WARNING]
> **Voltage divider on ECHO.** The AJ-SR04M runs on 5 V and drives ECHO at 5 V,
> while GPIO34 is rated for 3.3 V.
> ```
> AJ-SR04M ECHO ──┬── 1 kΩ ──── GPIO34
>                 └── 2 kΩ ──── GND
> ```

> [!WARNING]
> **Pull-ups on the button pins GPIO35 and GPIO39.** GPIO34–39 on the
> ESP32 are input-only and have **no internal pull resistors** —
> `INPUT_PULLUP` is a silent no-op there. Without an external pull-up the
> pin floats and the button fires on its own.
>
> ```
> 3.3V ──[ 10 kΩ ]──┬────────── GPIO35 / GPIO39
>                   │
>                 [button] ║ 100 nF   (in parallel)
>                   │      ║
>                  GND    GND
> ```
>
> The 100 nF gives an RC of 10 kΩ × 100 nF = 1 ms: a hardware debounce
> and, more usefully in a greenhouse, a filter against the interference
> the eight motor relays throw at long button cables.
>
> Since v6.2 the rain sensor is analog on GPIO36 and needs no pull-up of
> its own — the module's AO drives the line.

Relays must be **HIGH-active** opto-isolated modules. The vent relay pairs form
an H-bridge; switching both relays of a pair on at once is blocked in firmware.

---

## 4. 🛠 Flashing with VS Code

### 4.1 What you need

1. [Visual Studio Code](https://code.visualstudio.com/)
2. The **PlatformIO IDE** extension — open the Extensions tab, search for
   "PlatformIO IDE", install it, wait for the initial setup, restart the editor
3. A USB-UART driver for your board: **CP210x** or **CH340**

Libraries are downloaded automatically on the first build. Nothing needs to be
installed by hand.

### 4.2 Steps

Open the project folder in VS Code: **File → Open Folder**. PlatformIO detects
the project automatically and its ant icon appears in the sidebar.

> [!IMPORTANT]
> The firmware consists of **two parts** — the executable code and the file
> system holding the web panel. Both must be uploaded, otherwise the panel will
> not open or will not match the firmware.

| Step | PlatformIO task | What it does |
|---|---|---|
| 1 | **Build Filesystem Image** | Builds the image from the `data/` folder |
| 2 | **Upload Filesystem Image** | Uploads the web panel to the board |
| 3 | **Build** | Builds the firmware |
| 4 | **Upload** | Uploads the firmware |

The tasks live in the PlatformIO panel under **PROJECT TASKS →
esp32doit-devkit-v1 → Platform** and **General**.

Or from the VS Code terminal:

```bash
pio run -t buildfs
pio run -t uploadfs
pio run
pio run -t upload
```

The port is detected automatically. If you have several boards and the wrong one
is picked, uncomment `upload_port` in [`platformio.ini`](platformio.ini).

After the build, **`greenhouse_vX.Y.bin`** appears in the project root — the
ready-made file for over-the-air updates.

### 4.3 Serial monitor

**PlatformIO → Monitor**, baud rate **115200**. It shows the IP address, the
reason for the last reset, and system health.

---

## 5. 🚀 First connection and setup

> [!CAUTION]
> **Close both vents before powering up.** On first start the firmware assumes
> the sashes are closed, and there are no position sensors — the position is
> derived from actuator run time. A mismatch cannot be corrected afterwards by
> normal means.

<div align="center">

**Power on** → **`Greenhouse-XXXXXX`** → **`192.168.4.1`** → **pick your network** → **reboot** → **web panel**

</div>

### 5.1 Connect to the controller's access point

Right after flashing there is no saved network, so the controller brings up
**its own access point** and waits to be configured. In the serial monitor it
looks like this:

```
[NET] Cache loaded from NVS. SSID="" (EMPTY -> AP provisioning at boot)
[BOOT] No WiFi credentials — starting AP provisioning
[PROV] AP started: "Greenhouse-040390"
[PROV] Password: 12345678
[PROV] AP IP: 192.168.4.1
```

#### 5.1.1 Access point parameters

| Parameter | Value |
|---|---|
| **Network name (SSID)** | **`Greenhouse-XXXXXX`**, where `XXXXXX` is the last three bytes of the MAC address |
| **Password** | **`12345678`** |
| **Portal address** | **`http://192.168.4.1`** |
| **Band and channel** | 2.4 GHz, channel 6 |
| **Simultaneous clients** | up to 4 |

#### 5.1.2 What to do

1. Take a phone or a laptop and open the list of Wi-Fi networks.
2. Find the network **`Greenhouse-XXXXXX`**. The exact name is printed in the
   serial monitor on the `[PROV] AP started:` line. Without a monitor, look for
   the `Greenhouse-` prefix — there should be no other such network nearby.
3. Connect and enter the password **`12345678`**.
4. Your phone will warn that the **network has no internet access** — that is
   normal and expected. Stay on it.

> [!TIP]
> **Android:** turn mobile data off while you configure. Otherwise the system
> decides a network without internet is broken and quietly moves traffic to
> cellular — the portal will not open.
>
> **iPhone:** in the `Greenhouse-XXXXXX` network settings turn off
> "Auto-Join" for your home network so the phone does not switch back.

5. The browser should open by itself — this is a captive portal. If it does not,
   type the address manually: **`http://192.168.4.1`**

> [!IMPORTANT]
> It must be `http://`, not `https://`. The portal runs unencrypted, and a
> browser that silently upgrades to `https` will show a connection error. Type
> the full address, otherwise the browser will send it to a search engine.

### 5.2 Find your network and connect to it

The portal is two fields and a button. It is in English regardless of the web
panel language — the page is built into the firmware and is not translated.

| Portal element | Purpose |
|---|---|
| **Wi-Fi Network** | drop-down list of the networks found |
| **Refresh list** | scan again |
| **Password** | the password of **your home** network |
| **Show password** | reveal what you typed |
| **Save & reboot** | save and restart |

#### 5.2.1 What to do

1. Open the **Wi-Fi Network** list. The controller scans the air by itself; the
   list fills in 2–3 seconds after the page opens.
2. Find your network. Next to each name you get the **signal level in dBm** and
   a 🔒 for protected networks. The closer the number is to zero, the stronger
   the signal: `-45 dBm` is excellent, `-75 dBm` is marginal.
3. If the list is missing or empty, press **Refresh list** and wait.
4. Type your network's password into **Password**. Tick **Show password** and
   read it back: a typo will only reveal itself after the reboot.
5. Press **Save & reboot**.

> [!NOTE]
> Limits: the network name may be up to 32 characters, the password up to 63.
> Open networks are supported too — just leave **Password** empty.

#### 5.2.2 If your network is not in the list

| Reason | What to do |
|---|---|
| **The network runs on 5 GHz** | The ESP32 only speaks 2.4 GHz. Enable the 2.4 GHz band on the router. Many routers give both bands the same name — split them by name temporarily |
| **The network name is hidden** | The portal only lists networks that broadcast their name. Enable SSID broadcast, configure the controller, then hide it again |
| **Weak signal** | Move the controller closer to the router, configure it there, then put it back. You can check the level later in the panel itself |
| **More than twenty networks around** | The portal lists the first twenty found. Configure the controller closer to the router, where your network lands near the top |
| **The list is always empty** | Check for `[PROV] AP started` in the serial monitor. If the line is missing, the access point never came up — see [section 13](#13--what-to-do-if) |

#### 5.2.3 What happens after saving

The controller replies `OK`, reboots three seconds later, and the access point
goes away. Your phone will return to its usual network on its own. The serial
monitor shows:

```
[PROV] Saved: ssid="YourNetwork", pass=12 chars. Reboot in 3s.
...
[NET] Cache loaded from NVS. SSID="YourNetwork" (configured)
[NET] Got IP: 192.168.1.42
```

> [!NOTE]
> **Mistyped the password?** The controller will fail to connect and bring the
> same access point back up — same name, same password. The difference is that
> `192.168.4.1` now serves the full web panel instead of the portal: go to
> **Settings → Network** and fix it. No trip to the greenhouse with a USB cable.

### 5.3 Find the controller on the network

The address is printed in the serial monitor as `Got IP: ...`. Alternatively,
check the DHCP client list in your router — the device is named
`greenhouse-XXXXXX`.

It is worth reserving a fixed address in the router, or setting a static IP
under **Settings → Network**.

### 5.4 Sign in and configure

Open the address in a browser and sign in with login `admin` and password
`admin`. Then, in order:

1. **Settings → Access** — change the login and password. Do this first
2. **Settings → Time** — pick your time zone. Irrigation timers do not run
   without correct time
3. **Vents → Calibration** — measure the full travel time of each sash with a
   stopwatch and enter it. All positioning accuracy depends on this
4. **Thresholds and schedule** — ventilation temperatures, irrigation times and
   durations, humidity, water levels
5. **Settings → MQTT** — if you want Home Assistant integration

By default all subsystems start in **automatic** mode.

> [!TIP]
> To come back to initial-setup mode later, use
> **Settings → Network → Forget Wi-Fi**. Wi-Fi credentials live in a
> separate flash area and survive firmware uploads, filesystem uploads
> and over-the-air updates alike. Details in
> [section 13](#13--what-to-do-if).

> [!WARNING]
> While the controller sits in initial-setup mode, **the greenhouse automation
> is not running**: vents, irrigation, humidifier and pump are not controlled —
> the vents will not close in the rain either. Do not leave the controller in
> that state for long.

---

## 6. 🔑 Default credentials

| What | Address | Login | Password |
|---|---|---|---|
| Setup access point | `192.168.4.1` | — | `12345678` |
| Web panel | assigned by router | `admin` | `admin` |
| Network flashing (ArduinoOTA) | `greenhouse-XXXXXX.local` | — | web panel password |

> [!WARNING]
> **Change the web panel password right after your first sign-in.** The same
> password protects network flashing: while it is still `admin`, anyone on your
> local network can upload arbitrary code to the controller.

The access point password can only be changed by rebuilding — the
`WIFI_AP_PASSWORD` constant in [`include/config.h`](include/config.h).

---

## 7. 🌡 How the automation works

### 7.1 Vents

Position is computed from actuator run time, which makes **calibration the key
setting**. Measure how long a sash takes to travel from fully closed to fully
open and enter that value. Factory defaults: upper **42 s**, lower **24 s**;
the accepted range is 1–120 seconds.

**Manual mode.** Two buttons, physical or on the panel, work on press-and-hold:
hold and the actuator runs, release and it stops immediately.

- **Open** — the upper sash first, up to 100 %, then handoff to the lower one
- **Close** — mirrored: the lower sash down to 0 % first, then the upper one

There is a 200 ms pause between actuator switches so the power supply does not
sag. Holding longer than 79 seconds aborts automatically — protection in case
the connection drops with a button held down. Rain blocks opening.

**Automatic mode.** Thresholds are shared by both sashes; the defaults are open
at **26 °C**, close at **22 °C**. The logic is stepped:

1. Temperature above the threshold — the upper sash opens by **20 %**
2. A **50 second** pause while the controller watches the temperature
3. If it kept rising by more than **0.3 °C** and is still above the threshold,
   the next step follows. The upper sash goes to 100 %, then the lower joins in
4. If the rise stopped, movement stops too

Closing works in mirror: the lower sash closes first and the temperature must
keep falling. The pause exists to let the greenhouse respond to ventilation
rather than throwing the sashes wide open on a brief spike.

**Rain** takes absolute priority: the sashes close immediately, lower first,
then upper.

### 7.2 Irrigation

Three channels, each with its own start time, duration, and repeat interval of
up to 168 hours. Time is tracked as Unix timestamps, so midnight rollover and
intervals longer than a day are handled correctly.

The start moment is saved immediately: if the controller reboots mid-watering,
it works out the remainder on startup and continues.

### 7.3 Humidifier and pump

The **humidifier** uses humidity hysteresis; the defaults are on at 90 %, off at
95 %. If the sensor fails, the relay is forced off.

The **pump** fills the tank: on below 5 %, off above 95 %. If it runs
continuously past the timeout (30 minutes by default), it enters a fault state —
most likely the well ran dry or the filter clogged. Clear the fault from the
panel, or wait an hour for the automatic reset.

---

## 8. 🖥 Web panel

The main screen shows the sensors, sash positions, irrigation, humidifier and
tank state, plus daily charts — 24 points, one per hour, stored in the file
system and surviving reboots.

Settings are split across seven tabs: network, MQTT, access, time, interface,
update, system. Six color themes and two languages are available; the choice is
stored on the controller and applies on any device. Themes are picked from
tiles that preview their colors, not from a text list.

The panel is fully operable from the keyboard. Tab moves the focus and every
element shows a visible ring; the vents run while you hold Space or Enter on
OPEN or CLOSE and stop the moment you let go, lose focus, or switch tabs.

The panel requests state once per second over WebSocket. This is deliberate:
pushing from the controller reached the socket from a foreign task and corrupted
memory.

---

## 9. 🔄 Firmware updates

> [!NOTE]
> **What is and is not protected.** The container carries a CRC32 for
> each part, and the firmware is written into the spare app partition,
> so a failed or corrupted upload leaves the running version untouched.
> There is **no rollback after a successful flash**: the project does
> not call `esp_ota_mark_app_valid_cancel_rollback()`, and the
> bootloader's rollback support is not enabled in this build. If a
> freshly flashed firmware boots and then panics, the controller will
> keep rebooting into it — recover over USB.

### 9.1 Through the web panel — recommended

**Settings → Update**, then pick **`greenhouse_vX.Y.bin`** from the project
root. It is built automatically on every build and contains **both the firmware
and the file system**, with checksums for each part.

The file name must match the pattern — the firmware validates it and rejects
anything else. Before writing, the controller de-energizes the actuators, saves
sash positions, and halts automation. It reboots on its own once the update
succeeds.

### 9.2 Over the network from VS Code

```bash
pio run -e ota -t upload
```

Before first use, fill in the `[env:ota]` section of
[`platformio.ini`](platformio.ini): the device name
(`greenhouse-XXXXXX.local`, printed in the serial monitor) and `--auth=` with
the web panel password.

> [!NOTE]
> This channel updates **firmware only**. It does not touch the web panel
> files — those need the container or a USB upload.

### 9.3 Over USB

Always works, and is mandatory when the partition table changes. See the
Flashing section.

---

## 10. 🏠 Home Assistant

Enable MQTT in the panel settings and enter your broker address. The device
appears automatically as `greenhouse-XXXXXX`:

| Type | Entities |
|---|---|
| Sensors | temperature, humidity, pressure, illuminance, water level |
| Binary sensors | rain, pump fault |
| Switches | irrigation 1/2/3, humidifier, pump |
| Covers | upper and lower vents — open, close, stop |

From Home Assistant you can drive the relays, switch auto/manual modes, and move
the vents with open, close, and stop commands — the same ones the enclosure
buttons use, with the upper → lower handoff.

> [!NOTE]
> A command acts on **both sashes**, while each reports its own position. There
> is deliberately no position slider: the firmware has no "travel to position N"
> mechanism, and advertising one would be dishonest.

---

## 11. 🩺 Diagnostics

Serial monitor, baud rate **115200**.

On startup the reason for the previous reset is printed in words. The normal
ones are `POWERON`, `SW`, `EXT`. Anything else is accompanied by a dedicated
line reading `PREVIOUS RUN ENDED ABNORMALLY`:

| Reason | What it means |
|---|---|
| `PANIC` | Exception: memory corruption or a null dereference |
| `TASK_WDT` | A task stalled for more than 30 seconds |
| `INT_WDT` | Interrupt watchdog |
| `BROWNOUT` | Supply voltage sag — nine relays on one PSU, check it |

Once a minute a health line is printed:

```
[HEALTH] heap=... min=... largest=... stackWM=... uptime=...s
```

`largest` is the biggest contiguous free block. That is the value that reveals
fragmentation: if `heap` holds steady while `largest` creeps down over weeks,
memory is leaking and failure is not far off.

Every 180 days the controller reboots on schedule at 4 a.m. This is controlled
by the `PLANNED_REBOOT_DAYS` constant.

---

## 12. ⚠️ Known limitations

**Vent position is derived from run time and can drift from reality.** There are
no limit switches. To clear the drift use the "Reset upper / lower" buttons in
the calibration section: the actuator closes for the full travel time plus a
margin, right up to the limit switch, and the counter is zeroed. To keep drift
from accumulating, measure the travel time precisely.

**The rescue access point only comes up at startup.** If the network disappears
while running, the controller reconnects quietly in the background. If you need
direct access, power-cycle it.

---

## 13. 🔧 What to do if

<details>
<summary><b>You mistyped the Wi-Fi password</b></summary>

<br>

This is not a dead end. If a saved network exists at startup but the connection
fails within 30 seconds, the controller brings up a **rescue access point** with
the same name and password as during initial setup, while continuing to retry
the home network.

Connect to it and open `http://192.168.4.1` — this is the regular panel with all
sections. Fix the network under **Settings → Network**. Once the connection
succeeds, the access point shuts down on its own. Greenhouse automation keeps
running throughout.

</details>

<details>
<summary><b>You need to get back to initial setup mode</b></summary>

<br>

Wi-Fi settings live in a separate memory region and are **not erased** by
firmware uploads, file system uploads, or over-the-air updates. This is
deliberate: otherwise the greenhouse would drop off the network after every
update.

- **Settings → Network → Forget Wi-Fi** — the normal path
- If the panel is unreachable, erase the whole flash:
  ```bash
  pio run -t erase
  ```
  This wipes everything: network, panel password, calibration, history.

The **factory reset** under System does **not** touch network settings — it
restores thresholds, schedules, and tank parameters to their defaults.

</details>

<details>
<summary><b>The panel says "LittleFS not uploaded"</b></summary>

<br>

The file system has not been uploaded. Run **Upload Filesystem Image**, or
update using the `greenhouse_vX.Y.bin` container.

</details>

<details>
<summary><b>Relays click when the board powers up</b></summary>

<br>

Check the 10 kΩ pull-down between GPIO15 and GND, and make sure you are using
HIGH-active opto-isolated relay modules.

</details>

---

## 14. 📁 Project layout

```
include/          module headers; config.h holds all configuration
src/              implementation: sensors, vents, irrigation, network, OTA
data/             web panel (LittleFS): index.html, app.js, style.css
tools/            OTA container packer and build post-script
platformio.ini    build setup, dependencies, environments
CHANGELOG.md      version history
CLAUDE.md         working notes for future development
```

---

## 15. 📄 License

Personal project. Use at your own risk.

<div align="center">
<sub>Version history — <a href="CHANGELOG.md">CHANGELOG.md</a></sub>
</div>
