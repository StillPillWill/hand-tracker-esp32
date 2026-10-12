import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';
import vm from 'node:vm';

const header = await readFile(new URL('../src/tracker_page.h', import.meta.url), 'utf8');
const page = header.match(/R"TRACKER_PAGE\(([\s\S]*?)\)TRACKER_PAGE"/)?.[1];
assert.ok(page, 'embedded tracker page must be present');
const script = [...page.matchAll(/<script(?:\s[^>]*)?>([\s\S]*?)<\/script>/gi)]
  .map(match => match[1]).join('\n');
new vm.Script(script, { filename: 'tracker_page.h inline script' });

test('recording elapsed-time state is declared for the strict module script', () => {
  assert.match(script, /\blet\b[^;\n]*\brecordingStartedAt\b/);
});

function extractFunction(name) {
  const starts = [
    script.indexOf(`    function ${name}(`),
    script.indexOf(`    async function ${name}(`)
  ].filter(index => index >= 0);
  const start = starts.length ? Math.min(...starts) : -1;
  assert.notEqual(start, -1, `embedded page must define ${name}`);
  const end = script.indexOf('\n    }', start);
  assert.notEqual(end, -1, `${name} must have a complete function body`);
  return script.slice(start, end + '\n    }'.length);
}

function loadLogic() {
  const names = [
    'getSoccerProviders', 'getBundledSoccerModel', 'getWasmModelCandidates', 'playerActivitySignals',
    'playerEngagement', 'playerFocusWeight', 'activePlayerGroup', 'centerPlay',
    'isPlayerEngagedInPlay', 'playersInActivePlay', 'compositionZoomLimit', 'calculateFocusOfPlay'
  ];
  const functions = names.map(extractFunction).join('\n');
  const context = {
    SOCCER_MODEL_URL: 'phone-float.onnx',
    SOCCER_WASM_MODEL_URL: 'phone-int8.onnx',
    SOCCER_MODEL_NAME: 'phone-float.onnx',
    SOCCER_WASM_MODEL_NAME: 'phone-int8.onnx',
    Uint8Array,
    clamp: (value, min, max) => Math.max(min, Math.min(max, value)),
    maxZoom: { value: '4' },
    previousPlayFocus: null,
    previousPlayFocusAt: 0
  };
  const logic = vm.runInNewContext(`${functions}\n({${names.join(',')}})`, context);
  return { ...logic, context };
}

test('iPhone Safari opportunistically tries WebGPU when Safari exposes it', () => {
  const { getSoccerProviders, getBundledSoccerModel } = loadLogic();
  const iphone = {
    userAgent: 'Mozilla/5.0 (iPhone; CPU iPhone OS 26_0 like Mac OS X) AppleWebKit/605.1.15 Version/26.0 Mobile Safari/605.1.15',
    platform: 'iPhone', maxTouchPoints: 5, gpu: {}
  };
  assert.deepEqual(Array.from(getSoccerProviders(iphone)), ['webgpu', 'wasm']);
  assert.equal(getBundledSoccerModel(iphone).name, 'phone-float.onnx');
});

test('iPhone Safari requests an NPU when it exposes WebNN', () => {
  const { getSoccerProviders, getBundledSoccerModel } = loadLogic();
  const iphone = {
    userAgent: 'Mozilla/5.0 (iPhone; CPU iPhone OS 26_0 like Mac OS X) AppleWebKit/605.1.15 Version/26.0 Mobile Safari/605.1.15',
    platform: 'iPhone', maxTouchPoints: 5, ml: {}, gpu: {}
  };
  assert.deepEqual(Array.from(getSoccerProviders(iphone)), ['webnn', 'webgpu', 'wasm']);
  assert.equal(getBundledSoccerModel(iphone).name, 'phone-float.onnx');
});

test('iPhone Safari without WebGPU uses the smaller int8 WASM model directly', () => {
  const { getSoccerProviders, getBundledSoccerModel } = loadLogic();
  const iphone = {
    userAgent: 'Mozilla/5.0 (iPhone; CPU iPhone OS 26_0 like Mac OS X) AppleWebKit/605.1.15 Version/26.0 Mobile Safari/605.1.15',
    platform: 'iPhone', maxTouchPoints: 5
  };
  assert.deepEqual(Array.from(getSoccerProviders(iphone)), ['wasm']);
  assert.equal(getBundledSoccerModel(iphone).name, 'phone-int8.onnx');
});

