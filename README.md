# ESP32 hand tracker

This starter uses an ESP32 access point, an HTTPS page served by the ESP32, browser-based MediaPipe Hand Landmarker, and a STEP/DIR stepper driver such as an A4988 or DRV8825.

## Why HTTPS and why not `localhost`

On a phone, `localhost` means the phone itself. It does not mean the ESP32. The ESP32 is `192.168.4.1` on its access point.

Phone browsers also restrict camera access to secure pages. This project serves the control page from the ESP32 over HTTPS on port 8000. It creates a self-signed certificate at startup; the first visit will show a certificate warning. Continue to `https://192.168.4.1:8000`, then allow camera access. The certificate warning is expected for this local prototype.

## Hardware assumed

- Classic ESP32 DevKit / ESP32-WROOM board
- A4988 or DRV8825 stepper driver (STEP/DIR interface)
- A bipolar stepper motor and a separate motor power supply sized for the motor
- GPIO 26 to STEP, GPIO 27 to DIR, GPIO 25 to ENABLE (optional; active LOW)
- Driver logic ground and ESP32 ground connected together

Do not connect a stepper motor directly to ESP32 pins. Set the driver's current limit for your motor before powering it. This prototype assumes the head is mechanically centered when the ESP32 boots; it has no limit switch or homing sensor. Keep travel clear and adjust the software angle limits to your mechanism.

## Build and flash

1. Install PlatformIO (VS Code extension or `pio` command line).
2. Open this folder as the PlatformIO project.
3. Edit the AP name and motor settings at the top of `src/main.cpp` if needed. The access point is open and has no Wi-Fi password.
4. Build and upload to an ESP32 DevKit board.
5. Open the serial monitor at 115200 baud. Wait for the certificate generation to finish.
6. Join the open Wi-Fi network `HandTracker-ESP32`. If your phone asks, stay connected without internet.
7. Open **https://192.168.4.1:8000** (including `https://`). Accept the local self-signed certificate warning, then tap **Start camera** and allow camera permission.

The PlatformIO project pins ESP32 Arduino platform 6.6.0 because the HTTPS server's current release uses an ESP-IDF header removed from newer releases. The small compatibility shim is included under `lib/esp32_https_server/src/hwcrypto/sha.h`.

The hand model runs in the phone browser. On first start, the page downloads MediaPipe JavaScript and WebAssembly from jsDelivr and an approximately 8 MB model from Google's CDN. The ESP32 does not relay internet; the phone itself must be able to reach those servers (for example, through cellular data while still connected to the ESP32 AP). The page shows which download or initialization stage fails. Camera frames stay on the phone.

## Behavior

The horizontal center of the detected hand maps to a pan angle from -60 to +60 degrees. The browser sends only the normalized hand-center position at a limited rate; no camera frames leave the phone. The stepper starts disabled. Use **Enable stepper** to allow movement, adjust the speed limit and hand smoothing sliders, and use **Stop tracking** to stop the camera and disable the driver.

The ESP32 runs a bounded PID controller with acceleration limiting and a braking-distance speed cap. It uses AccelStepper's emitted-step count as its position estimate. Without a physical encoder, it cannot detect or correct missed motor steps; keep the mechanism within the software angle limits and add homing/feedback hardware if exact physical positioning is required.

This is a pan tracker only; it does not control tilt, distance, or identify a particular person. For reliable direction and repeatable position after power cycling, add a homing switch and calibrate the camera field of view.

## Adjustments

In `src/main.cpp`, change `STEP_PIN`, `DIR_PIN`, `ENABLE_PIN`, `STEPS_PER_REV`, `MICROSTEPS`, `GEAR_RATIO`, and `MAX_ANGLE_DEG` for your setup. If the motor moves the wrong way, change `INVERT_DIRECTION`. The page's speed limit and smoothing controls are in `src/tracker_page.h`; PID gains and acceleration are near the top of `src/main.cpp`.

## Browser/model note

The page uses MediaPipe Hand Landmarker 1.1.0 in VIDEO mode, with GPU delegate first and CPU/WASM fallback. Its frame inference is performed on the phone; it sends only a scalar target to the ESP32. The MediaPipe web guide documents the browser task and model setup: https://developers.google.com/edge/mediapipe/solutions/vision/hand_landmarker/web_js
