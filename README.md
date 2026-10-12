# ESP32 Soccer Camera Turret

The ESP32 provides the yaw/pitch motor controller and an open local Wi-Fi access point with no password. An iPhone joins the access point and opens the Safari control page. Camera capture, detection, preview, and recording run on the phone; the ESP32 receives only target coordinates and motor settings.

## Features

- The ESP32 serves the control page at https://192.168.4.1:8000 on SoccerTurret-ESP32.
- Safari requests the rear camera at up to 3840×2160 and 30 fps; the page displays the actual negotiated camera dimensions and frame rate. It requests microphone access only when recording starts, and records video without audio if mic permission is denied.
- The default detector is a phone-view soccer model adapted from a general soccer baseline; WASM-only browsers download its smaller int8 build, while browsers with a supported ONNX accelerator use the float model. A **Load general soccer baseline** button remains available for broadcast-style footage. Both models stay on the phone. The first successful load caches the selected model and phone-side runtime assets in Safari storage for offline sessions when storage is available; iOS may clear site data. An ONNX file picker remains available for a custom model. If no soccer model is loaded, the general MediaPipe EfficientDet-Lite0 COCO detector is available as a fallback. See [model/README.md](model/README.md) for scores, licenses, and limitations.
- The default focus-of-play mode estimates where the action is centered, rather than aiming only at the ball. It combines a visible or briefly predicted ball with nearby players. During longer occlusion it uses the fading recent ball position and scores player groups by proximity, movement toward play, convergence, activity, compactness, and continuity. Referees are not treated as play participants. Auto-frame stays wider around isolated detections and zooms in as a larger active group establishes the play. Ball-only and active-player-group modes are available.
- The preview overlays detected balls and players; a green reticle marks the current calculated aim point and is labeled for the selected tracking mode.
- The camera panel reports rolling model inference latency and observed inference FPS, alongside the selected inference provider. Use those readings on the target iPhone to check performance under the actual camera and recording load.
- Recording follows auto-framing without using the 1920-pixel preview canvas. When auto-framing can zoom, the page records a crop surface at the negotiated camera dimensions so the saved view follows the play; available camera hardware zoom is applied to its source frames. When auto-framing is off, MediaRecorder records the camera stream directly. The page writes each one-second media chunk into Safari's origin-private file storage through a worker, so a match recording does not accumulate as one huge in-memory blob. A configurable time limit, free-space estimate, screen-wake request when supported, and saved-recording list support long sessions. Stop recording, then tap a clip link to save it in Files; delete exported clips from Safari storage to reclaim space.
- The tracker uses camera hardware zoom if Safari exposes a zoom range and accepts the zoom constraint. Otherwise it applies digital crop to the preview and full-resolution recording surface.
- Cropped recording frames come from the phone's camera stream, preserving camera-pipeline stabilization when supplied. The phone-side target smoothing, ESP32 acceleration limiting, and PID motion control ease turret movement.
- ESP32 controls two STEP/DIR drivers with bounded PID speed control, acceleration limiting, and software travel limits. Motor power starts disabled. **STOP holds position; the motor toggle releases driver torque.**

The browser page is the intended setup; it does not require installing a native iPhone app. Safari supplies the camera MediaStream. Recording uses it directly when auto-framing is off, or crops from it at the negotiated camera dimensions when auto-framing is on. The page requests high-resolution capture, shows the dimensions and frame rate Safari actually returns, and detects recording formats available on that phone. Long match recording requires Safari's Origin Private File System (iOS 15.4 or later), is unavailable in Private Browsing, and needs enough free phone storage. The page requests a screen wake lock where Safari supports it and tries to stop/finalize if it detects that Safari is leaving the foreground. Keep the phone awake and Safari visible because iOS can suspend browser pages.

## Detection acceleration

The soccer ONNX path loads ONNX Runtime Web 1.30.0 from jsDelivr. It requests WebNN/NPU only on supported Chromium/Windows builds and WebGPU only on supported Chromium platforms, requesting a high-performance adapter before session creation. On iPhone, it selects the smaller int8 phone-view model and runs the tested WebAssembly CPU path with up to four threads when `SharedArrayBuffer` is available. Safari can expose `navigator.gpu`, but ONNX Runtime Web's published support matrix does not support WebGPU on iOS Safari, so the page does not attempt that unsupported provider. WebGPU uses the graphics GPU (Metal on Apple platforms), not the Neural Engine. A webpage cannot call Apple's Core ML/Neural Engine directly. The ESP32 response enables cross-origin isolation so Safari can use WebAssembly threads; the runtime uses one thread if `SharedArrayBuffer` is unavailable. WASM inference runs in ONNX Runtime's proxy worker to keep camera and recording controls responsive. Its transferable input buffer is recreated for each frame because the browser detaches it when sending it to the worker. Cross-origin runtime and model downloads must allow CORS. The model also fails ONNX Runtime Web's legacy WebGL provider on unsupported YOLO operators, so that provider is not used.