test('iPad Safari is not mistaken for a supported macOS WebGPU browser', () => {
  const { getSoccerProviders } = loadLogic();
  const ipad = {
    userAgent: 'Mozilla/5.0 (Macintosh; Intel Mac OS X 15_0) AppleWebKit/605.1.15 Version/26.0 Safari/605.1.15',
    platform: 'MacIntel', maxTouchPoints: 5
  };
  assert.deepEqual(Array.from(getSoccerProviders(ipad)), ['wasm']);
});

test('supported desktop Chrome keeps WebGPU and the float model', () => {
  const { getSoccerProviders, getBundledSoccerModel } = loadLogic();
  const chrome = {
    userAgent: 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/140.0.0.0 Safari/537.36',
    platform: 'Win32', maxTouchPoints: 0, gpu: {}
  };
  assert.deepEqual(Array.from(getSoccerProviders(chrome)), ['webgpu', 'wasm']);
  assert.equal(getBundledSoccerModel(chrome).name, 'phone-float.onnx');
});

test('failed hardware acceleration loads cached/downloaded int8 before trying float WASM', async () => {
  const { getWasmModelCandidates } = loadLogic();
  const floatBytes = new Uint8Array([1, 2]);
  const int8Bytes = new Uint8Array([3, 4]);
  const result = await getWasmModelCandidates(floatBytes, true,
    async () => ({ bytes: int8Bytes, name: 'phone-int8.onnx' }));
  assert.equal(result.error, null);
  assert.equal(result.candidates[0].bytes, int8Bytes);
  assert.equal(result.candidates[0].name, 'phone-int8.onnx');
  assert.equal(result.candidates[1].bytes, floatBytes);
});

test('successful WebGPU setup does not fetch the WASM fallback model', async () => {
  const { getWasmModelCandidates } = loadLogic();
  let loaded = false;
  const floatBytes = new Uint8Array([1, 2]);
  const result = await getWasmModelCandidates(floatBytes, false, async () => {
    loaded = true;
    return { bytes: new Uint8Array([3, 4]), name: 'phone-int8.onnx' };
  });
  assert.equal(loaded, false);
  assert.equal(result.candidates.length, 1);
  assert.equal(result.candidates[0].bytes, floatBytes);
});

test('failed fallback download still preserves float WASM as a last resort', async () => {
  const { getWasmModelCandidates } = loadLogic();
  const floatBytes = new Uint8Array([1, 2]);
  const result = await getWasmModelCandidates(floatBytes, true, async () => {
    throw new Error('offline');
  });
  assert.equal(result.candidates.length, 1);
  assert.equal(result.candidates[0].bytes, floatBytes);
  assert.match(result.error.message, /offline/);
});

test('focus of play combines a visible ball with nearby players', () => {
  const { calculateFocusOfPlay } = loadLogic();
  const ball = { cx: 0.5, cy: 0.5, x: 0.49, y: 0.49, w: 0.02, h: 0.02, score: 0.9 };
  const players = [
    { cx: 0.53, cy: 0.51, score: 0.9, motion: 0.2, vx: 0.04, vy: 0.02 },
    { cx: 0.46, cy: 0.52, score: 0.8, motion: 0.1, vx: -0.02, vy: 0.02 }
  ];
  const focus = calculateFocusOfPlay(ball, null, players, ball, 5000);
  assert.match(focus.label, /ball \+ 2 engaged players/);
  assert.equal(focus.framingSubjects.length, 3);
  assert.ok(focus.target.x > 0.46 && focus.target.x < 0.53);
  assert.equal(focus.zoomLimit, 4);
});

test('player activity estimates approach direction and discounts a stale ball hint', () => {
  const { playerActivitySignals } = loadLogic();
  const player = { cx: 0.62, cy: 0.5, motion: 0.6, vx: -0.2, vy: 0 };
  const current = playerActivitySignals(player, { cx: 0.5, cy: 0.5 });
  const fading = playerActivitySignals(player, { cx: 0.5, cy: 0.5, focusConfidence: 0.25 });
  assert.equal(current.state, 'closing');
  assert.equal(current.approach, 0.6);
  assert.equal(fading.approach, 0.15);
  assert.ok(current.engagement > fading.engagement);
});

test('weighted play center favors a player closing on the ball', () => {
  const { centerPlay } = loadLogic();
  const ball = { cx: 0.5, cy: 0.5, score: 0.9 };
  const players = [
    { cx: 0.62, cy: 0.5, score: 0.9, motion: 0.6, vx: -0.2, vy: 0 },
    { cx: 0.38, cy: 0.5, score: 0.9, motion: 0, vx: 0, vy: 0 }
  ];
  const focus = centerPlay(ball, players);
  assert.ok(focus.x > ball.cx, 'closing player should pull focus farther than the stationary player');
});

