# 🏀 ESP8266 Interactive 2D Physics Ball Simulation

An interactive, real-time 2D rigid-body physics simulation running on an **ESP8266** paired with an **ILI9341 320x240 TFT display**, **XPT2046 touch controller**, **MPU6050 6-axis gyro/accelerometer**, and a **passive buzzer**.

Featuring dynamic 100 Hz sub-stepped physics, inertia-driven physical shake forces, procedural collision sound generation, interactive touch repulsion ("whitehole"), and flicker-free rendering.

---

## 🌟 Key Features

* **150 Hz Sub-Stepped Physics Engine**: Physics updates run at 150 Hz (6.66 ms sub-steps) while display rendering is locked to 50 FPS (20 ms), delivering buttery-smooth elastic collisions and zero tunneling without overloading SPI transfers.
* **Mass & Elastic Collisions**: Ball sizes are generated randomly with mass proportional to area ($m \propto r^2$). Collisions conserve momentum and kinetic energy with customizable restitution and drag.
* **MPU6050 Accelerometer Gravity & Inertia**:
* **Tilt Gravity**: Rolling direction tracks physical board tilt smoothly.
* **Physical Inertial Force**: Sudden shakes, jerks, and stops calculate frame-to-frame acceleration deltas ($\Delta A$), transferring real force to the balls (they lag behind and slam forward on sudden stops).
* **Dynamic Highlight Shifting**: Specular light highlights shift position based on real-time gravity vectors with geometry bounds checking to keep reflections inside ball perimeters.


* **Touch Interaction ("Whitehole")**: Touching the screen spawns a multi-layered force field that repels nearby balls.
* **Procedural Collision Audio**:
* Sound pitch scales inversely with ball size (smaller balls produce high-frequency clinks, large balls create low-pitched thumps).
* Sound duration is calculated using **relative positional displacement deltas ($\Delta p$)** rather than raw velocity spikes, preventing piezo hum/lockup during heavy ball compression.


* **Optimized Interleaved Rendering**: Clears and redraws individual balls sequentially with instant neighbor overlap repairs (`j < i`), eliminating whole-screen flickering while preventing black visual artifacts.

---

## 🛠️ Hardware Components

| Component | Description |
| --- | --- |
| **Microcontroller** | ESP8266 (NodeMCU V3 / Wemos D1 Mini / ESP-12F) |
| **Display Module** | 2.8" or 2.4" SPI TFT (ILI9341, 320x240 resolution) |
| **Touch Controller** | Integrated SPI Touch (XPT2046) |
| **IMU Sensor** | MPU6050 6-Axis Gyroscope / Accelerometer (GY-521) |
| **Audio Output** | 5V / 3.3V Passive Buzzer Module |

---

## 🔌 Pin Mapping (ESP8266)

### 1. ILI9341 Display & Touch (SPI Bus)

*Make sure your `TFT_eSPI` library `User_Setup.h` matches these pins:*

| Signal | ESP8266 Pin | GPIO | Notes |
| --- | --- | --- | --- |
| **TFT_MOSI** | `D7` | GPIO 13 | Shared SPI Data |
| **TFT_MISO** | `D6` | GPIO 12 | Shared SPI Data |
| **TFT_SCLK** | `D5` | GPIO 14 | Shared SPI Clock |
| **TFT_CS** | `D8` | GPIO 15 | Display Chip Select |
| **TFT_DC** | `D3` | GPIO 0 | Data / Command |
| **TOUCH_CS** | `D0` | GPIO 16 | Touch Chip Select |

### 2. MPU6050 Sensor ($\text{I}^2\text{C}$ Bus)

| MPU6050 Pin | ESP8266 Pin | GPIO | Notes |
| --- | --- | --- | --- |
| **VCC** | `3V3` or `5V` | — | Module has 3.3V regulator |
| **GND** | `GND` | — | Common Ground |
| **SDA** | `D2` | GPIO 4 | $\text{I}^2\text{C}$ Data |
| **SCL** | `D1` | GPIO 5 | $\text{I}^2\text{C}$ Clock |

### 3. Passive Buzzer

| Buzzer Pin | ESP8266 Pin | GPIO | Notes |
| --- | --- | --- | --- |
| **Signal (+)** | `D4` | GPIO 2 | Series $1\text{k}\Omega$ resistor recommended |
| **GND (-)** | `GND` | — | Ground |

---

## 💻 Dependencies & Libraries

Install the following libraries via the Arduino IDE Library Manager:

1. **[TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)** by Bodmer (Configure `User_Setup.h` for ILI9341 and your pin definitions)
2. **Adafruit MPU6050** by Adafruit
3. **Adafruit Unified Sensor** by Adafruit
4. **Wire** and **SPI** (Included in ESP8266 Arduino Core)

---

## ⚙️ Physics Adjustments

You can tune the physics constants at the top of the `.ino` sketch:

```cpp
#define NUM_BALLS     8        // Number of active physics balls
#define DAMPING       0.9975f  // Friction / Air resistance per sub-step
#define RESTITUTION   0.85f    // Elasticity (Bounciness coefficient)
#define REPULSION_STR 1.75f    // Touch force strength
#define GRAVITY_SCALE 0.0375f  // Tilt gravity multiplier

```

---

## 📜 License

This project is open-source under the **MIT License**. Feel free to modify, extend, or use it in your micro-controller projects!
