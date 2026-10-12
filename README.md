# ESP32 Soccer Camera Turret

The ESP32 provides the yaw/pitch motor controller and an open local Wi-Fi access point. An iPhone joins the access point and opens the Safari control page. Camera capture, detection, preview, and recording run on the phone; the ESP32 receives only target coordinates and motor settings.

## Current prototype

- The ESP32 serves the control page at https://192.168.4.1:8000 on SoccerTurret-ESP32.
- Safari requests the rear camera at up to 3840×2160 and 30 fps; the page displays the actual negotiated camera dimensions and frame rate. It requests microphone access only when recording starts, and records video without audio if mic permission is denied.
- The page includes a **Download + load soccer model** action for the bundled three-class ONNX model. On first use, it downloads the 11 MB model through the phone's internet connection and saves it in Safari storage when available. Later sessions can load the saved copy. An ONNX file picker remains available for a custom model. If no soccer model is loaded, the general MediaPipe EfficientDet-Lite0 COCO detector is available as a fallback.
- The default focus-of-play mode estimates where the action is centered, rather than aiming only at the ball. It combines a visible or briefly predicted ball with nearby players. During longer occlusion it uses the fading recent ball position and scores player groups by proximity, movement toward play, convergence, activity, compactness, and continuity. Referees are not treated as play participants. Auto-frame stays wider around isolated detections and zooms in as a larger active group establishes the play. Ball-only and active-player-group modes are available.
- The preview overlays detected balls and players; a green reticle marks the current calculated aim point and is labeled for the selected tracking mode.
- Recording follows auto-framing without using the 1920-pixel preview canvas. When auto-framing can zoom, the page records a crop surface at the negotiated camera dimensions so the saved view follows the play; available camera hardware zoom is applied to its source frames. When auto-framing is off, MediaRecorder records the camera stream directly. The page writes each one-second media chunk into Safari's origin-private file storage through a worker, so a match recording does not accumulate as one huge in-memory blob. A configurable time limit, free-space estimate, screen-wake request when supported, and saved-recording list support long sessions. Stop recording, then tap a clip link to save it in Files; delete exported clips from Safari storage to reclaim space.
- The tracker uses camera hardware zoom if Safari exposes a zoom range and accepts the zoom constraint. Otherwise it applies digital crop to the preview and full-resolution recording surface.
- Cropped recording frames come from the phone's camera stream, preserving camera-pipeline stabilization when supplied. The phone-side target smoothing, ESP32 acceleration limiting, and PID motion control ease turret movement.
- ESP32 controls two STEP/DIR drivers with bounded PID speed control, acceleration limiting, and software travel limits. Motor power starts disabled. **STOP holds position; the motor toggle releases driver torque.**

The browser page is the intended setup; it does not require installing a native iPhone app. Safari supplies the camera MediaStream. Recording uses it directly when auto-framing is off, or crops from it at the negotiated camera dimensions when auto-framing is on. The page requests high-resolution capture, shows the dimensions and frame rate Safari actually returns, and detects recording formats available on that phone. Long match recording requires Safari's Origin Private File System (iOS 15.4 or later), is unavailable in Private Browsing, and needs enough free phone storage. The page requests a screen wake lock where Safari supports it and tries to stop/finalize if it detects that Safari is leaving the foreground. Keep the phone awake and Safari visible because iOS can suspend browser pages.

## Detection acceleration

The soccer ONNX path loads ONNX Runtime Web 1.30.0 from jsDelivr. It requests WebNN's NPU device when the browser exposes `navigator.ml`, then tries WebGPU with WASM fallback, then WASM CPU. The ESP32 page response enables cross-origin isolation so compatible Safari versions can use WebAssembly threads; ONNX Runtime uses up to four phone CPU threads when `SharedArrayBuffer` is available and falls back to one thread otherwise. Pure WASM inference runs in ONNX Runtime's proxy worker to keep the camera and recording controls responsive. Its transferable input buffer is recreated for each frame because the browser detaches it when sending it to the worker. Cross-origin runtime and model downloads must allow CORS. ONNX Runtime's current browser matrix does not list Safari on iOS for WebNN or WebGPU, so those paths are feature-detected and need testing on the target iPhone. The model also fails ONNX Runtime Web's legacy WebGL provider on unsupported YOLO operators, so that provider is not used. Safari does not expose a direct Core ML/Neural Engine path to this page.

