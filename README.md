# ☀️ Solar Enerlytics

Solar Enerlytics is an IoT-enabled smart solar power monitoring and management system designed to monitor, analyze, and optimize solar energy usage in real time.

The project integrates hardware sensors, ESP32 microcontroller, cloud/database services, and a responsive web dashboard for intelligent solar power management.

---

# 🌐 Live Demo

🔗 [Open Solar Enerlytics](https:solarenerlytics.vercel.app)

## 🚀 Features

- ⚡ Real-time Solar Power Monitoring
- 🔋 Battery Level & Charging Management
- 🏠 Smart Switching Between Solar, Battery & Grid
- 📊 Live Dashboard Visualization
- 🌦️ Weather-aware Energy Optimization
- 📈 Historical Data Logging & Analytics
- 🔐 Secure Role-based Control System
- 📡 ESP32 + Sensor Integration
- ☁️ Cloud Database Connectivity
- 📱 Responsive Web Interface
- 🧠 AI/ML-based Smart Energy Features
- ⚠️ Hardware Safety Monitoring
- 🔄 Automatic Relay Switching Logic

---

## 🛠️ Technologies Used

### Frontend
- HTML
- CSS
- JavaScript
- Chart.js

### Backend
- Flask (Python)

### Database
- SQLite
- Supabase

### Hardware
- ESP32
- Voltage Sensors
- Current Sensors
- Relay Modules
- Solar Panel
- Battery System

### Deployment
- Vercel

---

## 📂 Project Structure

```bash
Solar-Enerlytics/
│
├── app.py
├── requirements.txt
├── package.json
├── vercel.json
├── README.md
│
└── src/
    ├── index.html
    ├── style.css
    └── script.js
```

---

## 🛠️ ESP32 Code

```bash
Solar-Enerlytics/
│
└── src/
    ├── hardware.ino
```
Upload the code by changing the WiFi SSID and Password to ESP32
---

## ⚙️ System Working

The system continuously monitors:

- Solar panel voltage and current
- Battery percentage
- Power consumption
- Grid availability
- Relay states

Based on intelligent switching logic:

### 🔋 Battery Below Threshold
- Grid powers the house
- Solar charges the battery

### ☀️ Battery in Healthy Range
- Battery powers the house
- Solar continues charging

### ⚡ Battery Fully Charged
- Excess solar energy can be redirected/sold to grid

The dashboard displays:

- Live voltage/current values
- Battery percentage
- Relay status
- Solar generation statistics
- Historical energy analytics

---

## 🌐 Live Demo

🔗 https://solar-enerlytics.vercel.app

---

## 🔧 Installation & Setup

### 1️⃣ Clone Repository

```bash
git clone https://github.com/your-username/Solar-Enerlytics.git
cd Solar-Enerlytics
```

---

### 2️⃣ Install Dependencies

```bash
pip install -r requirements.txt
```

---

### 3️⃣ Run Flask Server

```bash
python app.py
```

---

### 4️⃣ Open in Browser

```bash
http://127.0.0.1:5000
```

---

## 📡 Hardware Components Used

- ESP32 Development Board
- Solar Panel
- Battery
- Voltage Sensor
- Current Sensor
- Relay Modules
- DC Power Supply
- Connecting Wires

---

## 📸 Hardware Integration

The hardware setup consists of:

- ESP32 Controller
- Relay Modules
- Solar Panel
- Battery
- Sensors for Voltage & Current Monitoring

ESP32 sends real-time sensor data to the backend server, which updates the live dashboard dynamically.

---

## 🔐 Security Features

- Role-based Dashboard Access
- Manual Relay Override for Admin
- Restricted Client Controls
- Secure Database Communication

---

## 📊 Future Improvements

- AI-based Power Prediction
- Advanced Load Forecasting
- Mobile Application
- Smart Appliance Automation
- Voice Control Integration
- Enhanced Grid Optimization
- Battery Health Prediction

---

## 👨‍💻 Developed By

# 👨‍💻 Team

## Dilip Kumar A N
- Email: dilipkumaran.ec23@rvce.edu.in

## Arya B V
- Email: aryabv.ec23@rvce.edu.in

---

# 📄 License

Developed and maintained by the Solar Enerlytics Team.

All rights reserved © 2026 Solar Enerlytics. This project is intended for academic and educational purposes only.
