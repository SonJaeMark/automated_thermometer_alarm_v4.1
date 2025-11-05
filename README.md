# 🌡️ ESP32 Automated Thermometer Alarm v3

An IoT-based **real-time temperature monitoring and chemical management system** built using the **ESP32**, **MAX6675 thermocouple**, **LittleFS web hosting**, **WebSocket communication**, and **Supabase cloud integration**.

---

## 🧭 Overview

This project provides a **web-based dashboard** served directly from the ESP32, displaying **live temperature readings**, **real-time alarms**, and a **chemical database** synchronized with Supabase.

---

## 🎯 System Components

| Component | Description |
|------------|-------------|
| **ESP32** | Main microcontroller running web server & WebSocket |
| **MAX6675** | Thermocouple amplifier for temperature sensing |
| **Supabase** | Cloud database for storing ESP32 IPs and chemical data |
| **LittleFS** | Filesystem for hosting `index.html`, `stylesheet.css`, and `script.js` |
| **WebSocket** | Real-time communication between ESP32 and web dashboard |
| **Buzzer & LEDs** | Audio/visual temperature alerts |
| **WiFiManager** | Dynamic Wi-Fi setup (no hardcoded credentials) |

---

## ⚙️ Features

### 🔌 ESP32 Firmware
- Hosts a **web dashboard** from LittleFS (`index.html`, `stylesheet.css`, `script.js`)
- Connects to Wi-Fi using **WiFiManager** with SSID `ESP32-Setup`
- Streams **live temperature data** via WebSocket (`/ws`)
- **Logs, records, and exports** temperature data to CSV
- Sends **current IP and SSID** to Supabase table `esp32_connections`
- Includes **Wi-Fi reset button (GPIO4)** — hold for 3 seconds to clear credentials
- LED indicators:
  - 🟢 **Green** — Connected to Wi-Fi  
  - 🔵 **Blue** — Reading active  
  - 🔴 **Red** — Threshold exceeded (alarm active)  
  - 🟡 **Yellow** — Recording active  

### 🧭 Web Dashboard
- Real-time temperature chart powered by **Chart.js**
- Adjustable temperature threshold alert
- Automatic **buzzer + red LED** when temperature exceeds threshold
- CSV export for recorded temperature logs
- Live WebSocket connection indicator
- Supabase-linked **chemical database** with CRUD support
- Modern **glassmorphic UI** and full **mobile responsiveness**

---

## 🌐 Accessing the Dashboard

### 🧭 Option 1 — Local Access (Direct)
Once your ESP32 is connected to Wi-Fi:
1. Check its IP address in the **Serial Monitor** or in your **Supabase `esp32_connections`** table.
2. Open your browser and go to: http://<ESP32-IP>

### ☁️ Option 2 — Cloud Access (Recommended)
Visit the **Netlify-hosted dashboard**: https://esp32-connect.netlify.app

---

This site automatically retrieves your latest ESP32 IP from Supabase for easy access.

> 💡 **Note:**  
> If the IP is not responding or outdated, simply **press the ESP32 reset button** (not the Wi-Fi reset button).  
> This will reinitialize the ESP32 and resend its latest IP to Supabase.

---

## 🧩 File Structure

📦 ESP32_Automated_Thermometer_Alarm_v3
├── automated_thermometer_alarm_v3.ino # ESP32 firmware
├── data/
│ ├── index.html # Web dashboard UI
│ ├── script.js # Front-end logic & Supabase sync
│ ├── stylesheet.css # Styling and design
└── README.md


---

## 📡 Supabase Integration

### Table: `esp32_connections`
Stores current ESP32 SSID and IP for cloud dashboard access.

| Column | Type | Description |
|---------|------|-------------|
| `id` | bigint (auto increment) | Primary key |
| `ssid` | text | Connected Wi-Fi SSID |
| `ip` | text | Current ESP32 IP address |
| `timestamp` | timestamptz | Auto-generated timestamp |

### Table: `chemicals`
Stores chemical information for lab reference.

| Column | Type | Description |
|---------|------|-------------|
| `id` | bigint | Primary key |
| `chemical_name` | text | Name of the chemical |
| `formula` | text | Chemical formula |
| `boiling_point` | float | Boiling point (°C) |
| `freezing_point` | float | Freezing point (°C) |
| `hazard_level` | text | Low / Medium / High |
| `notes` | text | Optional notes |

---

## ⚙️ Hardware Configuration

| Component | GPIO Pin | Function |
|------------|-----------|----------|
| MAX6675 SO | 19 | Data (MISO) |
| MAX6675 CS | 5 | Chip Select |
| MAX6675 SCK | 18 | Clock |
| LED Green | 13 | Connection status |
| LED Blue | 27 | Reading indicator |
| LED Red | 23 | Temperature alert |
| LED Yellow | 33 | Recording indicator |
| Wi-Fi Reset Button | 4 | Hold 3s to clear Wi-Fi credentials |

🟢 LEDs are **active-low** (LOW = ON, HIGH = OFF)

---

## 🧾 Setup Guide

### 1. Upload Web Files
1. Place `index.html`, `stylesheet.css`, and `script.js` inside the `data/` folder.
2. Upload them to your ESP32 using **Arduino ESP32 LittleFS Data Upload** tool.

### 2. Flash Firmware
- Open and upload `automated_thermometer_alarm_v3.ino` to your ESP32.

### 3. Connect to Wi-Fi
- Connect to the access point `ESP32-Setup` (password: `12345678`).
- Choose your Wi-Fi network via the captive portal.

### 4. View Dashboard
- Access `http://<ESP32-IP>` locally or  
  go to [esp32-connect.netlify.app](https://esp32-connect.netlify.app).

---

## 🧠 Technical Details

| Component | Description |
|------------|-------------|
| **WiFiManager** | Simplifies dynamic Wi-Fi setup |
| **ESPAsyncWebServer** | Handles web and WebSocket server |
| **MAX6675 Library** | Reads thermocouple data |
| **Supabase REST API** | Syncs ESP32 IP and chemical database |
| **Chart.js** | Renders dynamic temperature charts |
| **LittleFS** | Stores web interface on the ESP32 flash memory |

---

## 🧰 Dependencies

### Arduino Libraries
- `WiFiManager`
- `ESPAsyncWebServer`
- `AsyncTCP`
- `ArduinoJson`
- `max6675`
- `LittleFS`
- `HTTPClient`

Install these via **Arduino Library Manager** or **PlatformIO**.

---

## 📊 Example Use Cases

- Laboratory chemical temperature monitoring  
- Real-time industrial process control  
- Smart IoT safety and alert systems  
- Educational IoT projects using Supabase + ESP32  

---

## 🧩 Future Enhancements
- Email or SMS alerts when threshold is exceeded  
- Multi-sensor (MAX6675 array) dashboard  
- Automatic Supabase data logging  
- OTA (Over-the-Air) firmware updates  

---

## 🧾 License

Licensed under the **MIT License**.  
Free to modify and use for educational or research projects.

---

## 👨‍💻 Author

**Mark Jayson Lanuzo**  
📅 November 2025  
📍 Manila, Philippines  
💡 Developed for real-time chemical monitoring and IoT automation using ESP32.

---