test('focus framing and zoom bounds follow engaged players instead of a distant stationary player', () => {
  const { calculateFocusOfPlay } = loadLogic();
  const ball = { cx: 0.5, cy: 0.5, x: 0.49, y: 0.49, w: 0.02, h: 0.02, score: 0.9 };
  const players = [
    { cx: 0.58, cy: 0.5, score: 0.9, motion: 0.6, vx: -0.2, vy: 0 },
    { cx: 0.72, cy: 0.5, score: 0.9, motion: 0, vx: 0, vy: 0 }
  ];
  const focus = calculateFocusOfPlay(ball, null, players, ball, 5000);
  assert.match(focus.label, /1 engaged player · 1 closing/);
  assert.equal(focus.framingSubjects.length, 2);
  assert.equal(focus.zoomLimit, 1.8);
  assert.ok(focus.target.x > 0.5);
});

test('focus labels a stationary fallback as nearby instead of active', () => {
  const { calculateFocusOfPlay } = loadLogic();
  const ball = { cx: 0.5, cy: 0.5, x: 0.49, y: 0.49, w: 0.02, h: 0.02, score: 0.9 };
  const player = { cx: 0.72, cy: 0.5, score: 0.9, motion: 0, vx: 0, vy: 0 };
  const focus = calculateFocusOfPlay(ball, null, [player], ball, 5000);
  assert.match(focus.label, /1 nearby player/);
  assert.equal(focus.framingSubjects.length, 2);
});

test('occlusion fallback center weights an approaching player more than nearby stationary players', () => {
  const { activePlayerGroup } = loadLogic();
  const players = [
    { cx: 0.25, cy: 0.5, score: 0.9, motion: 0.7, vx: 0.1, vy: 0 },
    { cx: 0.29, cy: 0.5, score: 0.9, motion: 0, vx: 0, vy: 0 },
    { cx: 0.33, cy: 0.5, score: 0.9, motion: 0, vx: 0, vy: 0 }
  ];
  const group = activePlayerGroup(players, { cx: 0.43, cy: 0.5, focusConfidence: 1 }, 5000);
  assert.ok(group);
  assert.ok(group.x < 0.29, 'weighted centroid should shift toward the player moving into play');
});

test('focus of play uses the predicted ball during a short occlusion', () => {
  const { calculateFocusOfPlay } = loadLogic();
  const predicted = { cx: 0.42, cy: 0.52, x: 0.41, y: 0.51, w: 0.02, h: 0.02, score: 0.8 };
  const players = [{ cx: 0.45, cy: 0.53, score: 0.8, motion: 0.1, vx: 0.01, vy: 0 }];
  const focus = calculateFocusOfPlay(null, predicted, players, predicted, 5000);
  assert.match(focus.label, /predicted ball/);
  assert.equal(focus.framingSubjects.length, 2);
});

test('longer ball occlusion favors an active player group near the recent ball hint', () => {
  const { calculateFocusOfPlay } = loadLogic();
  const leftGroup = [
    { cx: 0.25, cy: 0.48, score: 0.9, motion: 0.6, vx: 0.05, vy: 0.01 },
    { cx: 0.29, cy: 0.50, score: 0.9, motion: 0.5, vx: 0.03, vy: 0.01 },
    { cx: 0.33, cy: 0.52, score: 0.8, motion: 0.4, vx: 0.02, vy: 0 }
  ];
  const farGroup = [
    { cx: 0.76, cy: 0.48, score: 0.95, motion: 0.5, vx: -0.04, vy: 0 },
    { cx: 0.80, cy: 0.51, score: 0.95, motion: 0.4, vx: -0.03, vy: 0.01 },
    { cx: 0.84, cy: 0.54, score: 0.9, motion: 0.4, vx: -0.02, vy: 0 }
  ];
  const focus = calculateFocusOfPlay(null, null, [...leftGroup, ...farGroup],
    { cx: 0.29, cy: 0.5, focusConfidence: 0.8 }, 5000);
  assert.match(focus.label, /active group near recent ball position/);
  assert.ok(focus.target.x < 0.4);
  assert.equal(focus.framingSubjects.length, 3);
});

test('no ball or players produces no play target', () => {
  const { calculateFocusOfPlay } = loadLogic();
  assert.equal(calculateFocusOfPlay(null, null, [], null, 5000), null);
});
