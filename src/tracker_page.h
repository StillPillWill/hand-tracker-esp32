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
    #stage{position:relative;overflow:hidden;background:#05080d;border-radius:12px;aspect-ratio:16/9}canvas{display:block;width:100%;height:100%;object-fit:contain}.overlay{position:absolute;inset:0;pointer-events:none}.manual{display:grid;grid-template-columns:repeat(3,1fr);gap:6px;max-width:360px;margin:auto}.manual button{padding:8px}.spacer{visibility:hidden}a.clip{color:#9eb5ff;word-break:break-all}.hint{margin-bottom:0}
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
    <p id="status">Ready. Start the back camera and load the phone-side object detector.</p>
    <span class="pill" id="trackingState">No camera</span><span class="pill" id="targetReadout">Aim: center</span><span class="pill" id="zoomReadout">Crop zoom: 1.0×</span>
    <label class="setting">Track mode
      <select id="trackMode"><option value="play">Ball and nearby players</option><option value="ball">Ball only</option><option value="players">Players</option></select>
    </label>
    <label class="setting">Maximum digital crop zoom: <span id="maxZoomReadout" class="readout">3.0×</span>
      <input id="maxZoom" type="range" min="1" max="4" step="0.1" value="3">
    </label>
    <label class="setting">Tracking smoothing: <span id="smoothingReadout" class="readout">65%</span>
      <input id="smoothing" type="range" min="0" max="90" step="1" value="65">
    </label>
    <label class="check"><input id="autoZoom" type="checkbox" checked><span>Auto-frame the detected play</span></label>
    <div class="row"><button id="record" class="secondary" disabled>Record clip</button><button id="stopRecord" class="secondary" disabled>Stop &amp; save clip</button></div>
    <p class="small">The detector runs on the iPhone. Camera frames stay on the phone; only x/y targets go to the ESP32. Browser zoom is a digital crop, so it trades image detail for a tighter shot.</p>
    <a id="lastClip" class="clip" hidden></a>
  </div>

  <div class="card"><div id="stage"><canvas id="canvas"></canvas></div>
    <p class="small">Use a stable tripod and keep the entire yaw/pitch travel clear. Stop the motors before handling the mount.</p></div>

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
    const recordBtn = document.querySelector('#record');
    const stopRecordBtn = document.querySelector('#stopRecord');
    const lastClip = document.querySelector('#lastClip');

    let detector = null, stream = null, running = false, motorsEnabled = false;
    let lastInfer = 0, lastSend = 0, lastLockAt = 0, settingsTimer = 0;
    let detections = [], filteredX = 0.5, filteredY = 0.5, currentZoom = 1;
    let lastTargetX = -1, lastTargetY = -1, previousBall = null, lastBallAt = 0;
    let recorder = null, recordingChunks = [], recordingVideoTrack = null;
    const clamp = (value, low, high) => Math.max(low, Math.min(high, value));
    const status = text => { statusEl.textContent = text; };

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

    async function sendTarget(x, y, force = false) {
      if (!motorsEnabled) return;
      const now = performance.now();
      if (!force && (now - lastSend < 100 || (Math.abs(x - lastTargetX) < 0.008 && Math.abs(y - lastTargetY) < 0.008))) return;
      lastSend = now; lastTargetX = x; lastTargetY = y;
      try {
        const response = await fetch(`/target?x=${x.toFixed(4)}&y=${y.toFixed(4)}`, { cache: 'no-store' });
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

    async function setupDetector() {
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
        try {
          return await vision.ObjectDetector.createFromOptions(files, options);
        } catch (cpuError) {
          throw new Error(`Object model setup failed. GPU: ${gpuError?.message || gpuError}. CPU: ${cpuError?.message || cpuError}`);
        }
      }
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
      previousBall = { cx: balls[0].cx, cy: balls[0].cy };
      lastBallAt = now;
      return balls[0];
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
      const players = detections.filter(d => d.name === 'person');
      const balls = detections.filter(d => d.name === 'sports ball');
      const ball = chooseBall(balls, now);
      let target = null;
      let framingSubjects = [];

      if (trackMode.value !== 'players' && ball) {
        target = { x: ball.cx, y: ball.cy };
        const nearbyPlayers = players.filter(p => Math.hypot(p.cx - ball.cx, p.cy - ball.cy) < 0.34);
        framingSubjects = [ball, ...nearbyPlayers];
      } else if (trackMode.value !== 'ball' && players.length) {
        let group = players;
        if (previousBall && now - lastBallAt < 1400) {
          const nearby = players.filter(p => Math.hypot(p.cx - previousBall.cx, p.cy - previousBall.cy) < 0.4);
          if (nearby.length) group = nearby;
        }
        const weight = group.reduce((sum, p) => sum + Math.max(0.15, p.score), 0);
        target = {
          x: group.reduce((sum, p) => sum + p.cx * Math.max(0.15, p.score), 0) / weight,
          y: group.reduce((sum, p) => sum + p.cy * Math.max(0.15, p.score), 0) / weight
        };
        framingSubjects = group;
      }

      if (target) {
        lastLockAt = now;
        const amount = 1 - Number(smoothing.value) / 100;
        filteredX += (target.x - filteredX) * amount;
        filteredY += (target.y - filteredY) * amount;
        trackingState.textContent = ball && trackMode.value !== 'players'
          ? `Ball ${Math.round(ball.score * 100)}% · ${players.length} players`
          : players.length ? `${players.length} players · ball not locked` : 'No ball or players';
        targetReadout.textContent = `Aim: ${(filteredX * 100).toFixed(0)}%, ${(filteredY * 100).toFixed(0)}%`;
        sendTarget(filteredX, filteredY);
      } else {
        trackingState.textContent = 'No ball or players';
      }

      const bounds = unionBounds(framingSubjects);
      if (autoZoom.checked && bounds) {
        const fitHeight = 0.70 / Math.max(bounds.h, 0.08);
        const fitWidth = 0.82 / Math.max(bounds.w, 0.08);
        const wanted = clamp(Math.min(fitHeight, fitWidth), 1, Number(maxZoom.value));
        currentZoom += (wanted - currentZoom) * 0.16;
      } else if (!autoZoom.checked) {
        currentZoom = 1;
      } else if (!target && now - lastLockAt > 900) {
        currentZoom += (1 - currentZoom) * 0.08;
      }
      zoomReadout.textContent = `Crop zoom: ${currentZoom.toFixed(1)}×`;
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

      const zoom = clamp(currentZoom, 1, Number(maxZoom.value));
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
        if (d.name !== 'person' && d.name !== 'sports ball') continue;
        const x = (d.x * sourceWidth - cropX) * scaleX;
        const y = (d.y * sourceHeight - cropY) * scaleY;
        const w = d.w * sourceWidth * scaleX;
        const h = d.h * sourceHeight * scaleY;
        if (x + w < 0 || y + h < 0 || x > outWidth || y > outHeight) continue;
        ctx.strokeStyle = d.name === 'sports ball' ? '#ffd166' : '#67e8f9';
        ctx.strokeRect(x, y, w, h);
        ctx.fillStyle = ctx.strokeStyle;
        ctx.fillText(d.name === 'sports ball' ? 'BALL' : 'PLAYER', x + 3, Math.max(18, y - 4));
      }
      if (running) requestAnimationFrame(frame);
    }

    function frame(now) {
      if (!running) return;
      if (video.readyState >= 2) {
        if (detector && now - lastInfer >= 120) {
          lastInfer = now;
          try { updateTracking(detector.detectForVideo(video, now), now); }
          catch (error) { status(`Detection error: ${error?.message || error}`); }
        }
        drawFrame(now);
      } else {
        requestAnimationFrame(frame);
      }
    }

    async function startCameraAndTracking() {
      startBtn.disabled = true;
      try {
        if (!window.isSecureContext || !navigator.mediaDevices?.getUserMedia) {
          throw new Error('Camera access needs HTTPS. Open https://192.168.4.1:8000 and accept the ESP32 certificate first.');
        }
        if (!stream) {
          status('Opening the iPhone back camera and microphone…');
          stream = await navigator.mediaDevices.getUserMedia({
            video: { facingMode: { ideal: 'environment' }, width: { ideal: 1920 }, height: { ideal: 1080 }, frameRate: { ideal: 30, max: 30 } },
            audio: true
          });
          video.srcObject = stream;
          await video.play();
        }
        stopBtn.disabled = false;
        recordBtn.disabled = false;
        if (!running) { running = true; requestAnimationFrame(frame); }
        if (!detector) detector = await setupDetector();
        status('Tracking live. The phone sends only yaw/pitch targets to the ESP32.');
        startBtn.textContent = 'Tracker running';
      } catch (error) {
        status(stream ? `Camera is on, but tracker setup failed: ${error?.message || error}` : `Could not start camera: ${error?.message || error}`);
        startBtn.textContent = detector ? 'Retry' : stream ? 'Retry tracker' : 'Start camera + tracker';
        startBtn.disabled = false;
      }
    }

    function stopAndSaveRecording() {
      if (!recorder || recorder.state === 'inactive') return;
      stopRecordBtn.disabled = true;
      recorder.onstop = () => {
        const type = recorder.mimeType || 'video/mp4';
        const blob = new Blob(recordingChunks, { type });
        recordingChunks = [];
        recordingVideoTrack?.stop(); recordingVideoTrack = null;
        if (lastClip.href.startsWith('blob:')) URL.revokeObjectURL(lastClip.href);
        const extension = type.includes('webm') ? 'webm' : 'mp4';
        const name = `soccer-turret-${new Date().toISOString().replace(/[:.]/g, '-')}.${extension}`;
        lastClip.href = URL.createObjectURL(blob);
        lastClip.download = name;
        lastClip.textContent = `Download recorded clip (${(blob.size / 1048576).toFixed(1)} MB)`;
        lastClip.hidden = false;
        lastClip.click();
        status('Clip recorded. If it did not save automatically, tap the download link below.');
        recorder = null;
        recordBtn.disabled = false;
      };
      recorder.stop();
    }

    async function stopCamera() {
      running = false;
      if (recorder && recorder.state !== 'inactive') stopAndSaveRecording();
      detector?.close(); detector = null;
      stream?.getTracks().forEach(track => track.stop());
      stream = null; video.srcObject = null;
      recordBtn.disabled = true; stopBtn.disabled = true;
      startBtn.disabled = false; startBtn.textContent = 'Start camera + tracker';
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

    recordBtn.addEventListener('click', () => {
      try {
        if (!window.MediaRecorder || !canvas.captureStream) throw new Error('This iPhone browser does not support canvas recording.');
        const clipStream = canvas.captureStream(30);
        recordingVideoTrack = clipStream.getVideoTracks()[0] || null;
        stream?.getAudioTracks().forEach(track => clipStream.addTrack(track));
        const types = ['video/mp4', 'video/mp4;codecs=avc1.42E01E,mp4a.40.2', 'video/webm;codecs=vp8,opus', 'video/webm'];
        const mimeType = types.find(type => MediaRecorder.isTypeSupported(type));
        recorder = mimeType ? new MediaRecorder(clipStream, { mimeType }) : new MediaRecorder(clipStream);
        recordingChunks = [];
        recorder.ondataavailable = event => { if (event.data?.size) recordingChunks.push(event.data); };
        recorder.start(1000);
        recordBtn.disabled = true; stopRecordBtn.disabled = false;
        status('Recording the framed view with microphone audio.');
      } catch (error) {
        status(`Could not start recording: ${error?.message || error}`);
      }
    });
    stopRecordBtn.addEventListener('click', stopAndSaveRecording);
    window.addEventListener('pagehide', () => { running = false; stream?.getTracks().forEach(track => track.stop()); navigator.sendBeacon('/stop'); });

    updateReadouts(); setMotorUi();
    sendSettings().catch(error => status(`Could not sync settings with the ESP32: ${error?.message || error}`));
  </script>
</body>
</html>
)TRACKER_PAGE";