## Model status

The webpage includes a generic COCO fallback, the phone-view float model at `model/soccer-yolo11n-1280-mobile.onnx`, its WASM-only int8 build at `model/soccer-yolo11n-1280-mobile-int8.onnx`, and the general soccer baseline at `model/soccer-yolo11n-1280.onnx`. Both phone-view variants use fixed 1280×1280 input, output classes in the order `ball`, `player`, `referee`, and use a 0.18 ball threshold. The general baseline uses a 0.12 ball threshold. The phone-view adaptation improves results on held-out public handheld soccer footage with machine-generated labels, while scoring slightly lower on broadcast validation. The public data cannot establish performance from the intended iPhone mount. Dataset sources, attribution, model licenses, metrics, and limitations are documented in [model/README.md](model/README.md).

The camera aim is a play-focus estimate, not just the ball coordinate: visible ball and nearby players establish the action center; short ball gaps use trajectory prediction; longer gaps use the fading recent ball position while scoring active player groups. This heuristic still needs field evaluation for possession changes, set pieces, camera movement, and ball occlusion.

The model choice uses public held-out phone-view footage because no personal camera clip is required for setup. On a 449-frame sample from seven public camera streams, it improves aggregate ball recall at similar precision and improves player precision and recall, but performance varies sharply by view. The annotations are machine-generated, so these scores are diagnostics; accuracy from the exact iPhone, mount, distance, and lighting remains unmeasured. The inference readout exposes on-device latency and rate for a later live check.

## Camera, network, and recording

The ESP32 access point has no internet connection. The phone can remain connected to that access point while reaching external runtime/model hosts over cellular when iOS routes those requests there. The page installs a same-origin service worker that caches successful CORS-enabled requests to its runtime and MediaPipe hosts. The selected ONNX model is stored separately in IndexedDB. Load the selected model once while the phone has internet; later inference can use cached assets if Safari retains the site's storage. The page shows when runtime caching fails. Camera video and inference stay on the phone; only control requests go to the ESP32.

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

1. Install PlatformIO Core or the VS Code PlatformIO extension, connect a classic ESP32 DevKit, and open this folder as the project.
2. Set `STEPS_PER_REV`, `MICROSTEPS`, gear ratios, direction inversion, and software limits near the top of `src/main.cpp` for the actual mount. Center and level the camera before power-up; the firmware has no homing switches or encoders.
3. Build with `pio run`. Flash with `pio run -t upload` (add `--upload-port COMx` if PlatformIO does not select the ESP32 port). Open serial at 115200 baud with `pio device monitor -b 115200 --port COMx`. On first boot, wait for certificate generation; later boots load the same certificate and key from ESP32 NVS.
4. Join **SoccerTurret-ESP32** from the iPhone; the AP has no password. If iOS says the AP has no internet, stay connected so the phone can reach the ESP32 while using cellular for model/runtime downloads.
5. Open **https://192.168.4.1:8000** and accept the local self-signed certificate warning on first visit so Safari can grant camera access. The certificate identity is stored in NVS, so it remains stable across reboots. `localhost` would point to the iPhone itself; use the ESP32 address above. Load the phone-view model; use **Load general soccer baseline** for a broadcast-style view. If GitHub is unreachable, choose the `.onnx` file from iPhone Files instead.
6. Tap **Start camera + tracker** and grant rear-camera permission. Grant microphone permission when starting a recording if you want audio. Enable the motors only after checking the turret is clear. Use STOP to halt and hold the camera; disabling the motors releases holding torque.

The preview processes each decoded camera frame with Safari's `requestVideoFrameCallback()` when available, avoiding duplicate canvas work at the display refresh rate; other browsers fall back to animation frames. The preview drawing surface is capped at 1920 pixels wide, and detector inference is capped at about 8 times per second. The recording uses the source camera track instead.

## References

- [MediaPipe Object Detector for Web](https://developers.google.com/edge/mediapipe/solutions/vision/object_detector/web_js)
- [ONNX Runtime Web browser support](https://onnxruntime.ai/docs/get-started/with-javascript/web.html)
- [WebKit Safari 26 WebGPU support](https://webkit.org/blog/17333/webkit-features-in-safari-26-0/)
- [Apple Core ML](https://developer.apple.com/documentation/coreml)
- [WebKit MediaRecorder API](https://webkit.org/blog/11353/mediarecorder-api/)
- [W3C Media Capture and Streams](https://www.w3.org/TR/mediacapture-streams/)
