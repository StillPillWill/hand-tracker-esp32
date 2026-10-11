#pragma once

const char TRACKER_HTML[] PROGMEM = R"TRACKER_PAGE(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
  <meta name="color-scheme" content="dark">
  <title>ESP32 Hand Tracker</title>
  <style>
    :root{font-family:system-ui,sans-serif;color:#eef2ff;background:#101522}*{box-sizing:border-box}body{margin:0;padding:18px;max-width:760px;margin-inline:auto}
    h1{font-size:1.45rem;margin:0 0 8px}.sub{color:#aab6d1;margin:0 0 16px}.card{background:#1b2435;border:1px solid #34415c;border-radius:16px;padding:14px;margin:12px 0}
    button{border:0;border-radius:10px;padding:12px 16px;font-size:1rem;font-weight:650;background:#7c9cff;color:#0b1220}button.stop{background:#ff6d7e}button.enabled{background:#75d99a}button:disabled{opacity:.5}
    #status{min-height:1.4em;color:#c7d2fe}#stage{position:relative;overflow:hidden;background:#080b11;border-radius:12px;aspect-ratio:4/3}video,canvas{position:absolute;width:100%;height:100%;object-fit:cover;transform:scaleX(-1)}
    #video{display:block}#canvas{inset:0}.controls{display:flex;gap:10px;flex-wrap:wrap}.small{font-size:.9rem;color:#aab6d1}.pill{display:inline-block;padding:4px 9px;border:1px solid #52617f;border-radius:999px;margin-right:6px}
    h2{font-size:1.05rem;margin:0 0 12px}.setting{display:block;margin:14px 0;color:#eef2ff}.setting input{display:block;width:100%;margin-top:8px;accent-color:#7c9cff}.readout{color:#aab6d1;font-variant-numeric:tabular-nums}
  </style>
</head>
<body>
  <h1>ESP32 Hand Tracker</h1>
  <p class="sub">On-device hand tracking · STEP/DIR pan control</p>
  <div class="card">
    <h2>Stepper control</h2>
    <div class="controls"><button id="motorToggle">Enable stepper</button></div>
    <p id="motorState" class="small">Stepper disabled. Enable it when the mechanism is clear.</p>
    <label class="setting">Speed limit: <span id="speedReadout" class="readout">45°/s</span>
      <input id="speedControl" type="range" min="5" max="100" step="1" value="45">
    </label>
    <label class="setting">Hand smoothing: <span id="smoothingReadout" class="readout">72%</span>
      <input id="smoothingControl" type="range" min="0" max="90" step="1" value="72">
    </label>
    <p class="small">PID and acceleration limiting smooth the pan. Position is estimated from step pulses; without an encoder, missed steps cannot be detected.</p>
  </div>
  <div class="card">
    <div class="controls"><button id="start">Start camera</button><button id="stop" class="stop" disabled>Stop tracking</button></div>
    <p id="status">Ready. Allow camera permission to begin.</p>
    <span class="pill" id="handState">No hand</span><span class="pill" id="position">Center: —</span>
  </div>
  <div class="card"><div id="stage"><video id="video" playsinline muted></video><canvas id="canvas"></canvas></div>
    <p class="small">Using the back camera. Move your hand left or right; the pan angle follows its horizontal center.</p></div>
  <script type="module">
    const MODEL = "https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/1/hand_landmarker.task";
    const video = document.querySelector('#video'), canvas = document.querySelector('#canvas'), ctx = canvas.getContext('2d');
    const statusEl = document.querySelector('#status'), handState = document.querySelector('#handState'), position = document.querySelector('#position');
    const startBtn = document.querySelector('#start'), stopBtn = document.querySelector('#stop');
    const motorBtn = document.querySelector('#motorToggle'), motorState = document.querySelector('#motorState');
    const speedControl = document.querySelector('#speedControl'), speedReadout = document.querySelector('#speedReadout');
    const smoothingControl = document.querySelector('#smoothingControl'), smoothingReadout = document.querySelector('#smoothingReadout');
    let landmarker, stream, running = false, lastInfer = 0, lastSend = 0, filteredX = 0.5, lastTargetX = -1;
    let motorEnabled = false, settingsTimer;
    const status = (text) => statusEl.textContent = text;

    function updateMotorUi() {
      motorBtn.textContent = motorEnabled ? 'Disable stepper' : 'Enable stepper';
      motorBtn.classList.toggle('enabled', motorEnabled);
      motorState.textContent = motorEnabled ? 'Stepper enabled. Keep the pan mechanism clear.' : 'Stepper disabled. Enable it when the mechanism is clear.';
    }

    async function sendSettings() {
      const response = await fetch(`/settings?enabled=${motorEnabled ? 1 : 0}&speed=${speedControl.value}`, { method: 'POST', cache: 'no-store' });
      if (!response.ok) throw new Error(`ESP32 rejected motor settings (${response.status})`);
    }

    async function changeMotorEnabled(enabled) {
      const previous = motorEnabled;
      motorEnabled = enabled;
      updateMotorUi();
      motorBtn.disabled = true;
      try {
        await sendSettings();
        lastTargetX = -1;
        status(motorEnabled ? 'Stepper enabled. Tracking will steer it when a hand is found.' : 'Stepper disabled.');
      } catch (e) {
        motorEnabled = previous;
        updateMotorUi();
        status(`Could not update stepper settings: ${e?.message || e}`);
      } finally {
        motorBtn.disabled = false;
      }
    }

    updateMotorUi();
    sendSettings().catch(e => status(`Could not sync motor settings with the ESP32: ${e?.message || e}`));
    motorBtn.addEventListener('click', () => changeMotorEnabled(!motorEnabled));
    speedControl.addEventListener('input', () => {
      speedReadout.textContent = `${speedControl.value}°/s`;
      clearTimeout(settingsTimer);
      settingsTimer = setTimeout(() => sendSettings().catch(e => status(`Could not update speed: ${e?.message || e}`)), 150);
    });
    smoothingControl.addEventListener('input', () => {
      smoothingReadout.textContent = `${smoothingControl.value}%`;
    });

    async function setupModel() {
      let vision;
      status('Downloading hand-tracking code…');
      try {
        vision = await import('https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@1.1.0/vision_bundle.mjs');
      } catch (e) {
        throw new Error(`Could not load MediaPipe JavaScript from jsDelivr: ${e?.message || e}`);
      }

      status('Loading the phone’s WebAssembly vision runtime…');
      let files;
      try {
        files = await vision.FilesetResolver.forVisionTasks('https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@1.1.0/wasm');
      } catch (e) {
        throw new Error(`Could not load MediaPipe WebAssembly from jsDelivr: ${e?.message || e}`);
      }

      status('Downloading the hand model from Google (~8 MB)…');
      const config = { baseOptions: { modelAssetPath: MODEL, delegate: 'GPU' }, runningMode: 'VIDEO', numHands: 1, minHandDetectionConfidence: 0.55, minHandPresenceConfidence: 0.5, minTrackingConfidence: 0.5 };
      try {
        return await vision.HandLandmarker.createFromOptions(files, config);
      } catch (gpuError) {
        status('GPU setup failed; retrying the hand model on the phone CPU…');
        config.baseOptions.delegate = 'CPU';
        try {
          return await vision.HandLandmarker.createFromOptions(files, config);
        } catch (cpuError) {
          throw new Error(`Hand model setup failed. GPU: ${gpuError?.message || gpuError}. CPU: ${cpuError?.message || cpuError}`);
        }
      }
    }

    async function sendTarget(x) {
      if (!motorEnabled) return;
      const now = performance.now();
      if (now - lastSend < 120 || Math.abs(x - lastTargetX) < 0.012) return;
      lastSend = now; lastTargetX = x;
      try { await fetch(`/target?x=${x.toFixed(4)}`, { cache: 'no-store' }); }
      catch (_) { status('Lost connection to the ESP32. Reconnect to its Wi-Fi and reload.'); }
    }

    function draw(result) {
      canvas.width = video.videoWidth || 640; canvas.height = video.videoHeight || 480;
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      ctx.drawImage(video, 0, 0, canvas.width, canvas.height);
      const landmarks = result.landmarks?.[0];
      if (!landmarks) { handState.textContent = 'No hand'; return; }
      handState.textContent = 'Hand found';
      const xs = landmarks.map(p => p.x), ys = landmarks.map(p => p.y);
      const cx = (Math.min(...xs) + Math.max(...xs)) / 2;
      const smoothing = Number(smoothingControl.value) / 100;
      filteredX = filteredX * smoothing + cx * (1 - smoothing);
      position.textContent = `Center: ${(filteredX * 100).toFixed(0)}%`;
      // Mirror is visual only; invert x so a hand moving toward screen-left pans left.
      const px = filteredX * canvas.width, py = ((Math.min(...ys) + Math.max(...ys)) / 2) * canvas.height;
      ctx.beginPath(); ctx.arc(px, py, Math.max(12, canvas.width * 0.018), 0, Math.PI * 2); ctx.fillStyle = '#7c9cff'; ctx.fill();
      sendTarget(1 - filteredX);
    }

    function frame(now) {
      if (!running) return;
      if (video.readyState >= 2 && now - lastInfer > 45) {
        lastInfer = now;
        const result = landmarker.detectForVideo(video, now);
        draw(result);
      }
      requestAnimationFrame(frame);
    }

    startBtn.addEventListener('click', async () => {
      startBtn.disabled = true; status('Starting camera…');
      try {
        if (!window.isSecureContext || !navigator.mediaDevices?.getUserMedia) throw new Error('Camera access requires HTTPS. Open https://192.168.4.1:8000 and accept the certificate warning first.');
        if (!stream) {
          stream = await navigator.mediaDevices.getUserMedia({ video: { facingMode: 'environment', width: { ideal: 640 }, height: { ideal: 480 } }, audio: false });
          video.srcObject = stream; await video.play();
        }
        stopBtn.disabled = false;
        landmarker = await setupModel();
        running = true;
        status('Tracking. The camera frames stay on this phone.'); requestAnimationFrame(frame);
      } catch (e) {
        const detail = e?.message || String(e);
        status(stream ? `Camera is on, but hand tracking failed: ${detail}` : `Could not start camera: ${detail}`);
        startBtn.textContent = stream ? 'Retry' : 'Start camera'; startBtn.disabled = false;
      }
    });
    stopBtn.addEventListener('click', async () => {
      running = false; stream?.getTracks().forEach(t => t.stop()); stream = null; video.srcObject = null;
      motorEnabled = false; updateMotorUi(); lastTargetX = -1;
      try { await fetch('/stop', { method: 'POST' }); } catch (_) {}
      handState.textContent = 'Stopped'; status('Stopped.'); stopBtn.disabled = true; startBtn.disabled = false; startBtn.textContent = 'Start camera';
    });
  </script>
</body>
</html>
)TRACKER_PAGE";