## Model status

The webpage includes a generic COCO fallback and the bundled soccer ONNX model at `model/soccer-yolo11n-1280.onnx`. The page can load that file from the repository over the phone's internet connection, then save it in Safari storage for later sessions. A training pipeline fine-tunes YOLO11n at 1280 pixels on a public CC BY 4.0 football dataset with 612 broadcast frames and 500 ball boxes, then evaluates and exports ONNX. The browser decoder expects fixed 1280×1280 input and the output class order `ball`, `player`, `referee`. That is a soccer baseline, but the broadcast dataset is too small and too far from an iPhone sideline view to validate full-field ball tracking. The dataset source, attribution, model license, and scope are documented in model/README.md.

The camera's aim point is therefore a play-focus estimate, not simply the ball coordinate: visible ball and nearby players establish the current action center; short ball gaps use trajectory prediction; longer gaps use the last ball position as a decaying clue while scoring active player groups. This heuristic is a prototype and needs match footage evaluation, especially for possession changes, set pieces, camera movement, and ball occlusion.

For a dependable soccer model, collect representative sideline iPhone clips from the intended pitch distance and label ball, player, referee, and optionally goal/field landmarks. Validate on held-out games and camera positions. Temporal ball tracking is also needed to bridge detector misses; a per-frame detector alone is not enough.

## Camera, network, and recording

The ESP32 access point has no internet connection. The phone can remain connected to that access point while reaching external runtime/model hosts over cellular when iOS routes those requests there. The generic detector downloads its code and model on first use; the soccer detector runtime downloads on model load after you select its ONNX file from Files. The page shows the loading stage that failed. Camera video and inference stay on the phone; only control requests go to the ESP32.

The page asks the camera for 4K/30 as an ideal setting, not a guarantee. Safari may return a different resolution or frame rate; the camera pill shows the negotiated values. Recording uses that negotiated size and requests a high video bitrate. The recording control estimates the storage needed for the selected time limit and refuses to start when Safari reports insufficient space. The recorded crop cannot create detail beyond the camera source. Safari can still stop capture if iOS suspends the page or the phone runs out of storage.

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
4. Join SoccerTurret-ESP32 from the iPhone. If iOS warns that it has no internet, stay connected; external model and runtime downloads use the phone's internet connection when available.
5. Open https://192.168.4.1:8000 and accept the local self-signed certificate warning. On first use, tap **Download + load soccer model**; Safari saves it locally when storage is available. If the phone cannot reach the repository, load the `.onnx` file from iPhone Files instead.
6. Tap **Start camera + tracker** and grant rear-camera permission. Grant microphone permission when starting a recording if you want audio. Enable the motors only after checking the turret is clear. Use STOP to halt and hold the camera; disabling the motors releases holding torque.

The preview processes each decoded camera frame with Safari's `requestVideoFrameCallback()` when available, avoiding duplicate canvas work at the display refresh rate; other browsers fall back to animation frames. The preview drawing surface is capped at 1920 pixels wide, and detector inference is capped at about 8 times per second. The recording uses the source camera track instead.

## References

- [MediaPipe Object Detector for Web](https://developers.google.com/edge/mediapipe/solutions/vision/object_detector/web_js)
- [ONNX Runtime Web browser support](https://onnxruntime.ai/docs/get-started/with-javascript/web.html)
- [WebKit Safari 26 WebGPU support](https://webkit.org/blog/17333/webkit-features-in-safari-26-0/)
- [Apple Core ML](https://developer.apple.com/documentation/coreml)
- [WebKit MediaRecorder API](https://webkit.org/blog/11353/mediarecorder-api/)
- [W3C Media Capture and Streams](https://www.w3.org/TR/mediacapture-streams/)
