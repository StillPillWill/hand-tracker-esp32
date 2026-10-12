import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import test from 'node:test';
import vm from 'node:vm';

const header = await readFile(new URL('../src/tracker_page.h', import.meta.url), 'utf8');
const page = header.match(/R"TRACKER_PAGE\(([\s\S]*?)\)TRACKER_PAGE"/)?.[1];
assert.ok(page, 'embedded tracker page must be present');
const script = [...page.matchAll(/<script(?:\s[^>]*)?>([\s\S]*?)<\/script>/gi)]
  .map(match => match[1]).join('\n');

function extractFunction(name) {
  const start = script.indexOf(`    function ${name}(`);
  assert.notEqual(start, -1, `embedded page must define ${name}`);
  const end = script.indexOf('\n    }', start);
  assert.notEqual(end, -1, `${name} must have a complete function body`);
  return script.slice(start, end + '\n    }'.length);
}

function createSimulator() {
  const names = [
    'chooseBall', 'updatePlayerMotion', 'predictBall', 'recentBallHint',
    'playerActivitySignals', 'playerFocusWeight', 'activePlayerGroup', 'centerPlay',
    'isPlayerEngagedInPlay', 'playersInActivePlay', 'compositionZoomLimit',
    'calculateFocusOfPlay', 'unionBounds'
  ];
  const context = {
    clamp: (value, min, max) => Math.max(min, Math.min(max, value)),
    maxZoom: { value: '4' },
    previousPlayers: [], lastPlayersAt: 0,
    previousBall: null, lastBallAt: 0, ballVelocity: { x: 0, y: 0 },
    previousPlayFocus: null, previousPlayFocusAt: 0
  };
  const logic = vm.runInNewContext(
    `${names.map(extractFunction).join('\n')}\n({${names.join(',')}})`, context);

  function detection(cx, cy, width, height, name) {
    return { name, score: 0.9, cx, cy, w: width, h: height,
      x: cx - width / 2, y: cy - height / 2 };
  }

  function frame(now, positions, ballPosition = null) {
    const players = positions.map(([cx, cy]) => detection(cx, cy, 0.04, 0.14, 'player'));
    logic.updatePlayerMotion(players, now);
    const ball = logic.chooseBall(ballPosition
      ? [detection(ballPosition[0], ballPosition[1], 0.02, 0.02, 'sports ball')]
      : [], now);
    const predictedBall = ball || logic.predictBall(now);
    const ballHint = predictedBall || logic.recentBallHint(now);
    const focus = logic.calculateFocusOfPlay(ball, predictedBall, players, ballHint, now);
    return { players, ball, predictedBall, ballHint, focus };
  }

  function zoomFor(subjects) {
    const bounds = logic.unionBounds(subjects);
    const fit = Math.min(0.70 / Math.max(bounds.h, 0.08), 0.82 / Math.max(bounds.w, 0.08));
    return Math.min(4, Math.max(1, fit));
  }

  return { logic, frame, detection, zoomFor };
}

test('synthetic soccer scene weights the chase and zooms its crop to the active play', () => {
  const { logic, frame, detection, zoomFor } = createSimulator();
  const initial = frame(1000, [
    [0.70, 0.52], [0.76, 0.48], [0.31, 0.55], [0.10, 0.50], [0.90, 0.50]
  ], [0.45, 0.50]);
  assert.ok(initial.focus, 'initial scene should produce a play target');

  const chase = frame(1200, [
    [0.62, 0.52], [0.68, 0.48], [0.31, 0.55], [0.10, 0.50], [0.90, 0.50]
  ], [0.49, 0.50]);
  const [closingPlayer, teammate, stationaryPlayer] = chase.players;
  const closing = logic.playerActivitySignals(closingPlayer, chase.ball);
  const stationary = logic.playerActivitySignals(stationaryPlayer, chase.ball);

  assert.equal(closing.state, 'closing');
  assert.ok(closing.approach > 0.75,
    'motion smoothing should retain a strong approach score without requiring full measured speed');
  assert.ok(logic.playerFocusWeight(closingPlayer, chase.ball)
    > logic.playerFocusWeight(stationaryPlayer, chase.ball) * 2,
  'the approaching player should receive materially more focus weight');
  assert.match(chase.focus.label, /2 engaged players · 2 closing/);
  assert.ok(chase.focus.target.x > 0.54 && chase.focus.target.x < 0.59,
    'the aim should shift toward the two players closing on the ball');
  const unweightedSceneX = [chase.ball, ...chase.players]
    .reduce((sum, detection) => sum + detection.cx, 0) / (chase.players.length + 1);
  assert.ok(chase.focus.target.x > unweightedSceneX + 0.02,
    'weighted focus should move toward play compared with the whole-scene centroid');
  assert.equal(chase.focus.framingSubjects.length, 3,
    'the frame should contain the ball and the engaged pair, not sideline detections');

  const fullSceneSubjects = [chase.ball, ...chase.players];
  const actionZoom = zoomFor(chase.focus.framingSubjects);
  const fullSceneZoom = zoomFor(fullSceneSubjects);
  assert.ok(actionZoom > fullSceneZoom + 1.5,
    'framing the engaged group should allow a substantially tighter action crop');
  assert.ok(teammate.motion > 0.75);
});

test('synthetic short ball occlusion follows the predicted ball and active players', () => {
  const { frame } = createSimulator();
  frame(1000, [[0.70, 0.52], [0.76, 0.48], [0.31, 0.55]], [0.45, 0.50]);
  frame(1200, [[0.62, 0.52], [0.68, 0.48], [0.31, 0.55]], [0.49, 0.50]);

  const occluded = frame(1400, [[0.57, 0.52], [0.63, 0.48], [0.31, 0.55]]);
  assert.ok(occluded.predictedBall);
  assert.ok(occluded.predictedBall.cx > 0.49, 'ball prediction should continue its measured path');
  assert.match(occluded.focus.label, /predicted ball/);
  assert.equal(occluded.focus.framingSubjects.length, 3);
  assert.ok(occluded.focus.target.x > 0.52,
    'the predicted ball and nearby active players should keep focus on the chase');
});

test('synthetic longer occlusion falls back to the group around the fading ball position', () => {
  const { frame } = createSimulator();
  frame(1000, [[0.70, 0.52], [0.76, 0.48], [0.31, 0.55]], [0.45, 0.50]);
  frame(1200, [[0.62, 0.52], [0.68, 0.48], [0.31, 0.55]], [0.49, 0.50]);
  const occluded = frame(2300, [[0.56, 0.52], [0.62, 0.48], [0.31, 0.55]]);

  assert.equal(occluded.predictedBall, null, 'short-term ball prediction should expire');
  assert.ok(occluded.ballHint.focusConfidence > 0.5, 'the recent-ball hint should still be usable');
  assert.match(occluded.focus.label, /active group near recent ball position/);
  assert.equal(occluded.focus.framingSubjects.length, 2);
  assert.ok(occluded.focus.target.x > 0.5 && occluded.focus.target.x < 0.68,
    'fallback should select the active pair near the previous play, not the stationary player');
});
