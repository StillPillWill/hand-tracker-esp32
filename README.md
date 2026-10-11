# ESP32 Soccer Camera Turret

This branch turns the hand-tracker prototype into a two-axis soccer camera turret. The ESP32 is a local yaw/pitch motor controller; the iPhone supplies the rear camera, object detection, framing, and recording UI.

## Current prototype

- The ESP32 starts an open access point named `SoccerTurret-ESP32` and serves an HTTPS control page at `https://192.168.4.1:8000`.
- The iPhone page requests the rear camera and microphone. It runs MediaPipe Object Detector on the phone, looks for COCO `person` and `sports ball` detections, smooths the target, and sends only normalized x/y coordinates to the ESP32.
- The page can frame the ball and nearby players with a digital crop, show boxes, and record the composed view with microphone audio where iPhone Safari supports canvas recording.
- The ESP32 controls independent yaw and pitch STEP/DIR drivers with bounded PID speed control, acceleration limiting, and software travel limits. Motor power starts disabled. **STOP holds position; the motor toggle releases driver torque.**

The default model is Google's [EfficientDet-Lite0 COCO model](https://developers.google.com/edge/mediapipe/solutions/vision/object_detector). It recognizes general `person` and `sports ball` classes; it is not a soccer-trained model and may miss a small ball across a full-size field. Treat this as a first field prototype, not dependable game coverage. A soccer-specific detector and temporal ball tracker will likely be needed.

## iPhone and zoom limits

The current web version digitally crops the camera stream and records the cropped canvas. That works as a browser-side zoom control but reduces detail as it zooms in. It does not switch iPhone lenses or directly command optical/hardware zoom. Apple exposes smooth capture zoom through AVFoundation's `videoZoomFactor` and ramp API, so a native iOS camera app is the path to reliable lens control and full-quality recording. ONNX Runtime's current browser support table lists WebAssembly on iOS Safari but not its WebGPU execution provider; the web prototype therefore has CPU/WASM fallback and must be measured on the actual iPhone.

The model runtime and model file download from Google's and jsDelivr's servers on first start. The ESP32 AP has no internet connection; the iPhone must still be able to reach those hosts over cellular. Camera video is not sent over Wi-Fi. Only the x/y target requests reach the ESP32.

## Hardware

Assumes a classic ESP32 DevKit / ESP32-WROOM and two independent A4988 or DRV8825-style STEP/DIR drivers:

| ESP32 pin | Connects to |
|---|---|
| GPIO 26 | Yaw driver STEP |
| GPIO 27 | Yaw driver DIR |
| GPIO 33 | Pitch driver STEP |
| GPIO 32 | Pitch driver DIR |
| GPIO 25 | Both driver ENABLE pins (active LOW; optional) |
| GND | Both driver logic grounds |

Use a separate motor supply sized for the motors and driver current limit. Connect the ESP32, drivers, and supply grounds together. Never connect a stepper motor directly to ESP32 pins. Verify the driver's input thresholds accept 3.3 V logic.

Default soft travel limits are ±70° yaw, +35° up, and −25° down. At boot, firmware assumes the turret is centered and the camera is level. It has no encoder, homing routine, or physical limit switches; step counts cannot reveal missed steps. Counterbalance the pitch axis. Keep the mechanism clear, use the web motor toggle deliberately, and do not leave it unattended until homing and hard-limit protection are added.

## Build and use

1. Open this folder as a PlatformIO project and install its declared dependencies.
2. Set `STEPS_PER_REV`, `MICROSTEPS`, gear ratios, direction inversion, and software limits near the top of `src/main.cpp` for the mount.
3. Flash a classic ESP32 DevKit and open the serial monitor at 115200 baud. Wait for HTTPS certificate generation.
4. Join `SoccerTurret-ESP32` from the iPhone. If iOS warns there is no internet, stay connected; cellular access may still be used for model downloads.
5. Open **https://192.168.4.1:8000**, accept the local self-signed certificate warning, and grant camera/microphone permission.
6. Start the camera/tracker. Enable the motors only after checking the turret is clear. Use STOP to halt and hold the camera; disabling the motors releases holding torque.

The browser captures at up to 1080p for preview/recording and throttles object detection to about 8 inferences per second. If the first model load fails, check cellular access to jsDelivr and Google; the app shows the failed load stage.

## Next engineering steps

1. Verify the ESP32 yaw/pitch motion directions and soft limits with the camera unloaded and the assembly centered at boot.
2. Measure inference rate, ball recall, framing, and clip saving on the actual iPhone at the sideline distance.
3. Replace or fine-tune the general COCO model with soccer footage labeled for ball and players; add temporal tracking to bridge missed detections.
4. Add homing switches and a safe pitch counterbalance, then calibrate camera field of view and travel.
5. If the browser camera/recording path is too limited, move the camera and model to a native iOS app. Apple documents smooth zoom with [`AVCaptureDevice.videoZoomFactor`](https://developer.apple.com/documentation/avfoundation/avcapturedevice/videozoomfactor).

## References

- [MediaPipe Object Detector for Web](https://developers.google.com/edge/mediapipe/solutions/vision/object_detector/web_js)
- [MediaPipe Object Detector model overview and model benchmarks](https://developers.google.com/edge/mediapipe/solutions/vision/object_detector)
- [ONNX Runtime Web browser support](https://onnxruntime.ai/docs/get-started/with-javascript/web.html)
- [Apple AVFoundation camera zoom](https://developer.apple.com/documentation/avfoundation/avcapturedevice/videozoomfactor)
