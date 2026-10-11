# ESP32 Soccer Camera Turret

The ESP32 provides the yaw/pitch motor controller and an open local Wi-Fi access point. An iPhone joins the access point and opens the Safari control page. Camera capture, detection, preview, and recording run on the phone; the ESP32 receives only target coordinates and motor settings.

## Current prototype

- The ESP32 serves the control page at https://192.168.4.1:8000 on SoccerTurret-ESP32.
- Safari requests the rear camera at up to 3840×2160 and 30 fps; the page displays the actual negotiated camera dimensions and frame rate. It requests microphone access only when recording starts, and records video without audio if mic permission is denied.
- Detection uses MediaPipe Object Detector and the generic EfficientDet-Lite0 COCO model (person and sports ball classes). It is not soccer-trained and may miss a small ball at field distance.
- The default focus-of-play mode estimates where the action is centered. It combines the detected ball and its recent trajectory with nearby players' movement. During a brief ball miss it predicts the ball position; if the ball stays unavailable, it scores player groups by density, activity, movement toward the recent play, and continuity with the previous focus. Ball-only and active-player-group modes are available.
- Recording follows auto-framing without using the 1920-pixel preview canvas. When auto-framing can zoom, the page records a crop surface at the negotiated camera dimensions so the saved view follows the play; available camera hardware zoom is applied to its source frames. When auto-framing is off, MediaRecorder records the camera stream directly. The page reports negotiated capture dimensions and detects supported recording formats.
- The tracker uses camera hardware zoom if Safari exposes a zoom range and accepts the zoom constraint. Otherwise it applies digital crop to the preview and full-resolution recording surface.
- Cropped recording frames come from the phone's camera stream, preserving camera-pipeline stabilization when supplied. The phone-side target smoothing, ESP32 acceleration limiting, and PID motion control ease turret movement.
- ESP32 controls two STEP/DIR drivers with bounded PID speed control, acceleration limiting, and software travel limits. Motor power starts disabled. **STOP holds position; the motor toggle releases driver torque.**

The browser page is the intended setup; it does not require installing a native iPhone app. Safari supplies the camera MediaStream. Recording uses it directly when auto-framing is off, or crops from it at the negotiated camera dimensions when auto-framing is on. The page requests high-resolution capture, shows the dimensions and frame rate Safari actually returns, and detects recording formats available on that phone.

## Detection acceleration

Safari 26 adds WebGPU on iOS, which can run general GPU compute through WebKit's Metal path. ONNX Runtime's current support table still does not list Safari on iOS for its WebGPU provider, so the webpage cannot assume that ONNX inference will use it. The active MediaPipe detector tries its GPU delegate and falls back to CPU; that does not prove it uses WebGPU or Apple's Neural Engine. Core ML can use the Neural Engine, but Safari does not expose a direct Core ML API to the page. Test each browser/runtime path on the actual iPhone and report the selected delegate.

## Model status

The webpage still uses the COCO model as its active detector. A training pipeline now fine-tunes YOLO11n at 1280 pixels on a public CC BY 4.0 football dataset with 612 broadcast frames and 500 ball boxes, then evaluates and exports ONNX. That is a useful soccer baseline, but too little and too far from an iPhone sideline view to validate full-field ball tracking. The dataset source, attribution, and scope are documented in model/README.md; training data and generated weights stay under ignored work/ and are not committed.

The camera's aim point is therefore a play-focus estimate, not simply the ball coordinate: visible ball and nearby players establish the current action center; short ball gaps use trajectory prediction; longer gaps use active player movement and group continuity. This heuristic is a prototype and needs match footage evaluation, especially for possession changes, set pieces, and ball occlusion.

For a dependable soccer model, collect representative sideline iPhone clips from the intended pitch distance and label ball, player, referee, and optionally goal/field landmarks. Validate on held-out games and camera positions. Temporal ball tracking is also needed to bridge detector misses; a per-frame detector alone is not enough.

## Camera, network, and recording

The ESP32 access point has no internet connection. The phone can remain connected to that access point while reaching model assets over its cellular connection when iOS routes those requests there. The runtime and current model are loaded from external HTTPS hosts on first start; the page shows which loading stage failed. The camera video itself stays on the phone. Only control requests go to the ESP32.

The page asks the camera for 4K/30 as an ideal setting, not a guarantee. Safari may return a different resolution or frame rate; the camera pill shows the negotiated values. Recording uses that negotiated size and requests a high video bitrate. The recorded crop cannot create detail beyond the camera source, and long recordings require sufficient free phone storage and browser memory.

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

Use a separate motor supply sized for the motors and driver current limit. Connect the ESP32, drivers, and supply grounds together. Never connect a stepper motor directly to ESP32 pins. Verify that the driver's input thresholds accept 3.3 V logic.

Default soft travel limits are ±70° yaw, +35° up, and −25° down. At boot, firmware assumes the turret is centered and the camera is level. It has no encoder, homing routine, or physical limit switches; step counts cannot reveal missed steps. Counterbalance the pitch axis. Keep the mechanism clear and do not leave it unattended until homing and hard-limit protection are added.

## Build and use

1. Open this folder as a PlatformIO project and install its declared dependencies.
2. Set STEPS_PER_REV, MICROSTEPS, gear ratios, direction inversion, and software limits near the top of src/main.cpp for the mount.
3. Flash a classic ESP32 DevKit and open the serial monitor at 115200 baud. Wait for HTTPS certificate generation.
4. Join SoccerTurret-ESP32 from the iPhone. If iOS warns that it has no internet, stay connected; the phone may still route external asset requests through cellular.
5. Open https://192.168.4.1:8000, accept the local self-signed certificate warning, and grant camera permission. Grant microphone permission when starting a recording if you want audio.
6. Start the camera/tracker. Enable the motors only after checking the turret is clear. Use STOP to halt and hold the camera; disabling the motors releases holding torque.

The preview and detector are capped to a 1920-pixel-wide drawing surface and about 8 inferences per second to keep tracking responsive. The recording uses the source camera track instead.

## References

- [MediaPipe Object Detector for Web](https://developers.google.com/edge/mediapipe/solutions/vision/object_detector/web_js)
- [ONNX Runtime Web browser support](https://onnxruntime.ai/docs/get-started/with-javascript/web.html)
- [WebKit Safari 26 WebGPU support](https://webkit.org/blog/17333/webkit-features-in-safari-26-0/)
- [Apple Core ML](https://developer.apple.com/documentation/coreml)
- [WebKit MediaRecorder API](https://webkit.org/blog/11353/mediarecorder-api/)
- [W3C Media Capture and Streams](https://www.w3.org/TR/mediacapture-streams/)
