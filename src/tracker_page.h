#pragma once

const char TRACKER_HTML[] PROGMEM = R"TRACKER_PAGE(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
  <meta name="color-scheme" content="dark">
  <title>Soccer Turret</title>
  <style>
    :root{font-family:system-ui,-apple-system,sans-serif;color:#eef2ff;background:#0c1220}*{box-sizing:border-box}body{margin:0;padding:16px;max-width:900px;margin-inline:auto;padding-bottom:calc(18px + env(safe-area-inset-bottom))}
    h1{font-size:1.5rem;margin:0 0 5px}.sub,.small{color:#aab6d1}.sub{margin:0 0 14px}.small{font-size:.88rem;line-height:1.4}.card{background:#182235;border:1px solid #34415c;border-radius:16px;padding:14px;margin:12px 0}
    h2{font-size:1rem;margin:0 0 12px}.row{display:flex;gap:9px;flex-wrap:wrap;align-items:center}.row>*{flex:1 1 130px}button,select{border:0;border-radius:10px;padding:12px 14px;font-size:.98rem;font-weight:650;background:#819cff;color:#091222}button.stop{background:#ff6d7e}button.enabled{background:#75d99a}button.secondary{background:#34435e;color:#eef2ff}button:disabled{opacity:.5}
    #status{min-height:1.4em;color:#d5def7;margin:10px 0}.pill{display:inline-block;padding:4px 9px;border:1px solid #52617f;border-radius:999px;margin:3px 5px 3px 0;font-size:.83rem}.setting{display:block;margin:13px 0;color:#eef2ff}.setting input{display:block;width:100%;margin-top:7px;accent-color:#819cff}.readout{color:#aab6d1;font-variant-numeric:tabular-nums}.check{display:flex;gap:9px;align-items:center;margin:12px 0}.check input{accent-color:#819cff;width:20px;height:20px}
    #stage{position:relative;overflow:hidden;background:#05080d;border-radius:12px;aspect-ratio:16/9}canvas{display:block;width:100%;height:100%;object-fit:contain}.overlay{position:absolute;inset:0;pointer-events:none}.manual{display:grid;grid-template-columns:repeat(3,1fr);gap:6px;max-width:360px;margin:auto}.manual button{padding:8px}.spacer{visibility:hidden}.recordingRow{display:flex;gap:8px;align-items:center;margin:8px 0}.recordingRow a{flex:1;min-width:0;color:#9eb5ff;word-break:break-all}.recordingRow button{flex:0 0 auto}.hint{margin-bottom:0}
  </style>
</head>
<body>
  <h1>Soccer Turret</h1>
  <p class="sub">iPhone camera · on-device player/ball detection · two-axis ESP32 tracking</p>

  <div class="card">
    <h2>Turret controls</h2>
    <div class="row"><button id="motorToggle">Enable motors</button><button id="emergencyStop" class="stop">STOP + hold</button></div>
    <p id="motorState" class="small">Motors disabled. Confirm the camera mount is clear before enabling.</p>
    <label class="setting">Yaw speed limit: <span id="yawSpeedReadout" class="readout">45°/s</span>
      <input id="yawSpeed" type="range" min="5" max="60" step="1" value="45">
    </label>
    <label class="setting">Pitch speed limit: <span id="pitchSpeedReadout" class="readout">30°/s</span>
      <input id="pitchSpeed" type="range" min="5" max="60" step="1" value="30">
    </label>
    <p class="small">Manual aim</p>
    <div class="manual" aria-label="Manual camera aim">
      <span class="spacer">.</span><button class="secondary" data-dy="-0.08">▲</button><span class="spacer">.</span>
      <button class="secondary" data-dx="-0.08">◀</button><button id="centerAim" class="secondary">Center</button><button class="secondary" data-dx="0.08">▶</button>
      <span class="spacer">.</span><button class="secondary" data-dy="0.08">▼</button><span class="spacer">.</span>
    </div>
    <p class="small hint">Soft limits are ±70° yaw, +35° up, and −25° down. The firmware assumes the mount boots centered and level; it has no homing switches yet.</p>
  </div>

  <div class="card">
    <h2>Camera and tracking</h2>
    <div class="row"><button id="start">Start camera + tracker</button><button id="stop" class="secondary" disabled>Stop camera</button></div>
    <p id="status">Ready. Load the included soccer model, or start with the general detector.</p>
    <span class="pill" id="trackingState">No camera</span><span class="pill" id="targetReadout">Aim: center</span><span class="pill" id="zoomReadout">Zoom: 1.0×</span><span class="pill" id="cameraReadout">Camera: not started</span>
    <label class="setting">Track mode
      <select id="trackMode"><option value="play">Focus of play</option><option value="ball">Ball only</option><option value="players">Active player group</option></select>
    </label>
    <p class="small">Focus of play combines the ball and nearby players when visible. During occlusion it predicts the ball, then uses its fading recent position to score player groups by proximity, movement, convergence, activity, and continuity. Auto-frame keeps a wider field view for an isolated ball or player, then zooms further as it sees a larger active group.</p>
    <div class="row"><button id="loadBundledSoccerModel">Download + load soccer model</button><button id="loadSavedSoccerModel" class="secondary" disabled>Load saved model</button><button id="useGenericDetector" class="secondary" disabled>Use general detector</button></div>
    <p class="small">First use downloads the included 11 MB soccer model through the phone's internet connection and saves it on this phone when storage is available. When connected to the ESP32 access point, the phone can use cellular for this download. To use a different model, choose an ONNX file below.</p>
    <label class="setting">Use a different soccer model (.onnx)
      <input id="soccerModelFile" type="file" accept=".onnx,application/octet-stream">
    </label>
    <div class="row"><button id="loadSoccerModel" class="secondary" disabled>Load chosen model</button></div>
    <p id="detectorReadout" class="small">Active detector: general COCO model. Choose a soccer ONNX file to replace it.</p>
    <label class="setting">Maximum zoom: <span id="maxZoomReadout" class="readout">3.0×</span>
      <input id="maxZoom" type="range" min="1" max="4" step="0.1" value="3">
    </label>
    <label class="setting">Tracking smoothing: <span id="smoothingReadout" class="readout">65%</span>
      <input id="smoothing" type="range" min="0" max="90" step="1" value="65">
    </label>
    <label class="check"><input id="autoZoom" type="checkbox" checked><span>Auto-frame the detected play</span></label>
    <label class="setting">Recording time limit: <span id="recordMinutesReadout" class="readout">120 min</span>
      <input id="recordMinutes" type="range" min="15" max="180" step="15" value="120">
    </label>
    <p id="recordStorageReadout" class="small">Checking Safari storage…</p>
    <div class="row"><button id="record" class="secondary" disabled>Record match</button><button id="stopRecord" class="secondary" disabled>Stop &amp; save clip</button></div>
    <p class="small">The page requests up to 4K at 30 fps and shows the camera settings the phone actually supplies. When auto-framing can zoom, recording uses a full-resolution crop surface so the saved view follows the play; hardware camera zoom is also applied when available. The crop uses phone camera frames, preserving any stabilization supplied by its camera pipeline; tracking smoothing and motor acceleration limits also ease turret movement. With auto-framing off, it records the camera stream directly. Microphone access is requested only when recording starts.</p>
    <p class="small">Recordings are written to Safari storage in one-second chunks so a full match does not build one huge in-memory video. Keep Safari open in the foreground while recording. After stopping, tap a clip below to save it to Files; delete exported clips here to free phone storage. Long recordings need Safari storage enabled and enough free space.</p>
    <p class="small">Detection and the selected model stay on the iPhone. The custom soccer model requests WebNN/NPU when exposed by the browser, then tries WebGPU and WebAssembly. Safari does not expose a direct Core ML/Neural Engine path to this page; WebGPU uses the phone GPU, not its Neural Engine.</p>
    <div id="recordingLibrary" aria-live="polite"></div>
  </div>

  <div class="card"><div id="stage"><canvas id="canvas"></canvas></div>
    <p class="small">The green reticle marks the calculated aim point. In Focus of play mode it follows the ball and nearby players, then the active group if the ball is hidden. Use a stable tripod and keep the full yaw/pitch travel clear.</p></div>

  <video id="video" playsinline muted hidden></video>
  <script type="module">
    const MODEL = 'https://storage.googleapis.com/mediapipe-tasks/object_detector/efficientdet_lite0_uint8.tflite';
    const video = document.querySelector('#video');
    const canvas = document.querySelector('#canvas');
    const ctx = canvas.getContext('2d', { alpha: false });
    const statusEl = document.querySelector('#status');
    const trackingState = document.querySelector('#trackingState');
    const targetReadout = document.querySelector('#targetReadout');
    const zoomReadout = document.querySelector('#zoomReadout');
    const cameraReadout = document.querySelector('#cameraReadout');
    const startBtn = document.querySelector('#start');
    const stopBtn = document.querySelector('#stop');
    const motorBtn = document.querySelector('#motorToggle');
    const motorState = document.querySelector('#motorState');
    const emergencyStop = document.querySelector('#emergencyStop');
    const yawSpeed = document.querySelector('#yawSpeed');
    const pitchSpeed = document.querySelector('#pitchSpeed');
    const smoothing = document.querySelector('#smoothing');
    const maxZoom = document.querySelector('#maxZoom');
    const autoZoom = document.querySelector('#autoZoom');
    const trackMode = document.querySelector('#trackMode');
    const soccerModelFile = document.querySelector('#soccerModelFile');
    const loadBundledSoccerModelBtn = document.querySelector('#loadBundledSoccerModel');
    const loadSoccerModelBtn = document.querySelector('#loadSoccerModel');
    const loadSavedSoccerModelBtn = document.querySelector('#loadSavedSoccerModel');
    const useGenericDetectorBtn = document.querySelector('#useGenericDetector');
    const detectorReadout = document.querySelector('#detectorReadout');
    const recordBtn = document.querySelector('#record');
    const stopRecordBtn = document.querySelector('#stopRecord');
    const recordMinutes = document.querySelector('#recordMinutes');
    const recordMinutesReadout = document.querySelector('#recordMinutesReadout');
    const recordStorageReadout = document.querySelector('#recordStorageReadout');
    const recordingLibrary = document.querySelector('#recordingLibrary');

    let detector = null, detectorKind = 'coco', detectorBusy = false, detectorFailed = false;
    let genericDelegate = 'GPU', modelLoading = false;
    let stream = null, running = false, motorsEnabled = false, startToken = 0;
    let frameRequestId = 0, frameRequestKind = 'animation';
    let lastInfer = 0, lastSend = 0, lastLockAt = 0, settingsTimer = 0;
    let detections = [], filteredX = 0.5, filteredY = 0.5, currentZoom = 1;
    let videoTrack = null, hardwareZoomRange = null, hardwareZoomActive = false;
    let hardwareZoomPending = false, hardwareZoomNow = 1, lastZoomApply = 0;
    let lastTargetX = -1, lastTargetY = -1, previousBall = null, lastBallAt = 0;
    let ballVelocity = { x: 0, y: 0 }, previousPlayers = [], lastPlayersAt = 0;
    let previousPlayFocus = null, previousPlayFocusAt = 0;
    let recorder = null, recordingChunks = [];
    let recordingCanvas = null, recordingCanvasContext = null;
    let recordingCaptureStream = null, recordingMicStream = null, recordingOutputStream = null;
    let recordingZoomMode = 'camera', lastRecordingFrameAt = 0;
    let recordingWriter = null, recordingFileHandle = null, recordingFolder = null;
    let recordingWriteQueue = Promise.resolve(), recordingWriteError = null;
    let recordingTimer = 0, recordingWakeLock = null, recordingFileName = '', recordingFinalizePromise = null;
    let recordingStorageInfo = null, recordingStorageAvailable = null, recordingStoragePersisted = false;
    const recordingObjectUrls = new Set();
    const clamp = (value, low, high) => Math.max(low, Math.min(high, value));
    const status = text => { statusEl.textContent = text; };

    function formatBytes(bytes) {
      if (!Number.isFinite(bytes) || bytes < 0) return 'unknown';
      if (bytes < 1024 ** 3) return `${(bytes / 1024 ** 2).toFixed(0)} MB`;
      return `${(bytes / 1024 ** 3).toFixed(1)} GB`;
    }

    function plannedRecordingBitrate() {
      if (!videoTrack) return 50000000;
      const settings = videoTrack?.getSettings ? videoTrack.getSettings() : {};
      return settings.width >= 3840 ? 50000000 : settings.width >= 1920 ? 24000000 : 12000000;
    }

    function updateRecordingPlan() {
      const minutes = Number(recordMinutes.value);
      recordMinutesReadout.textContent = `${minutes} min`;
      if (recordingStorageAvailable === false || !navigator.storage?.getDirectory) {
        recordStorageReadout.textContent = 'This Safari session does not expose local recording storage. Long match recording is unavailable; leave Private Browsing and reload.';
        return;
      }
      const bitrate = plannedRecordingBitrate();
      const expectedBytes = bitrate * minutes * 60 / 8;
      const freeBytes = recordingStorageInfo?.quota - recordingStorageInfo?.usage;
      const free = Number.isFinite(freeBytes) ? `${formatBytes(freeBytes)} available` : 'free space not reported';
      const quality = `${Math.round(bitrate / 1000000)} Mbps requested`;
      recordStorageReadout.textContent = `Safari storage: ${free} · estimated ${formatBytes(expectedBytes)} for ${minutes} min at ${quality}, before audio and overhead.`;
    }

    async function refreshRecordingStorageInfo() {
      recordingStorageInfo = null;
      recordingStorageAvailable = Boolean(navigator.storage?.getDirectory);
      if (!recordingStorageAvailable) {
        updateRecordingPlan();
        return;
      }
      try {
        await navigator.storage.getDirectory();
        if (navigator.storage?.estimate) recordingStorageInfo = await navigator.storage.estimate();
      } catch (_) {
        recordingStorageAvailable = false;
      }
      updateRecordingPlan();
    }

    function createRecordingWriter() {
      const workerSource = `
        let access = null, offset = 0, queue = Promise.resolve();
        self.onmessage = event => {
          const message = event.data;
          queue = queue.then(async () => {
            if (message.type === 'open') {
              access = await message.fileHandle.createSyncAccessHandle();
              access.truncate(0);
              offset = 0;
              self.postMessage({ type: 'ready', id: message.id });
            } else if (message.type === 'chunk') {
              if (!access) throw new Error('Recording storage is not open.');
              const bytes = new Uint8Array(await message.blob.arrayBuffer());
              let written = 0;
              while (written < bytes.length) {
                const count = access.write(bytes.subarray(written), { at: offset + written });
                if (!count) throw new Error('Safari could not write a video chunk to local storage.');
                written += count;
              }
              offset += written;
              access.flush();
              self.postMessage({ type: 'written', id: message.id, total: offset });
            } else if (message.type === 'close') {
              if (access) { access.flush(); access.close(); access = null; }
              self.postMessage({ type: 'closed', id: message.id, total: offset });
            }
          }).catch(error => {
            self.postMessage({ type: 'error', id: message.id, error: error && error.message ? error.message : String(error) });
          });
        };
      `;
      const workerUrl = URL.createObjectURL(new Blob([workerSource], { type: 'text/javascript' }));
      const worker = new Worker(workerUrl);
      const pending = new Map();
      let nextId = 0;
      const rejectPending = error => {
        for (const request of pending.values()) request.reject(error);
        pending.clear();
      };
      worker.onmessage = event => {
        const message = event.data;
        const request = pending.get(message.id);
        if (!request) return;
        pending.delete(message.id);
        if (message.type === 'error') request.reject(new Error(message.error));
        else request.resolve(message);
      };
      worker.onerror = event => rejectPending(new Error(event.message || 'Recording storage worker failed.'));
      return {
        request(type, payload = {}) {
          return new Promise((resolve, reject) => {
            const id = ++nextId;
            pending.set(id, { resolve, reject });
            try { worker.postMessage({ type, id, ...payload }); }
            catch (error) { pending.delete(id); reject(error); }
          });
        },
        dispose() {
          worker.terminate();
          URL.revokeObjectURL(workerUrl);
          rejectPending(new Error('Recording storage worker closed.'));
        }
      };
    }

    async function openRecordingStorage(name, bitrate, minutes) {
      if (!navigator.storage?.getDirectory) {
        throw new Error('Long match recording needs Safari local storage. Exit Private Browsing and reload this page.');
      }
      let estimate = null;
      try { estimate = await navigator.storage.estimate?.(); } catch (_) {}
      const freeBytes = estimate && Number.isFinite(estimate.quota) && Number.isFinite(estimate.usage)
        ? Math.max(0, estimate.quota - estimate.usage) : null;
      const expectedBytes = bitrate * minutes * 60 / 8;
      if (freeBytes != null && freeBytes < expectedBytes * 1.12) {
        throw new Error(`Safari reports ${formatBytes(freeBytes)} free; this recording may need about ${formatBytes(expectedBytes)}. Free phone storage or choose a shorter limit.`);
      }
      try { recordingStoragePersisted = Boolean(await navigator.storage.persist?.()); } catch (_) {
        recordingStoragePersisted = false;
      }
      const root = await navigator.storage.getDirectory();
      const folder = await root.getDirectoryHandle('soccer-recordings', { create: true });
      const fileHandle = await folder.getFileHandle(name, { create: true });
      const writer = createRecordingWriter();
      try {
        await writer.request('open', { fileHandle });
      } catch (error) {
        writer.dispose();
        await folder.removeEntry(name).catch(() => {});
        throw new Error(`Could not open Safari recording storage: ${error?.message || error}`);
      }
      recordingFolder = folder;
      recordingFileHandle = fileHandle;
      recordingFileName = name;
      recordingWriter = writer;
      recordingWriteQueue = Promise.resolve();
      recordingWriteError = null;
    }

    async function refreshRecordingLibrary() {
      for (const url of recordingObjectUrls) URL.revokeObjectURL(url);
      recordingObjectUrls.clear();
      recordingLibrary.replaceChildren();
      if (!navigator.storage?.getDirectory) {
        recordingLibrary.textContent = 'Saved recordings require Safari local storage.';
        return;
      }
      try {
        const root = await navigator.storage.getDirectory();
        const folder = await root.getDirectoryHandle('soccer-recordings', { create: true });
        const clips = [];
        for await (const [name, handle] of folder.entries()) {
          if (handle.kind !== 'file' || (!name.endsWith('.mp4') && !name.endsWith('.webm'))) continue;
          const file = await handle.getFile();
          if (file.size) clips.push({ name, file });
        }
        clips.sort((a, b) => b.name.localeCompare(a.name));
        if (!clips.length) {
          recordingLibrary.textContent = 'No saved recordings yet.';
          return;
        }
        for (const clip of clips) {
          const row = document.createElement('div');
          row.className = 'recordingRow';
          const link = document.createElement('a');
          link.href = URL.createObjectURL(clip.file);
          recordingObjectUrls.add(link.href);
          link.download = clip.name;
          link.textContent = `Save ${clip.name} to Files (${formatBytes(clip.file.size)})`;
          const remove = document.createElement('button');
          remove.type = 'button';
          remove.className = 'secondary';
          remove.textContent = 'Delete';
          remove.addEventListener('click', async () => {
            remove.disabled = true;
            try {
              await folder.removeEntry(clip.name);
              status(`Deleted ${clip.name} from Safari storage.`);
              await refreshRecordingStorageInfo();
              await refreshRecordingLibrary();
            } catch (error) {
              remove.disabled = false;
              status(`Could not delete the saved clip: ${error?.message || error}`);
            }
          });
          row.append(link, remove);
          recordingLibrary.append(row);
        }
      } catch (error) {
        recordingStorageAvailable = false;
        updateRecordingPlan();
        recordingLibrary.textContent = `Could not read saved recordings: ${error?.message || error}`;
      }
    }

    function setMotorUi() {
      motorBtn.textContent = motorsEnabled ? 'Disable motors' : 'Enable motors';
      motorBtn.classList.toggle('enabled', motorsEnabled);
      motorState.textContent = motorsEnabled
        ? 'Motors enabled. Keep the mount clear; use STOP if motion is unexpected.'
        : 'Motors disabled. Confirm the camera mount is clear before enabling.';
    }

    async function sendSettings() {
      const query = new URLSearchParams({
        enabled: motorsEnabled ? '1' : '0',
        yawSpeed: yawSpeed.value,
        pitchSpeed: pitchSpeed.value
      });
      const response = await fetch(`/settings?${query}`, { method: 'POST', cache: 'no-store' });
      if (!response.ok) throw new Error(`ESP32 rejected settings (${response.status})`);
    }

    async function setMotors(enabled) {
      const previous = motorsEnabled;
      motorsEnabled = enabled;
      setMotorUi();
      motorBtn.disabled = true;
      try {
        await sendSettings();
        lastTargetX = lastTargetY = -1;
        status(motorsEnabled ? 'Motors armed. They will follow the selected target.' : 'Motors disabled.');
      } catch (error) {
        motorsEnabled = previous;
        setMotorUi();
        status(`Could not update ESP32 motor state: ${error?.message || error}`);
      } finally {
        motorBtn.disabled = false;
      }
    }

    function queueSettings() {
      clearTimeout(settingsTimer);
      settingsTimer = setTimeout(() => sendSettings().catch(e => status(`Settings failed: ${e?.message || e}`)), 160);
    }

    async function sendTarget(x, y, force = false, visualServo = false) {
      if (!motorsEnabled) return;
      const now = performance.now();
      if (!force && (now - lastSend < 100 || (Math.abs(x - lastTargetX) < 0.008 && Math.abs(y - lastTargetY) < 0.008))) return;
      lastSend = now; lastTargetX = x; lastTargetY = y;
      try {
        const endpoint = visualServo
          ? `/aim?x=${x.toFixed(4)}&y=${y.toFixed(4)}&z=${(hardwareZoomActive ? hardwareZoomNow : 1).toFixed(2)}`
          : `/target?x=${x.toFixed(4)}&y=${y.toFixed(4)}`;
        const response = await fetch(endpoint, { cache: 'no-store' });
        if (!response.ok) throw new Error(`target request returned ${response.status}`);
      } catch (error) {
        status(`Lost connection to the ESP32: ${error?.message || error}`);
      }
    }

    function updateReadouts() {
      document.querySelector('#yawSpeedReadout').textContent = `${yawSpeed.value}°/s`;
      document.querySelector('#pitchSpeedReadout').textContent = `${pitchSpeed.value}°/s`;
      document.querySelector('#smoothingReadout').textContent = `${smoothing.value}%`;
      document.querySelector('#maxZoomReadout').textContent = `${Number(maxZoom.value).toFixed(1)}×`;
    }

    function updateCameraReadout() {
      if (!videoTrack) {
        cameraReadout.textContent = 'Camera: not started';
        updateRecordingPlan();
        return;
      }
      const settings = videoTrack.getSettings ? videoTrack.getSettings() : {};
      const width = settings.width || video.videoWidth;
      const height = settings.height || video.videoHeight;
      const fps = settings.frameRate ? ` @ ${Math.round(settings.frameRate)} fps` : '';
      const zoom = hardwareZoomActive ? ` · hardware zoom ${hardwareZoomNow.toFixed(1)}×` : ' · preview crop zoom';
      cameraReadout.textContent = width && height
        ? `Camera: ${width}×${height}${fps}${zoom}`
        : 'Camera: reading settings…';
      updateRecordingPlan();
    }

    async function applyHardwareZoom() {
      if (!hardwareZoomActive || !videoTrack || !hardwareZoomRange || hardwareZoomPending) return;
      const now = performance.now();
      if (now - lastZoomApply < 180) return;
      const maxAllowed = Math.max(hardwareZoomRange.min, Math.min(hardwareZoomRange.max, Number(maxZoom.value)));
      const wanted = clamp(currentZoom, hardwareZoomRange.min, maxAllowed);
      if (Math.abs(wanted - hardwareZoomNow) < 0.08) return;
      hardwareZoomPending = true;
      lastZoomApply = now;
      try {
        await videoTrack.applyConstraints({ advanced: [{ zoom: wanted }] });
        const actual = videoTrack.getSettings ? videoTrack.getSettings().zoom : undefined;
        hardwareZoomNow = Number.isFinite(actual) ? actual : wanted;
        updateCameraReadout();
      } catch (_) {
        hardwareZoomActive = false;
        hardwareZoomRange = null;
        updateCameraReadout();
      } finally {
        hardwareZoomPending = false;
      }
    }

    async function setupDetector() {
      genericDelegate = 'GPU';
      status('Loading MediaPipe object-detection code…');
      let vision;
      try {
        vision = await import('https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@1.1.0/vision_bundle.mjs');
      } catch (error) {
        throw new Error(`Could not load MediaPipe from jsDelivr: ${error?.message || error}`);
      }
      status('Loading the phone’s WebAssembly vision runtime…');
      let files;
      try {
        files = await vision.FilesetResolver.forVisionTasks('https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@1.1.0/wasm');
      } catch (error) {
        throw new Error(`Could not load the vision runtime: ${error?.message || error}`);
      }

      const options = {
        baseOptions: { modelAssetPath: MODEL, delegate: 'GPU' },
        runningMode: 'VIDEO', maxResults: 40, scoreThreshold: 0.25
      };
      status('Downloading the general object model from Google…');
      try {
        return await vision.ObjectDetector.createFromOptions(files, options);
      } catch (gpuError) {
        status('GPU delegate unavailable; retrying on the phone CPU…');
        options.baseOptions.delegate = 'CPU';
        genericDelegate = 'CPU';
        try {
          return await vision.ObjectDetector.createFromOptions(files, options);
        } catch (cpuError) {
          throw new Error(`Object model setup failed. GPU: ${gpuError?.message || gpuError}. CPU: ${cpuError?.message || cpuError}`);
        }
      }
    }

    const ORT_VERSION = '1.30.0';
    const ORT_DIST = `https://cdn.jsdelivr.net/npm/onnxruntime-web@${ORT_VERSION}/dist/`;
    const SOCCER_MODEL_URL = 'https://raw.githubusercontent.com/StillPillWill/hand-tracker-esp32/soccer-camera-turret/model/soccer-yolo11n-1280.onnx';
    const SOCCER_MODEL_NAME = 'soccer-yolo11n-1280.onnx';
    const SOCCER_INPUT_SIZE = 1280;
    const SOCCER_CLASS_NAMES = ['ball', 'player', 'referee'];
    const SOCCER_CLASS_THRESHOLDS = [0.26, 0.18, 0.18];

    function createSoccerOrtDetector(ort, session, provider, wasmThreads) {
      const inputName = session.inputNames[0];
      const outputName = session.outputNames[0];
      const inputSize = SOCCER_INPUT_SIZE;
      const area = inputSize * inputSize;
      const inputCanvas = document.createElement('canvas');
      inputCanvas.width = inputCanvas.height = inputSize;
      const inputContext = inputCanvas.getContext('2d', { alpha: false, willReadFrequently: true });
      if (!inputContext) throw new Error('Could not create the phone-side model input canvas.');

      function iou(a, b) {
        const left = Math.max(a.left, b.left);
        const top = Math.max(a.top, b.top);
        const right = Math.min(a.right, b.right);
        const bottom = Math.min(a.bottom, b.bottom);
        const intersection = Math.max(0, right - left) * Math.max(0, bottom - top);
        const union = (a.right - a.left) * (a.bottom - a.top) + (b.right - b.left) * (b.bottom - b.top) - intersection;
        return union > 0 ? intersection / union : 0;
      }

      function decode(output, sourceWidth, sourceHeight, scale, padX, padY) {
        const dims = output.dims || [];
        if (dims.length !== 3 || dims[0] !== 1) {
          throw new Error(`Expected a fixed-batch YOLO output, got [${dims.join(', ')}].`);
        }
        const features = SOCCER_CLASS_NAMES.length + 4;
        const channelsFirst = dims[1] === features;
        const channelsLast = dims[2] === features;
        if (!channelsFirst && !channelsLast) {
          throw new Error(`Expected ${features} YOLO features (box + ball/player/referee), got [${dims.join(', ')}].`);
        }
        const candidates = channelsFirst ? dims[2] : dims[1];
        const at = (feature, index) => channelsFirst
          ? output.data[feature * candidates + index]
          : output.data[index * features + feature];
        const boxes = [];
        for (let index = 0; index < candidates; index++) {
          let classId = -1;
          let confidence = 0;
          for (let classIndex = 0; classIndex < SOCCER_CLASS_NAMES.length; classIndex++) {
            const score = at(4 + classIndex, index);
            if (score > confidence) { confidence = score; classId = classIndex; }
          }
          if (classId < 0 || confidence < SOCCER_CLASS_THRESHOLDS[classId]) continue;
          const centerX = at(0, index);
          const centerY = at(1, index);
          const width = at(2, index);
          const height = at(3, index);
          if (![centerX, centerY, width, height, confidence].every(Number.isFinite) || width <= 0 || height <= 0) continue;
          const left = clamp((centerX - width / 2 - padX) / scale, 0, sourceWidth);
          const top = clamp((centerY - height / 2 - padY) / scale, 0, sourceHeight);
          const right = clamp((centerX + width / 2 - padX) / scale, 0, sourceWidth);
          const bottom = clamp((centerY + height / 2 - padY) / scale, 0, sourceHeight);
          if (right - left < 1 || bottom - top < 1) continue;
          boxes.push({ left, top, right, bottom, confidence, classId });
        }

        boxes.sort((a, b) => b.confidence - a.confidence);
        const selected = [];
        for (const box of boxes) {
          if (selected.some(other => other.classId === box.classId && iou(other, box) > 0.45)) continue;
          selected.push(box);
          if (selected.length >= 40) break;
        }
        return selected.map(box => ({
          boundingBox: {
            originX: box.left, originY: box.top,
            width: box.right - box.left, height: box.bottom - box.top
          },
          categories: [{ categoryName: SOCCER_CLASS_NAMES[box.classId], score: box.confidence }]
        }));
      }

      return {
        provider,
        wasmThreads,
        proxyWorker: provider === 'wasm',
        async warmup() {
          const warmupData = new Float32Array(area * 3);
          const tensor = new ort.Tensor('float32', warmupData, [1, 3, inputSize, inputSize]);
          const outputs = await session.run({ [inputName]: tensor });
          const output = outputs[outputName] || outputs[Object.keys(outputs)[0]];
          if (!output) throw new Error('The soccer model returned no output tensor.');
          decode(output, inputSize, inputSize, 1, 0, 0);
        },
        async detectForVideo(source) {
          const sourceWidth = source.videoWidth;
          const sourceHeight = source.videoHeight;
          if (!sourceWidth || !sourceHeight) return { detections: [] };
          const scale = Math.min(inputSize / sourceWidth, inputSize / sourceHeight);
          const resizedWidth = Math.round(sourceWidth * scale);
          const resizedHeight = Math.round(sourceHeight * scale);
          const padX = Math.floor((inputSize - resizedWidth) / 2);
          const padY = Math.floor((inputSize - resizedHeight) / 2);
          // The WASM proxy transfers tensor buffers to its worker and detaches
          // them here. Allocate per inference so later camera frames stay valid.
          const inputData = new Float32Array(area * 3);
          inputContext.fillStyle = 'rgb(114,114,114)';
          inputContext.fillRect(0, 0, inputSize, inputSize);
          inputContext.drawImage(source, 0, 0, sourceWidth, sourceHeight,
            padX, padY, resizedWidth, resizedHeight);
          const pixels = inputContext.getImageData(0, 0, inputSize, inputSize).data;
          for (let pixel = 0; pixel < area; pixel++) {
            const rgba = pixel * 4;
            inputData[pixel] = pixels[rgba] / 255;
            inputData[area + pixel] = pixels[rgba + 1] / 255;
            inputData[2 * area + pixel] = pixels[rgba + 2] / 255;
          }
          const tensor = new ort.Tensor('float32', inputData, [1, 3, inputSize, inputSize]);
          const outputs = await session.run({ [inputName]: tensor });
          const output = outputs[outputName] || outputs[Object.keys(outputs)[0]];
          return { detections: decode(output, sourceWidth, sourceHeight, scale, padX, padY) };
        },
        close() { session.release?.(); }
      };
    }

    async function setupSoccerDetector(modelBytes) {
      modelBytes = modelBytes instanceof Uint8Array ? modelBytes : new Uint8Array(modelBytes);
      if (!modelBytes.byteLength) throw new Error('The selected model file is empty.');
      const providers = [];
      if (navigator.ml) providers.push('webnn');
      if (navigator.gpu) providers.push('webgpu');
      providers.push('wasm');
      const canUseWasmThreads = window.crossOriginIsolated && typeof SharedArrayBuffer !== 'undefined';
      const wasmThreads = canUseWasmThreads
        ? Math.max(1, Math.min(4, Number(navigator.hardwareConcurrency) || 2))
        : 1;
      let lastError = null;
      for (const provider of providers) {
        try {
          status(provider === 'webnn'
            ? 'Loading ONNX Runtime and requesting the phone NPU…'
            : provider === 'webgpu'
              ? 'Loading ONNX Runtime and trying iPhone WebGPU…'
              : 'Loading ONNX Runtime WebAssembly fallback…');
          const runtimeFile = provider === 'webnn' ? 'ort.all.min.mjs'
            : provider === 'webgpu' ? 'ort.webgpu.min.mjs' : 'ort.wasm.min.mjs';
          const runtimeUrl = `${ORT_DIST}${runtimeFile}`;
          const ort = await import(runtimeUrl);
          ort.env.wasm.wasmPaths = ORT_DIST;
          ort.env.wasm.numThreads = wasmThreads;
          ort.env.wasm.proxy = provider === 'wasm';
          const executionProviders = provider === 'webnn'
            ? [{ name: 'webnn', deviceType: 'npu', powerPreference: 'high-performance' }, 'wasm']
            : provider === 'webgpu' ? ['webgpu', 'wasm'] : ['wasm'];
          const session = await ort.InferenceSession.create(modelBytes, {
            executionProviders, graphOptimizationLevel: 'all', logSeverityLevel: 3
          });
          let detector = null;
          try {
            detector = createSoccerOrtDetector(ort, session, provider, wasmThreads);
            status(provider === 'webnn' ? 'Preparing the soccer model on WebNN NPU…'
              : provider === 'webgpu' ? 'Preparing the soccer model on WebGPU…' : 'Preparing the soccer model on WASM…');
            await detector.warmup();
            return detector;
          } catch (error) {
            detector?.close?.();
            session.release?.();
            throw error;
          }
        } catch (error) {
          lastError = error;
        }
      }
      throw new Error(`Could not create the soccer detector. Check cellular access for the ONNX runtime. ${lastError?.message || lastError || ''}`);
    }

    function openSoccerModelDatabase() {
      return new Promise((resolve, reject) => {
        if (!window.indexedDB) { reject(new Error('This browser has no local model storage.')); return; }
        const request = indexedDB.open('soccer-turret-models', 1);
        request.onupgradeneeded = () => {
          if (!request.result.objectStoreNames.contains('models')) request.result.createObjectStore('models');
        };
        request.onsuccess = () => resolve(request.result);
        request.onerror = () => reject(request.error || new Error('Could not open local model storage.'));
      });
    }

    async function readSavedSoccerModel() {
      const database = await openSoccerModelDatabase();
      try {
        return await new Promise((resolve, reject) => {
          const request = database.transaction('models', 'readonly').objectStore('models').get('active');
          request.onsuccess = () => resolve(request.result || null);
          request.onerror = () => reject(request.error || new Error('Could not read the saved soccer model.'));
        });
      } finally {
        database.close();
      }
    }

    async function saveSoccerModel(modelBytes, name) {
      const database = await openSoccerModelDatabase();
      try {
        await new Promise((resolve, reject) => {
          const transaction = database.transaction('models', 'readwrite');
          const copy = modelBytes.buffer.slice(modelBytes.byteOffset, modelBytes.byteOffset + modelBytes.byteLength);
          transaction.objectStore('models').put({ name, bytes: copy, size: modelBytes.byteLength }, 'active');
          transaction.oncomplete = () => resolve();
          transaction.onerror = () => reject(transaction.error || new Error('Could not save the soccer model.'));
          transaction.onabort = () => reject(transaction.error || new Error('Local model storage is full.'));
        });
        return true;
      } finally {
        database.close();
      }
    }

    async function refreshSavedSoccerModelButton() {
      try {
        const saved = await readSavedSoccerModel();
        loadSavedSoccerModelBtn.disabled = !saved || running || modelLoading;
        loadSavedSoccerModelBtn.textContent = saved
          ? `Load saved soccer model (${(saved.size / 1048576).toFixed(1)} MB)`
          : 'Load saved model';
      } catch (_) {
        loadSavedSoccerModelBtn.disabled = true;
        loadSavedSoccerModelBtn.textContent = 'Load saved model';
      }
    }

    refreshSavedSoccerModelButton();

    function setSoccerDetector(soccerDetector, modelName) {
      detector?.close?.();
      detector = soccerDetector;
      detectorKind = 'soccer';
      detectorFailed = false;
      useGenericDetectorBtn.disabled = false;
      const wasmLabel = `${soccerDetector.wasmThreads}-thread WASM`;
      const providerLabel = soccerDetector.provider === 'webnn' ? `WebNN NPU requested · ${wasmLabel} fallback`
        : soccerDetector.provider === 'webgpu' ? `WebGPU · ${wasmLabel} fallback`
          : `WebAssembly CPU · ${wasmLabel} · proxy worker`;
      detectorReadout.textContent = `Active detector: soccer YOLO (${modelName}) · ${providerLabel}`;
    }

    async function activateSoccerModel(readModelBytes, modelName, remember) {
      if (running || modelLoading) throw new Error('Stop the camera before changing its detector.');
      modelLoading = true;
      startBtn.disabled = true;
      loadBundledSoccerModelBtn.disabled = true;
      soccerModelFile.disabled = true;
      loadSoccerModelBtn.disabled = true;
      loadSavedSoccerModelBtn.disabled = true;
      try {
        const modelBytes = await readModelBytes();
        status(`Loading soccer model (${(modelBytes.byteLength / 1048576).toFixed(1)} MB)…`);
        const soccerDetector = await setupSoccerDetector(modelBytes);
        setSoccerDetector(soccerDetector, modelName);
        if (remember) {
          let saved = false;
          try { saved = await saveSoccerModel(modelBytes, modelName); } catch (_) {}
          await refreshSavedSoccerModelButton();
          status(saved
            ? 'Soccer model loaded and saved on this phone. Start the rear camera to track play.'
            : 'Soccer model loaded for this session. Browser storage was unavailable; select the file again after reloading.');
        } else {
          status('Saved soccer model loaded on the phone. Start the rear camera to track play.');
        }
      } finally {
        modelLoading = false;
        startBtn.disabled = running;
        loadBundledSoccerModelBtn.disabled = running;
        soccerModelFile.disabled = running;
        loadSoccerModelBtn.disabled = running || !soccerModelFile.files?.length;
        await refreshSavedSoccerModelButton();
      }
    }

    async function loadSoccerModel() {
      const file = soccerModelFile.files?.[0];
      if (!file) throw new Error('Choose the soccer .onnx file first.');
      return activateSoccerModel(() => file.arrayBuffer().then(bytes => new Uint8Array(bytes)), file.name, true);
    }

    async function downloadBundledSoccerModel() {
      return activateSoccerModel(async () => {
        status('Downloading the included soccer model from GitHub through the phone’s internet connection…');
        const response = await fetch(SOCCER_MODEL_URL, { mode: 'cors', cache: 'no-store' });
        if (!response.ok) throw new Error(`Could not download the included model (${response.status}). Check the phone’s cellular/internet connection or choose an ONNX file.`);
        const modelBytes = new Uint8Array(await response.arrayBuffer());
        if (modelBytes.byteLength < 1000000) throw new Error('The downloaded model is incomplete. Check the phone’s internet connection and retry.');
        return modelBytes;
      }, SOCCER_MODEL_NAME, true);
    }

    async function loadSavedSoccerModel() {
      const saved = await readSavedSoccerModel();
      if (!saved?.bytes) throw new Error('There is no saved soccer model on this phone.');
      return activateSoccerModel(() => Promise.resolve(new Uint8Array(saved.bytes)), saved.name || 'saved model', false);
    }

    function useGenericDetector() {
      if (running || modelLoading) {
        status('Stop the camera before changing its detector.');
        return;
      }
      detector?.close?.();
      detector = null;
      detectorKind = 'coco';
      detectorFailed = false;
      useGenericDetectorBtn.disabled = true;
      detectorReadout.textContent = 'Active detector: general COCO model. Choose a soccer ONNX file to replace it.';
      status('General COCO detector selected. Start the camera to load it.');
    }

    function readDetection(detection) {
      const category = [...(detection.categories || [])].sort((a, b) => b.score - a.score)[0];
      if (!category) return null;
      const name = String(category.categoryName || category.displayName || '').toLowerCase();
      const box = detection.boundingBox;
      if (!box || !video.videoWidth || !video.videoHeight) return null;
      const x = box.originX / video.videoWidth;
      const y = box.originY / video.videoHeight;
      const w = box.width / video.videoWidth;
      const h = box.height / video.videoHeight;
      return { name, score: category.score, x, y, w, h, cx: x + w / 2, cy: y + h / 2 };
    }

    function chooseBall(balls, now) {
      if (!balls.length) return null;
      const recent = previousBall && now - lastBallAt < 1400;
      balls.sort((a, b) => {
        const scoreA = a.score - (recent ? Math.hypot(a.cx - previousBall.cx, a.cy - previousBall.cy) * 0.65 : 0);
        const scoreB = b.score - (recent ? Math.hypot(b.cx - previousBall.cx, b.cy - previousBall.cy) * 0.65 : 0);
        return scoreB - scoreA;
      });
      if (previousBall) {
        const dt = (now - lastBallAt) / 1000;
        if (dt > 0.04 && dt < 1.4) {
          const measuredX = (balls[0].cx - previousBall.cx) / dt;
          const measuredY = (balls[0].cy - previousBall.cy) / dt;
          ballVelocity.x = ballVelocity.x * 0.35 + measuredX * 0.65;
          ballVelocity.y = ballVelocity.y * 0.35 + measuredY * 0.65;
        } else {
          ballVelocity = { x: 0, y: 0 };
        }
      }
      previousBall = { ...balls[0] };
      lastBallAt = now;
      return balls[0];
    }

    function updatePlayerMotion(players, now) {
      const dt = lastPlayersAt ? (now - lastPlayersAt) / 1000 : 0;
      const used = new Set();
      for (const player of players) {
        let nearest = -1;
        let nearestDistance = 0.14;
        for (let i = 0; i < previousPlayers.length; i++) {
          if (used.has(i)) continue;
          const old = previousPlayers[i];
          const distance = Math.hypot(player.cx - old.cx, player.cy - old.cy);
          if (distance < nearestDistance) {
            nearest = i;
            nearestDistance = distance;
          }
        }
        player.motion = 0;
        player.vx = 0;
        player.vy = 0;
        if (nearest >= 0 && dt > 0.04 && dt < 1) {
          used.add(nearest);
          const old = previousPlayers[nearest];
          const measuredVx = (player.cx - old.cx) / dt;
          const measuredVy = (player.cy - old.cy) / dt;
          player.vx = old.vx * 0.35 + measuredVx * 0.65;
          player.vy = old.vy * 0.35 + measuredVy * 0.65;
          const speed = Math.hypot(player.vx, player.vy);
          player.motion = clamp(speed / 0.32, 0, 1);
        }
      }
      previousPlayers = players.map(player => ({ cx: player.cx, cy: player.cy, vx: player.vx, vy: player.vy }));
      lastPlayersAt = now;
    }

    function predictBall(now) {
      if (!previousBall) return null;
      const age = now - lastBallAt;
      if (age < 0 || age > 850) return null;
      const seconds = age / 1000;
      const cx = clamp(previousBall.cx + ballVelocity.x * seconds, 0, 1);
      const cy = clamp(previousBall.cy + ballVelocity.y * seconds, 0, 1);
      const w = previousBall.w || 0.025;
      const h = previousBall.h || 0.025;
      return {
        name: 'sports ball', score: previousBall.score, cx, cy, w, h,
        x: clamp(cx - w / 2, 0, 1), y: clamp(cy - h / 2, 0, 1)
      };
    }

    function recentBallHint(now) {
      if (!previousBall) return null;
      const age = now - lastBallAt;
      if (age < 0 || age > 2400) return null;
      const seconds = Math.min(age / 1000, 1.1);
      const cx = clamp(previousBall.cx + ballVelocity.x * seconds, 0, 1);
      const cy = clamp(previousBall.cy + ballVelocity.y * seconds, 0, 1);
      return {
        cx, cy,
        focusConfidence: clamp(1 - age / 2400, 0, 1)
      };
    }

    function playerEngagement(player, ballHint) {
      if (!ballHint) return 0;
      const dx = ballHint.cx - player.cx;
      const dy = ballHint.cy - player.cy;
      const distance = Math.hypot(dx, dy);
      const proximity = clamp(1 - distance / 0.42, 0, 1);
      if (!distance || !player.motion) return proximity * 0.35;
      const towardBall = (player.vx * dx + player.vy * dy) / (distance * Math.max(0.01, Math.hypot(player.vx, player.vy)));
      return proximity * (0.35 + player.motion * 0.35 + Math.max(0, towardBall) * 0.5);
    }

    function activePlayerGroup(players, ballHint = null, now) {
      if (!players.length) return null;
      const radius = 0.19;
      let best = null;
      for (const anchor of players) {
        const group = players.filter(player => Math.hypot(player.cx - anchor.cx, player.cy - anchor.cy) < radius);
        const totalWeight = group.reduce((sum, player) => sum + Math.max(0.15, player.score) * (1 + player.motion * 0.7), 0);
        const x = group.reduce((sum, player) => sum + player.cx * Math.max(0.15, player.score) * (1 + player.motion * 0.7), 0) / totalWeight;
        const y = group.reduce((sum, player) => sum + player.cy * Math.max(0.15, player.score) * (1 + player.motion * 0.7), 0) / totalWeight;
        const averageMotion = group.reduce((sum, player) => sum + player.motion, 0) / group.length;
        const averageConfidence = group.reduce((sum, player) => sum + player.score, 0) / group.length;
        const activeFraction = group.filter(player => player.motion >= 0.12).length / group.length;
        const meanRadius = group.reduce((sum, player) => sum + Math.hypot(player.cx - x, player.cy - y), 0) / group.length;
        const compactness = clamp(1 - meanRadius / radius, 0, 1);
        let convergence = 0;
        let movingCount = 0;
        for (const player of group) {
          const dx = x - player.cx;
          const dy = y - player.cy;
          const distance = Math.hypot(dx, dy);
          const speed = Math.hypot(player.vx, player.vy);
          if (distance < 0.015 || speed < 0.02) continue;
          convergence += Math.max(0, (player.vx * dx + player.vy * dy) / (speed * distance));
          movingCount++;
        }
        convergence = movingCount ? convergence / movingCount : 0;
        const hintConfidence = ballHint?.focusConfidence ?? 1;
        const ballProximity = ballHint
          ? Math.max(0, 1 - Math.hypot(x - ballHint.cx, y - ballHint.cy) / 0.45) * hintConfidence
          : 0;
        const engagement = group.reduce((sum, player) => sum + playerEngagement(player, ballHint), 0) / group.length * hintConfidence;
        const focusAge = Math.max(0, now - previousPlayFocusAt);
        const continuity = previousPlayFocus
          ? Math.max(0, 1 - Math.hypot(x - previousPlayFocus.x, y - previousPlayFocus.y) / 0.38)
            * clamp(1 - focusAge / 1400, 0, 1)
          : 0;
        const groupSize = clamp((group.length - 1) / 4, 0, 1);
        const score = groupSize * 0.32 + averageMotion * 0.20 + activeFraction * 0.10
          + ballProximity * 0.26 + engagement * 0.22 + convergence * 0.14
          + continuity * 0.18 + compactness * 0.12 + averageConfidence * 0.06;
        if (!best || score > best.score) {
          best = { group, x, y, score, engagement, activity: averageMotion, activeFraction,
            convergence, ballProximity, continuity, compactness, hintConfidence };
        }
      }
      return best;
    }

    function centerPlay(ball, players) {
      const ballWeight = 2.2;
      let totalWeight = ballWeight;
      let x = ball.cx * ballWeight;
      let y = ball.cy * ballWeight;
      for (const player of players) {
        const weight = Math.max(0.15, player.score)
          * (1 + player.motion * 0.55 + playerEngagement(player, ball) * 0.45);
        totalWeight += weight;
        x += player.cx * weight;
        y += player.cy * weight;
      }
      return { x: x / totalWeight, y: y / totalWeight };
    }

    function compositionZoomLimit(subjectCount, ballAnchored) {
      const userLimit = Number(maxZoom.value);
      const contextLimit = ballAnchored
        ? subjectCount === 0 ? 1.3 : subjectCount === 1 ? 1.8 : userLimit
        : subjectCount <= 1 ? 1.5 : subjectCount === 2 ? 2.2 : userLimit;
      return Math.min(userLimit, contextLimit);
    }

    function scheduleTrackerFrame() {
      if (!running || frameRequestId) return;
      if (typeof video.requestVideoFrameCallback === 'function') {
        frameRequestKind = 'video';
        frameRequestId = video.requestVideoFrameCallback(frame);
      } else {
        frameRequestKind = 'animation';
        frameRequestId = requestAnimationFrame(frame);
      }
    }

    function cancelTrackerFrame() {
      if (!frameRequestId) return;
      if (frameRequestKind === 'video') {
        if (typeof video.cancelVideoFrameCallback === 'function') {
          video.cancelVideoFrameCallback(frameRequestId);
        }
      } else {
        cancelAnimationFrame(frameRequestId);
      }
      frameRequestId = 0;
    }

    function calculateFocusOfPlay(ball, predictedBall, players, ballHint, now) {
      const playAnchor = ball || predictedBall;
      if (playAnchor) {
        const nearbyPlayers = players.filter(player => Math.hypot(player.cx - playAnchor.cx, player.cy - playAnchor.cy) < 0.34);
        const target = centerPlay(playAnchor, nearbyPlayers);
        return {
          target,
          framingSubjects: [playAnchor, ...nearbyPlayers],
          zoomLimit: compositionZoomLimit(nearbyPlayers.length, true),
          label: ball
            ? `Play focus · ball + ${nearbyPlayers.length} nearby players`
            : `Play focus · predicted ball + ${nearbyPlayers.length} nearby players`
        };
      }

      const group = activePlayerGroup(players, ballHint, now);
      if (!group) return null;
      const source = group.ballProximity > 0.15
        ? 'active group near recent ball position'
        : 'active player group';
      return {
        target: { x: group.x, y: group.y },
        framingSubjects: group.group,
        zoomLimit: compositionZoomLimit(group.group.length, false),
        label: `Play focus · ${source}, ${group.group.length} players`
      };
    }

    function unionBounds(items) {
      if (!items.length) return null;
      const left = Math.min(...items.map(d => d.x));
      const top = Math.min(...items.map(d => d.y));
      const right = Math.max(...items.map(d => d.x + d.w));
      const bottom = Math.max(...items.map(d => d.y + d.h));
      const padX = Math.max(0.025, (right - left) * 0.16);
      const padY = Math.max(0.025, (bottom - top) * 0.16);
      return { x: clamp(left - padX, 0, 1), y: clamp(top - padY, 0, 1),
        w: clamp(right + padX, 0, 1) - clamp(left - padX, 0, 1),
        h: clamp(bottom + padY, 0, 1) - clamp(top - padY, 0, 1) };
    }

    function updateTracking(result, now) {
      detections = (result.detections || []).map(readDetection).filter(Boolean);
      const players = detections.filter(d => d.name === 'person' || d.name === 'player');
      const balls = detections.filter(d => d.name === 'sports ball' || d.name === 'ball');
      updatePlayerMotion(players, now);
      const ball = chooseBall(balls, now);
      const predicted = ball || predictBall(now);
      const playHint = predicted || recentBallHint(now);
      let target = null;
      let framingSubjects = [];
      let focusZoomLimit = Number(maxZoom.value);
      let focusLabel = 'No active play detected';

      if (trackMode.value === 'ball' && ball) {
        target = { x: ball.cx, y: ball.cy };
        framingSubjects = [ball];
        focusLabel = 'Ball only · ' + Math.round(ball.score * 100) + '%';
      } else if (trackMode.value === 'play') {
        const focus = calculateFocusOfPlay(ball, predicted, players, playHint, now);
        if (focus) {
          target = focus.target;
          framingSubjects = focus.framingSubjects;
          focusZoomLimit = focus.zoomLimit;
          focusLabel = focus.label;
        }
      }

      if (!target && trackMode.value !== 'ball' && players.length) {
        const group = activePlayerGroup(players, playHint, now);
        if (group) {
          target = { x: group.x, y: group.y };
          framingSubjects = group.group;
          focusZoomLimit = compositionZoomLimit(group.group.length, false);
          focusLabel = `Active play · ${group.group.length} players, ${(group.engagement * 100).toFixed(0)}% ball engagement`;
        }
      }

      if (target) {
        previousPlayFocus = { x: target.x, y: target.y };
        previousPlayFocusAt = now;
        lastLockAt = now;
        const amount = 1 - Number(smoothing.value) / 100;
        filteredX += (target.x - filteredX) * amount;
        filteredY += (target.y - filteredY) * amount;
        trackingState.textContent = focusLabel;
        targetReadout.textContent = `Aim: ${(filteredX * 100).toFixed(0)}%, ${(filteredY * 100).toFixed(0)}%`;
        sendTarget(filteredX, filteredY, false, true);
      } else {
        trackingState.textContent = trackMode.value === 'ball' ? 'Waiting for ball' : 'No active play detected';
      }

      const bounds = unionBounds(framingSubjects);
      if (autoZoom.checked && bounds) {
        const fitHeight = 0.70 / Math.max(bounds.h, 0.08);
        const fitWidth = 0.82 / Math.max(bounds.w, 0.08);
        const relativeFit = Math.min(fitHeight, fitWidth);
        const wanted = hardwareZoomActive
          ? clamp(hardwareZoomNow * relativeFit, hardwareZoomRange.min,
              Math.max(hardwareZoomRange.min, Math.min(hardwareZoomRange.max, focusZoomLimit)))
          : clamp(relativeFit, 1, focusZoomLimit);
        currentZoom += (wanted - currentZoom) * 0.16;
      } else if (!autoZoom.checked) {
        currentZoom = 1;
      } else if (!target && now - lastLockAt > 900) {
        currentZoom += (1 - currentZoom) * 0.08;
      }
      applyHardwareZoom();
      zoomReadout.textContent = `Zoom: ${currentZoom.toFixed(1)}×`;
    }

    function drawFrame(now) {
      const sourceWidth = video.videoWidth;
      const sourceHeight = video.videoHeight;
      if (!sourceWidth || !sourceHeight) return;
      const outWidth = Math.min(sourceWidth, 1920);
      const outHeight = Math.round(outWidth * sourceHeight / sourceWidth);
      if (canvas.width !== outWidth || canvas.height !== outHeight) {
        canvas.width = outWidth;
        canvas.height = outHeight;
      }

      const zoom = hardwareZoomActive ? 1 : clamp(currentZoom, 1, Number(maxZoom.value));
      const cropWidth = sourceWidth / zoom;
      const cropHeight = sourceHeight / zoom;
      const cropX = clamp(filteredX * sourceWidth - cropWidth / 2, 0, sourceWidth - cropWidth);
      const cropY = clamp(filteredY * sourceHeight - cropHeight / 2, 0, sourceHeight - cropHeight);
      ctx.drawImage(video, cropX, cropY, cropWidth, cropHeight, 0, 0, outWidth, outHeight);

      const scaleX = outWidth / cropWidth;
      const scaleY = outHeight / cropHeight;
      ctx.lineWidth = Math.max(2, outWidth / 500);
      ctx.font = `600 ${Math.max(14, outWidth / 45)}px system-ui`;
      for (const d of detections) {
        const isBall = d.name === 'sports ball' || d.name === 'ball';
        const isPlayer = d.name === 'person' || d.name === 'player' || d.name === 'referee';
        if (!isBall && !isPlayer) continue;
        const x = (d.x * sourceWidth - cropX) * scaleX;
        const y = (d.y * sourceHeight - cropY) * scaleY;
        const w = d.w * sourceWidth * scaleX;
        const h = d.h * sourceHeight * scaleY;
        if (x + w < 0 || y + h < 0 || x > outWidth || y > outHeight) continue;
        ctx.strokeStyle = isBall ? '#ffd166' : '#67e8f9';
        ctx.strokeRect(x, y, w, h);
        ctx.fillStyle = ctx.strokeStyle;
        ctx.fillText(isBall ? 'BALL' : d.name === 'referee' ? 'REFEREE' : 'PLAYER', x + 3, Math.max(18, y - 4));
      }
      const focusAge = now - lastLockAt;
      if (focusAge >= 0 && focusAge < 900) {
        const focusX = (filteredX * sourceWidth - cropX) * scaleX;
        const focusY = (filteredY * sourceHeight - cropY) * scaleY;
        if (focusX >= 0 && focusX <= outWidth && focusY >= 0 && focusY <= outHeight) {
          const radius = Math.max(16, outWidth / 80);
          ctx.save();
          ctx.globalAlpha = 1 - 0.55 * (focusAge / 900);
          ctx.strokeStyle = '#a3e635';
          ctx.fillStyle = '#a3e635';
          ctx.lineWidth = Math.max(2, outWidth / 700);
          ctx.beginPath();
          ctx.arc(focusX, focusY, radius, 0, Math.PI * 2);
          ctx.moveTo(focusX - radius * 1.55, focusY);
          ctx.lineTo(focusX - radius * 0.55, focusY);
          ctx.moveTo(focusX + radius * 0.55, focusY);
          ctx.lineTo(focusX + radius * 1.55, focusY);
          ctx.moveTo(focusX, focusY - radius * 1.55);
          ctx.lineTo(focusX, focusY - radius * 0.55);
          ctx.moveTo(focusX, focusY + radius * 0.55);
          ctx.lineTo(focusX, focusY + radius * 1.55);
          ctx.stroke();
          const focusLabel = trackMode.value === 'play' ? 'PLAY FOCUS'
            : trackMode.value === 'ball' ? 'BALL AIM' : 'PLAYER GROUP';
          ctx.font = `700 ${Math.max(14, outWidth / 55)}px system-ui`;
          const labelWidth = ctx.measureText(focusLabel).width;
          const labelX = focusX + radius + 8 + labelWidth <= outWidth
            ? focusX + radius + 8 : Math.max(0, focusX - radius - 8 - labelWidth);
          ctx.fillText(focusLabel, labelX, clamp(focusY - radius, 18, outHeight - 4));
          ctx.restore();
        }
      }
      drawRecordingFrame(now);
    }

    function drawRecordingFrame(now) {
      if (!recordingCanvasContext || now - lastRecordingFrameAt < 33) return;
      const sourceWidth = video.videoWidth;
      const sourceHeight = video.videoHeight;
      if (!sourceWidth || !sourceHeight) return;
      lastRecordingFrameAt = now;

      const zoom = hardwareZoomActive ? 1 : clamp(currentZoom, 1, Number(maxZoom.value));
      const cropWidth = sourceWidth / zoom;
      const cropHeight = sourceHeight / zoom;
      const cropX = clamp(filteredX * sourceWidth - cropWidth / 2, 0, sourceWidth - cropWidth);
      const cropY = clamp(filteredY * sourceHeight - cropHeight / 2, 0, sourceHeight - cropHeight);
      recordingCanvasContext.drawImage(video, cropX, cropY, cropWidth, cropHeight,
        0, 0, recordingCanvas.width, recordingCanvas.height);
    }

    function frame(now) {
      frameRequestId = 0;
      if (!running) return;
      if (video.readyState >= 2) {
        if (detector && !detectorBusy && !detectorFailed && now - lastInfer >= 120) {
          lastInfer = now;
          detectorBusy = true;
          const inferenceToken = startToken;
          try {
            Promise.resolve(detector.detectForVideo(video, now))
              .then(result => {
                if (running && inferenceToken === startToken) updateTracking(result, performance.now());
              })
              .catch(error => {
                detectorFailed = true;
                status(`Detection stopped: ${error?.message || error}. Reload the model or use the generic detector.`);
              })
              .finally(() => { detectorBusy = false; });
          } catch (error) {
            detectorBusy = false;
            status(`Detection error: ${error?.message || error}`);
          }
        }
        drawFrame(now);
      }
      scheduleTrackerFrame();
    }

    async function startCameraAndTracking() {
      if (modelLoading) return;
      const request = ++startToken;
      startBtn.disabled = true;
      loadBundledSoccerModelBtn.disabled = true;
      soccerModelFile.disabled = true;
      loadSoccerModelBtn.disabled = true;
      loadSavedSoccerModelBtn.disabled = true;
      try {
        if (!window.isSecureContext || !navigator.mediaDevices?.getUserMedia) {
          throw new Error('Camera access needs HTTPS. Open https://192.168.4.1:8000 and accept the ESP32 certificate first.');
        }
        if (!stream) {
          status('Opening the iPhone back camera…');
          const videoConstraints = {
            width: { ideal: 3840 },
            height: { ideal: 2160 },
            aspectRatio: { ideal: 16 / 9 },
            frameRate: { ideal: 30, max: 30 }
          };
          try {
            stream = await navigator.mediaDevices.getUserMedia({
              video: { ...videoConstraints, facingMode: { exact: 'environment' } }, audio: false
            });
          } catch (error) {
            if (error?.name !== 'OverconstrainedError' && error?.constraint !== 'facingMode') throw error;
            status('Back-camera selection was unavailable; trying the available camera…');
            stream = await navigator.mediaDevices.getUserMedia({
              video: { ...videoConstraints, facingMode: { ideal: 'environment' } }, audio: false
            });
          }
          videoTrack = stream.getVideoTracks()[0] || null;
          if (videoTrack?.getCapabilities) {
            try {
              const zoom = videoTrack.getCapabilities().zoom;
              const minimumZoom = zoom ? Math.max(1, zoom.min) : 1;
              if (zoom && Number.isFinite(zoom.min) && Number.isFinite(zoom.max) && zoom.max > minimumZoom) {
                hardwareZoomRange = { min: minimumZoom, max: zoom.max };
                const initialZoom = videoTrack.getSettings ? videoTrack.getSettings().zoom : undefined;
                hardwareZoomNow = Number.isFinite(initialZoom) ? initialZoom : 1;
                hardwareZoomActive = typeof videoTrack.applyConstraints === 'function';
                currentZoom = clamp(hardwareZoomNow, hardwareZoomRange.min,
                  Math.max(hardwareZoomRange.min, Math.min(hardwareZoomRange.max, Number(maxZoom.value))));
              }
            } catch (_) {}
          }
          video.srcObject = stream;
          await video.play();
          updateCameraReadout();
        }
        if (request !== startToken) return;
        stopBtn.disabled = false;
        recordBtn.disabled = false;
        if (!detector) {
          let saved = null;
          try { saved = await readSavedSoccerModel(); } catch (_) {}
          if (saved?.bytes) {
            status('Loading the saved soccer model from this phone…');
            try {
              const savedDetector = await setupSoccerDetector(new Uint8Array(saved.bytes));
              if (request !== startToken) { savedDetector.close?.(); return; }
              setSoccerDetector(savedDetector, saved.name || 'saved model');
            } catch (error) {
              status(`Saved soccer model could not load (${error?.message || error}); using the general detector.`);
            }
          }
          if (!detector) {
            detector = await setupDetector();
            detectorKind = 'coco';
            detectorFailed = false;
            useGenericDetectorBtn.disabled = true;
            detectorReadout.textContent = `Active detector: general COCO · MediaPipe ${genericDelegate}`;
          }
        }
        if (request !== startToken) return;
        detectorFailed = false;
        detections = [];
        previousBall = null; lastBallAt = 0; ballVelocity = { x: 0, y: 0 };
        previousPlayers = []; lastPlayersAt = 0;
        previousPlayFocus = null; previousPlayFocusAt = 0;
        filteredX = filteredY = 0.5; lastLockAt = 0; lastInfer = 0;
        currentZoom = hardwareZoomActive ? hardwareZoomNow : 1;
        if (!running) { running = true; scheduleTrackerFrame(); }
        status('Tracking live. The phone sends only yaw/pitch targets to the ESP32.');
        startBtn.textContent = 'Tracker running';
      } catch (error) {
        if (request !== startToken) return;
        status(stream ? `Camera is on, but tracker setup failed: ${error?.message || error}` : `Could not start camera: ${error?.message || error}`);
        startBtn.textContent = detector ? 'Retry' : stream ? 'Retry tracker' : 'Start camera + tracker';
        startBtn.disabled = false;
        loadBundledSoccerModelBtn.disabled = false;
        soccerModelFile.disabled = false;
        loadSoccerModelBtn.disabled = !soccerModelFile.files?.length;
        await refreshSavedSoccerModelButton();
      }
    }

    function stopAndSaveRecording() {
      if (recordingFinalizePromise) return recordingFinalizePromise;
      if (!recorder || recorder.state === 'inactive') return Promise.resolve();
      stopRecordBtn.disabled = true;
      const activeRecorder = recorder;
      recordingFinalizePromise = new Promise(resolve => {
        activeRecorder.onstop = async () => {
          clearTimeout(recordingTimer);
          recordingTimer = 0;
          try {
            await recordingWriteQueue;
            if (recordingWriteError) throw recordingWriteError;
            if (!recordingWriter || !recordingFileHandle) throw new Error('Recording storage closed before the clip was finalized.');
            await recordingWriter.request('close');
            recordingWriter.dispose();
            recordingWriter = null;
            const file = await recordingFileHandle.getFile();
            if (!file.size) throw new Error('The recording file is empty.');
            await refreshRecordingLibrary();
            await refreshRecordingStorageInfo();
            status(`Clip ready in Safari storage (${formatBytes(file.size)}). Tap its link below to save it to Files.`);
          } catch (error) {
            recordingWriter?.dispose();
            recordingWriter = null;
            if (recordingFolder && recordingFileName) await recordingFolder.removeEntry(recordingFileName).catch(() => {});
            status(`Could not finish recording: ${error?.message || error}`);
          } finally {
            recordingChunks = [];
            recorder = null;
            recordingFileHandle = null; recordingFolder = null; recordingFileName = '';
            recordingCaptureStream?.getTracks().forEach(track => track.stop());
            recordingMicStream?.getTracks().forEach(track => track.stop());
            recordingCaptureStream = null; recordingMicStream = null; recordingOutputStream = null;
            recordingCanvasContext = null; recordingCanvas = null; recordingZoomMode = 'camera';
            autoZoom.disabled = false; maxZoom.disabled = false; recordMinutes.disabled = false;
            recordBtn.disabled = !running;
            stopRecordBtn.disabled = true;
            if (recordingWakeLock) {
              const wakeLock = recordingWakeLock;
              recordingWakeLock = null;
              try { await wakeLock.release(); } catch (_) {}
            }
            recordingFinalizePromise = null;
            resolve();
          }
        };
        try { activeRecorder.stop(); }
        catch (error) {
          recordingWriteError = error;
          status(`Could not stop recording cleanly: ${error?.message || error}`);
          activeRecorder.onstop?.();
        }
      });
      return recordingFinalizePromise;
    }

    async function stopCamera() {
      startToken++;
      running = false;
      cancelTrackerFrame();
      if (recorder && recorder.state !== 'inactive') await stopAndSaveRecording();
      if (detectorKind !== 'soccer') {
        detector?.close?.(); detector = null;
      }
      stream?.getTracks().forEach(track => track.stop());
      stream = null; video.srcObject = null;
      videoTrack = null; hardwareZoomRange = null; hardwareZoomActive = false;
      hardwareZoomPending = false; hardwareZoomNow = 1;
      updateCameraReadout();
      recordBtn.disabled = true; stopBtn.disabled = true;
      startBtn.disabled = false; startBtn.textContent = 'Start camera + tracker';
      loadBundledSoccerModelBtn.disabled = false;
      soccerModelFile.disabled = false;
      loadSoccerModelBtn.disabled = !soccerModelFile.files?.length;
      await refreshSavedSoccerModelButton();
      detections = []; trackingState.textContent = 'No camera';
      try { await fetch('/stop', { method: 'POST' }); } catch (_) {}
      status(motorsEnabled ? 'Camera stopped. Motors are holding their current position.' : 'Camera stopped. Motors remain disabled.');
    }

    motorBtn.addEventListener('click', () => setMotors(!motorsEnabled));
    emergencyStop.addEventListener('click', async () => {
      try { await fetch('/stop', { method: 'POST' }); status(motorsEnabled ? 'STOP sent. Motors are holding position.' : 'STOP sent. Motors are disabled.'); }
      catch (error) { status(`STOP request failed: ${error?.message || error}`); }
      lastTargetX = lastTargetY = -1;
    });
    yawSpeed.addEventListener('input', () => { updateReadouts(); queueSettings(); });
    pitchSpeed.addEventListener('input', () => { updateReadouts(); queueSettings(); });
    smoothing.addEventListener('input', updateReadouts);
    maxZoom.addEventListener('input', updateReadouts);
    autoZoom.addEventListener('change', () => { if (!autoZoom.checked) currentZoom = 1; });
    startBtn.addEventListener('click', startCameraAndTracking);
    stopBtn.addEventListener('click', stopCamera);
    loadBundledSoccerModelBtn.addEventListener('click', () => {
      downloadBundledSoccerModel().catch(error => status(`Included soccer model failed: ${error?.message || error}`));
    });
    loadSavedSoccerModelBtn.addEventListener('click', () => {
      loadSavedSoccerModel().catch(error => status(`Saved soccer model failed: ${error?.message || error}`));
    });
    soccerModelFile.addEventListener('change', () => {
      loadSoccerModelBtn.disabled = running || !soccerModelFile.files?.length;
    });
    loadSoccerModelBtn.addEventListener('click', () => {
      loadSoccerModel().catch(error => status(`Soccer model failed: ${error?.message || error}`));
    });
    useGenericDetectorBtn.addEventListener('click', useGenericDetector);

    document.querySelectorAll('[data-dx], [data-dy]').forEach(button => button.addEventListener('click', () => {
      if (running) { status('Stop tracking before using manual aim.'); return; }
      filteredX = clamp(filteredX + Number(button.dataset.dx || 0), 0, 1);
      filteredY = clamp(filteredY + Number(button.dataset.dy || 0), 0, 1);
      targetReadout.textContent = `Aim: ${(filteredX * 100).toFixed(0)}%, ${(filteredY * 100).toFixed(0)}%`;
      sendTarget(filteredX, filteredY, true);
    }));
    document.querySelector('#centerAim').addEventListener('click', () => {
      if (running) { status('Stop tracking before using manual aim.'); return; }
      filteredX = filteredY = 0.5;
      targetReadout.textContent = 'Aim: center';
      sendTarget(0.5, 0.5, true);
    });

    recordBtn.addEventListener('click', async () => {
      recordBtn.disabled = true;
      try {
        if (!window.MediaRecorder || !stream) throw new Error('This browser does not support recording the live camera stream.');
        if (!navigator.storage?.getDirectory) {
          throw new Error('Long match recording needs Safari local storage. Exit Private Browsing and reload this page.');
        }
        if (!recordingMicStream) {
          try {
            recordingMicStream = await navigator.mediaDevices.getUserMedia({ audio: true, video: false });
          } catch (_) {
            // Camera video recording remains available when microphone permission is denied.
          }
        }
        const settings = videoTrack?.getSettings ? videoTrack.getSettings() : {};
        const videoBitsPerSecond = settings.width >= 3840 ? 50000000 : settings.width >= 1920 ? 24000000 : 12000000;
        let recordingVideoTrack = videoTrack;
        const wantsAutoFraming = autoZoom.checked && Number(maxZoom.value) > 1;
        const canCropRecording = typeof HTMLCanvasElement.prototype.captureStream === 'function';
        if (wantsAutoFraming && canCropRecording) {
          recordingCanvas = document.createElement('canvas');
          recordingCanvas.width = settings.width || video.videoWidth;
          recordingCanvas.height = settings.height || video.videoHeight;
          recordingCanvasContext = recordingCanvas.getContext('2d', { alpha: false, desynchronized: true });
          if (!recordingCanvasContext || !recordingCanvas.width || !recordingCanvas.height) {
            throw new Error('Could not prepare a full-resolution recording surface.');
          }
          recordingCaptureStream = recordingCanvas.captureStream(30);
          recordingVideoTrack = recordingCaptureStream.getVideoTracks()[0];
          recordingZoomMode = 'crop';
        } else if (wantsAutoFraming && !hardwareZoomActive) {
          throw new Error('This browser cannot record the auto-framed view. Turn off Auto-frame or use a browser with canvas recording.');
        } else {
          recordingZoomMode = 'camera';
        }
        const audioTracks = recordingMicStream?.getAudioTracks() || [];
        const hasAudio = audioTracks.length > 0;
        recordingOutputStream = new MediaStream([recordingVideoTrack, ...audioTracks]);
        lastRecordingFrameAt = 0;
        drawRecordingFrame(performance.now());
        const types = hasAudio
          ? ['video/mp4;codecs=avc1.42E01E,mp4a.40.2', 'video/mp4;codecs=avc1', 'video/mp4', 'video/webm;codecs=vp9,opus', 'video/webm']
          : ['video/mp4;codecs=avc1', 'video/mp4', 'video/webm;codecs=vp9', 'video/webm'];
        const mimeType = types.find(type => MediaRecorder.isTypeSupported?.(type));
        const options = { videoBitsPerSecond };
        if (hasAudio) options.audioBitsPerSecond = 192000;
        if (mimeType) options.mimeType = mimeType;
        const extension = mimeType?.includes('webm') ? 'webm' : 'mp4';
        const name = `soccer-turret-${new Date().toISOString().replace(/[:.]/g, '-')}.${extension}`;
        await openRecordingStorage(name, videoBitsPerSecond, Number(recordMinutes.value));
        if (navigator.wakeLock?.request) {
          try { recordingWakeLock = await navigator.wakeLock.request('screen'); } catch (_) {}
        }
        recorder = new MediaRecorder(recordingOutputStream, options);
        recordingChunks = [];
        recordingWriteQueue = Promise.resolve();
        recordingWriteError = null;
        recorder.ondataavailable = event => {
          if (!event.data?.size || recordingWriteError || !recordingWriter) return;
          recordingWriteQueue = recordingWriteQueue
            .then(() => recordingWriter.request('chunk', { blob: event.data }))
            .then(result => {
              const elapsedMinutes = Math.floor((performance.now() - recordingStartedAt) / 60000);
              recordStorageReadout.textContent = `Recording · ${elapsedMinutes} min elapsed · ${formatBytes(result.total)} written to Safari storage.`;
            })
            .catch(error => {
              recordingWriteError = error;
              status(`Recording storage failed: ${error?.message || error}. Stopping the clip.`);
              if (recorder?.state === 'recording') recorder.stop();
            });
        };
        recorder.start(1000);
        recordingStartedAt = performance.now();
        const durationMinutes = Number(recordMinutes.value);
        recordingTimer = setTimeout(() => {
          if (recorder?.state === 'recording') {
            status(`Recording limit reached at ${durationMinutes} minutes. Finalizing the clip…`);
            stopAndSaveRecording();
          }
        }, durationMinutes * 60 * 1000);
        autoZoom.disabled = recordingZoomMode === 'camera';
        maxZoom.disabled = recordingZoomMode === 'camera';
        recordMinutes.disabled = true;
        recordBtn.disabled = true; stopRecordBtn.disabled = false;
        const actualBitrate = recorder.videoBitsPerSecond || videoBitsPerSecond;
        status((recordingZoomMode === 'crop' ? 'Recording the auto-framed camera view at ' : 'Recording the iPhone camera stream at ') +
          (settings.width || video.videoWidth) + '×' + (settings.height || video.videoHeight) +
          ` · ${Math.round(actualBitrate / 1000000)} Mbps video · ${durationMinutes} min limit · writing to Safari storage` +
          (hasAudio ? ', audio on.' : ', video only.') +
          (recordingWakeLock ? ' Screen kept awake.' : ' Keep Safari open and the iPhone awake.') +
          (recordingStoragePersisted ? ' Persistent storage granted.' : ' Keep enough phone storage free; Safari may evict site data.'));
      } catch (error) {
        clearTimeout(recordingTimer);
        recordingTimer = 0;
        if (recordingWriter) {
          try { await recordingWriter.request('close'); } catch (_) {}
          recordingWriter.dispose();
          recordingWriter = null;
        }
        if (recordingFolder && recordingFileName) await recordingFolder.removeEntry(recordingFileName).catch(() => {});
        recordingFileHandle = null; recordingFolder = null; recordingFileName = '';
        if (recordingWakeLock) {
          const wakeLock = recordingWakeLock;
          recordingWakeLock = null;
          try { await wakeLock.release(); } catch (_) {}
        }
        recordingCaptureStream?.getTracks().forEach(track => track.stop());
        recordingMicStream?.getTracks().forEach(track => track.stop());
        recordingCaptureStream = null; recordingMicStream = null; recordingOutputStream = null;
        recordingCanvasContext = null; recordingCanvas = null; recordingZoomMode = 'camera';
        autoZoom.disabled = false; maxZoom.disabled = false; recordMinutes.disabled = false;
        if (recorder?.state === 'recording') recorder.stop();
        recorder = null;
        recordBtn.disabled = !running;
        status(`Could not start recording: ${error?.message || error}`);
      }
    });
    recordMinutes.addEventListener('input', updateRecordingPlan);
    stopRecordBtn.addEventListener('click', stopAndSaveRecording);
    document.addEventListener('visibilitychange', () => {
      if (document.hidden && recorder?.state === 'recording') {
        status('Safari is leaving the foreground. Stopping and finalizing the recording…');
        stopAndSaveRecording();
      }
    });
    window.addEventListener('pagehide', () => {
      if (recorder?.state === 'recording') stopAndSaveRecording();
      running = false;
      cancelTrackerFrame();
      stream?.getTracks().forEach(track => track.stop());
      navigator.sendBeacon('/stop');
    });

    updateReadouts(); setMotorUi();
    updateRecordingPlan();
    refreshRecordingStorageInfo();
    refreshRecordingLibrary();
    sendSettings().catch(error => status(`Could not sync settings with the ESP32: ${error?.message || error}`));
  </script>
</body>
</html>
)TRACKER_PAGE";
