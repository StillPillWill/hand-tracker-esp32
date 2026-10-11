# ESP32 Soccer Camera Turret

The ESP32 provides the yaw/pitch motor controller and an open local Wi-Fi access point. An iPhone joins the access point and opens the Safari control page. Camera capture, detection, preview, and recording run on the phone; the ESP32 receives only target coordinates and motor settings.

## Current prototype

- The ESP32 serves the control page at https://192.168.4.1:8000 on SoccerTurret-ESP32.
- Safari requests the rear camera at up to 3840×2160 and 30 fps; the page displays the actual negotiated camera dimensions and frame rate. It requests microphone access only when recording starts, and records video without audio if mic permission is denied.
- Detection defaults to MediaPipe Object Detector and the generic EfficientDet-Lite0 COCO model (person and sports ball classes). Select the trained three-class soccer ONNX model from iPhone Files and tap **Load soccer model** to use ball/player/referee detections. The ONNX file is loaded into phone memory; it is not sent to the ESP32.
- The default focus-of-play mode estimates where the action is centered, rather than aiming only at the ball. It combines a visible or briefly predicted ball with nearby players. During longer occlusion it uses the fading recent ball position and scores player groups by proximity, movement toward play, convergence, activity, compactness, and continuity. Referees are not treated as play participants. Auto-frame stays wider around isolated detections and zooms in as a larger active group establishes the play. Ball-only and active-player-group modes are available.
- Recording follows auto-framing without using the 1920-pixel preview canvas. When auto-framing can zoom, the page records a crop surface at the negotiated camera dimensions so the saved view follows the play; available camera hardware zoom is applied to its source frames. When auto-framing is off, MediaRecorder records the camera stream directly. The page reports negotiated capture dimensions and detects supported recording formats.
- The tracker uses camera hardware zoom if Safari exposes a zoom range and accepts the zoom constraint. Otherwise it applies digital crop to the preview and full-resolution recording surface.
- Cropped recording frames come from the phone's camera stream, preserving camera-pipeline stabilization when supplied. The phone-side target smoothing, ESP32 acceleration limiting, and PID motion control ease turret movement.
- ESP32 controls two STEP/DIR drivers with bounded PID speed control, acceleration limiting, and software travel limits. Motor power starts disabled. **STOP holds position; the motor toggle releases driver torque.**

The browser page is the intended setup; it does not require installing a native iPhone app. Safari supplies the camera MediaStream. Recording uses it directly when auto-framing is off, or crops from it at the negotiated camera dimensions when auto-framing is on. The page requests high-resolution capture, shows the dimensions and frame rate Safari actually returns, and detects recording formats available on that phone.

## Detection acceleration

The soccer ONNX path loads ONNX Runtime Web 1.30.0 from jsDelivr. It requests WebNN's NPU device when the browser exposes `navigator.ml`, then tries WebGPU with WASM fallback, then WASM CPU. ONNX Runtime's current browser matrix does not list Safari on iOS for WebNN or WebGPU, so those paths are feature-detected and need testing on the target iPhone. Safari 26 does ship WebGPU, which runs general compute on the phone GPU through Metal; that does not establish Neural Engine use. The generic MediaPipe model tries its GPU delegate and falls back to CPU. The page reports the requested/selected runtime path and does not claim Core ML use without browser support.

## Model status

The webpage includes a generic COCO fallback and an ONNX runtime path for a custom soccer model. A training pipeline fine-tunes YOLO11n at 1280 pixels on a public CC BY 4.0 football dataset with 612 broadcast frames and 500 ball boxes, then evaluates and exports ONNX. The browser decoder expects fixed 1280×1280 input and the output class order `ball`, `player`, `referee`. That is a soccer baseline, but the broadcast dataset is too small and too far from an iPhone sideline view to validate full-field ball tracking. The dataset source, attribution, and scope are documented in model/README.md.

The camera's aim point is therefore a play-focus estimate, not simply the ball coordinate: visible ball and nearby players establish the current action center; short ball gaps use trajectory prediction; longer gaps use the last ball position as a decaying clue while scoring active player groups. This heuristic is a prototype and needs match footage evaluation, especially for possession changes, set pieces, camera movement, and ball occlusion.

For a dependable soccer model, collect representative sideline iPhone clips from the intended pitch distance and label ball, player, referee, and optionally goal/field landmarks. Validate on held-out games and camera positions. Temporal ball tracking is also needed to bridge detector misses; a per-frame detector alone is not enough.

## Camera, network, and recording

The ESP32 access point has no internet connection. The phone can remain connected to that access point while reaching external runtime/model hosts over cellular when iOS routes those requests there. The generic detector downloads its code and model on first use; the soccer detector runtime downloads on model load after you select its ONNX file from Files. The page shows the loading stage that failed. Camera video and inference stay on the phone; only control requests go to the ESP32.

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
