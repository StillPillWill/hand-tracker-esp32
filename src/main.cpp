#include <Arduino.h>
#include <WiFi.h>
#include <AccelStepper.h>
#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
#include <ResourceNode.hpp>
#include "tracker_page.h"

using namespace httpsserver;

// Local, open access point. The phone performs all vision inference.
static const char *AP_SSID = "SoccerTurret-ESP32";

// One STEP/DIR driver per axis. Both active-low ENABLE inputs may share GPIO 25.
static constexpr int YAW_STEP_PIN = 26;
static constexpr int YAW_DIR_PIN = 27;
static constexpr int PITCH_STEP_PIN = 33;
static constexpr int PITCH_DIR_PIN = 32;
static constexpr int ENABLE_PIN = 25;
static constexpr bool USE_ENABLE_PIN = true;
static constexpr bool INVERT_YAW_DIRECTION = false;
static constexpr bool INVERT_PITCH_DIRECTION = false;

// Set these for the actual motor, microstep jumpers, and gear ratio.
static constexpr float STEPS_PER_REV = 200.0f;
static constexpr float MICROSTEPS = 16.0f;
static constexpr float YAW_GEAR_RATIO = 1.0f;
static constexpr float PITCH_GEAR_RATIO = 1.0f;
static constexpr float YAW_LIMIT_DEG = 70.0f;
static constexpr float PITCH_UP_LIMIT_DEG = 35.0f;
static constexpr float PITCH_DOWN_LIMIT_DEG = 25.0f;
static constexpr float MAX_SPEED_DEG_S = 60.0f;
static constexpr float ACCEL_DEG_S2 = 90.0f;
static constexpr float PID_KP = 6.0f;
static constexpr float PID_KI = 0.03f;
static constexpr float PID_KD = 0.30f;
static constexpr float PID_DEADBAND_STEPS = 2.0f;
static constexpr uint32_t PID_INTERVAL_US = 20000; // 50 Hz per axis

static constexpr float YAW_STEPS_PER_DEG = (STEPS_PER_REV * MICROSTEPS * YAW_GEAR_RATIO) / 360.0f;
static constexpr float PITCH_STEPS_PER_DEG = (STEPS_PER_REV * MICROSTEPS * PITCH_GEAR_RATIO) / 360.0f;
static constexpr long YAW_LIMIT_STEPS = (long)(YAW_LIMIT_DEG * YAW_STEPS_PER_DEG);
static constexpr long PITCH_UP_LIMIT_STEPS = (long)(PITCH_UP_LIMIT_DEG * PITCH_STEPS_PER_DEG);
static constexpr long PITCH_DOWN_LIMIT_STEPS = (long)(PITCH_DOWN_LIMIT_DEG * PITCH_STEPS_PER_DEG);
static constexpr float MIN_SPEED_DEG_S = 5.0f;
static constexpr float MAX_YAW_SPEED_STEPS_S = MAX_SPEED_DEG_S * YAW_STEPS_PER_DEG;
static constexpr float MAX_PITCH_SPEED_STEPS_S = MAX_SPEED_DEG_S * PITCH_STEPS_PER_DEG;

struct AxisControl {
  AccelStepper *motor;
  float stepsPerDegree;
  long minSteps;
  long maxSteps;
  float accelerationStepsPerSecond2;
  float speedLimitStepsPerSecond;
  long targetSteps;
  float speedCommandStepsPerSecond;
  float integral;
  long previousPosition;
  uint32_t lastPidUpdateUs;
};

AccelStepper yawStepper(AccelStepper::DRIVER, YAW_STEP_PIN, YAW_DIR_PIN);
AccelStepper pitchStepper(AccelStepper::DRIVER, PITCH_STEP_PIN, PITCH_DIR_PIN);
AxisControl yawAxis{&yawStepper, YAW_STEPS_PER_DEG, -YAW_LIMIT_STEPS, YAW_LIMIT_STEPS,
                    ACCEL_DEG_S2 * YAW_STEPS_PER_DEG, 45.0f * YAW_STEPS_PER_DEG,
                    0, 0.0f, 0.0f, 0, 0};
AxisControl pitchAxis{&pitchStepper, PITCH_STEPS_PER_DEG, -PITCH_DOWN_LIMIT_STEPS, PITCH_UP_LIMIT_STEPS,
                      ACCEL_DEG_S2 * PITCH_STEPS_PER_DEG, 30.0f * PITCH_STEPS_PER_DEG,
                      0, 0.0f, 0.0f, 0, 0};

SSLCert *tlsCert = nullptr;
HTTPSServer *httpsServer = nullptr;
bool motorsEnabled = false;

static float clampFloat(float value, float low, float high) {
  return fmaxf(low, fminf(high, value));
}

static void resetAxis(AxisControl &axis, bool holdCurrentPosition) {
  const long position = axis.motor->currentPosition();
  if (holdCurrentPosition) axis.targetSteps = position;
  axis.speedCommandStepsPerSecond = 0.0f;
  axis.integral = 0.0f;
  axis.previousPosition = position;
  axis.lastPidUpdateUs = micros();
  axis.motor->setSpeed(0.0f);
}

static void disableMotors() {
  motorsEnabled = false;
  resetAxis(yawAxis, true);
  resetAxis(pitchAxis, true);
  if (USE_ENABLE_PIN) digitalWrite(ENABLE_PIN, HIGH);
}

static void stopAndHold() {
  resetAxis(yawAxis, true);
  resetAxis(pitchAxis, true);
  if (motorsEnabled && USE_ENABLE_PIN) digitalWrite(ENABLE_PIN, LOW);
}

static void setTargetFromImage(float x, float y, float zoom) {
  // Convert image-space displacement into a small, zoom-aware camera angle.
  // Add that correction to the live motor position so repeated detections
  // close the visual servo loop instead of mapping pixels to absolute angles.
  constexpr float HALF_HFOV_RAD = 30.0f * PI / 180.0f;
  constexpr float HALF_VFOV_RAD = 22.5f * PI / 180.0f;
  const float yawCorrection = atanf((2.0f * x - 1.0f) * tanf(HALF_HFOV_RAD) / zoom);
  const float pitchCorrection = atanf((1.0f - 2.0f * y) * tanf(HALF_VFOV_RAD) / zoom);
  const long yawTarget = yawStepper.currentPosition() +
      lroundf(yawCorrection * 180.0f / PI * yawAxis.stepsPerDegree);
  const long pitchTarget = pitchStepper.currentPosition() +
      lroundf(pitchCorrection * 180.0f / PI * pitchAxis.stepsPerDegree);
  yawAxis.targetSteps = constrain(yawTarget, yawAxis.minSteps, yawAxis.maxSteps);
  pitchAxis.targetSteps = constrain(pitchTarget, pitchAxis.minSteps, pitchAxis.maxSteps);
}

static void setTargetAngles(float yawDegrees, float pitchDegrees) {
  const long yawTarget = lroundf(yawDegrees * yawAxis.stepsPerDegree);
  const long pitchTarget = lroundf(pitchDegrees * pitchAxis.stepsPerDegree);
  yawAxis.targetSteps = constrain(yawTarget, yawAxis.minSteps, yawAxis.maxSteps);
  pitchAxis.targetSteps = constrain(pitchTarget, pitchAxis.minSteps, pitchAxis.maxSteps);
}

void handleRoot(HTTPRequest *req, HTTPResponse *res) {
  (void)req;
  res->setHeader("Content-Type", "text/html; charset=utf-8");
  res->setHeader("Cache-Control", "no-store");
  res->setHeader("Cross-Origin-Opener-Policy", "same-origin");
  res->setHeader("Cross-Origin-Embedder-Policy", "require-corp");
  res->print(TRACKER_HTML);
}

void handleStatus(HTTPRequest *req, HTTPResponse *res) {
  req->discardRequestBody();
  char statusJson[128];
  snprintf(statusJson, sizeof(statusJson),
      "{\"ok\":true,\"enabled\":%s,\"yawSpeed\":%.1f,\"pitchSpeed\":%.1f}",
      motorsEnabled ? "true" : "false",
      yawAxis.speedLimitStepsPerSecond / yawAxis.stepsPerDegree,
      pitchAxis.speedLimitStepsPerSecond / pitchAxis.stepsPerDegree);
  res->setHeader("Content-Type", "application/json");
  res->setHeader("Cache-Control", "no-store");
  res->print(statusJson);
}

static bool readNormalizedParameter(ResourceParameters *params, const char *name, float &value) {
  std::string raw;
  if (!params->getQueryParameter(name, raw)) return false;
  char *end = nullptr;
  value = strtof(raw.c_str(), &end);
  return end != raw.c_str() && *end == '\0' && isfinite(value) && value >= 0.0f && value <= 1.0f;
}

void handleTarget(HTTPRequest *req, HTTPResponse *res) {
  req->discardRequestBody();
  ResourceParameters *params = req->getParams();
  float x = 0.5f, y = 0.5f;
  if (!readNormalizedParameter(params, "x", x) || !readNormalizedParameter(params, "y", y)) {
    res->setStatusCode(400);
    res->setStatusText("Invalid target");
    res->setHeader("Content-Type", "application/json");
    res->print("{\"ok\":false,\"error\":\"Expected normalized x and y from 0 to 1\"}");
    return;
  }
  if (!motorsEnabled) {
    res->setHeader("Content-Type", "application/json");
    res->print("{\"ok\":false,\"enabled\":false}");
    return;
  }

  const float yawDegrees = (x - 0.5f) * 2.0f * YAW_LIMIT_DEG;
  const float pitchDegrees = y <= 0.5f
      ? (0.5f - y) * 2.0f * PITCH_UP_LIMIT_DEG
      : -(y - 0.5f) * 2.0f * PITCH_DOWN_LIMIT_DEG;
  setTargetAngles(yawDegrees, pitchDegrees);

  res->setHeader("Content-Type", "application/json");
  res->print("{\"ok\":true}");
}

void handleAim(HTTPRequest *req, HTTPResponse *res) {
  req->discardRequestBody();
  ResourceParameters *params = req->getParams();
  float x = 0.5f, y = 0.5f, zoom = 1.0f;
  if (!readNormalizedParameter(params, "x", x) || !readNormalizedParameter(params, "y", y)) {
    res->setStatusCode(400);
    res->setStatusText("Invalid target");
    res->setHeader("Content-Type", "application/json");
    res->print("{\"ok\":false,\"error\":\"Expected normalized x and y from 0 to 1\"}");
    return;
  }
  std::string zoomValue;
  if (params->getQueryParameter("z", zoomValue)) {
    char *end = nullptr;
    zoom = strtof(zoomValue.c_str(), &end);
    if (end == zoomValue.c_str() || *end != '\0' || !isfinite(zoom) || zoom < 1.0f || zoom > 12.0f) {
      res->setStatusCode(400);
      res->setStatusText("Invalid zoom");
      res->setHeader("Content-Type", "application/json");
      res->print("{\"ok\":false,\"error\":\"Zoom must be between 1 and 12\"}");
      return;
    }
  }
  if (!motorsEnabled) {
    res->setHeader("Content-Type", "application/json");
    res->print("{\"ok\":false,\"enabled\":false}");
    return;
  }
  setTargetFromImage(x, y, zoom);
  res->setHeader("Content-Type", "application/json");
  res->print("{\"ok\":true}");
}

void handleSettings(HTTPRequest *req, HTTPResponse *res) {
  req->discardRequestBody();
  ResourceParameters *params = req->getParams();
  std::string enabledValue, yawSpeedValue, pitchSpeedValue;
  if (!params->getQueryParameter("enabled", enabledValue) ||
      !params->getQueryParameter("yawSpeed", yawSpeedValue) ||
      !params->getQueryParameter("pitchSpeed", pitchSpeedValue) ||
      (enabledValue != "0" && enabledValue != "1")) {
    res->setStatusCode(400);
    res->setStatusText("Invalid settings");
    res->setHeader("Content-Type", "application/json");
    res->print("{\"ok\":false,\"error\":\"Expected enabled=0|1 and yawSpeed/pitchSpeed in degrees per second\"}");
    return;
  }

  char *yawEnd = nullptr;
  char *pitchEnd = nullptr;
  const float yawSpeedDegPerSec = strtof(yawSpeedValue.c_str(), &yawEnd);
  const float pitchSpeedDegPerSec = strtof(pitchSpeedValue.c_str(), &pitchEnd);
  if (yawEnd == yawSpeedValue.c_str() || *yawEnd != '\0' || !isfinite(yawSpeedDegPerSec) ||
      pitchEnd == pitchSpeedValue.c_str() || *pitchEnd != '\0' || !isfinite(pitchSpeedDegPerSec) ||
      yawSpeedDegPerSec < MIN_SPEED_DEG_S || yawSpeedDegPerSec > MAX_SPEED_DEG_S ||
      pitchSpeedDegPerSec < MIN_SPEED_DEG_S || pitchSpeedDegPerSec > MAX_SPEED_DEG_S) {
    res->setStatusCode(400);
    res->setStatusText("Invalid speed");
    res->setHeader("Content-Type", "application/json");
    res->print("{\"ok\":false,\"error\":\"Axis speed must be between 5 and 60 degrees per second\"}");
    return;
  }

  yawAxis.speedLimitStepsPerSecond = yawSpeedDegPerSec * yawAxis.stepsPerDegree;
  pitchAxis.speedLimitStepsPerSecond = pitchSpeedDegPerSec * pitchAxis.stepsPerDegree;
  if (enabledValue == "0") {
    disableMotors();
  } else {
    if (!motorsEnabled) {
      resetAxis(yawAxis, true);
      resetAxis(pitchAxis, true);
      motorsEnabled = true;
    }
    if (USE_ENABLE_PIN) digitalWrite(ENABLE_PIN, LOW);
  }

  res->setHeader("Content-Type", "application/json");
  res->print(motorsEnabled ? "{\"ok\":true,\"enabled\":true}" : "{\"ok\":true,\"enabled\":false}");
}

void handleStop(HTTPRequest *req, HTTPResponse *res) {
  req->discardRequestBody();
  // Stop movement but keep holding torque on the pitch axis. Use the motor
  // toggle to release the drivers when the mount is supported and safe.
  stopAndHold();
  res->setHeader("Content-Type", "application/json");
  res->print("{\"ok\":true,\"holding\":true}");
}

static void updateAxis(AxisControl &axis, uint32_t now) {
  if (!motorsEnabled) return;

  // AccelStepper emits at most one step per call. Service both axes every loop.
  long position = axis.motor->currentPosition();
  const float currentSpeed = axis.motor->speed();
  if ((position <= axis.minSteps && currentSpeed < 0.0f) ||
      (position >= axis.maxSteps && currentSpeed > 0.0f)) {
    axis.speedCommandStepsPerSecond = 0.0f;
    axis.motor->setSpeed(0.0f);
  } else {
    axis.motor->runSpeed();
  }

  const uint32_t elapsedUs = now - axis.lastPidUpdateUs;
  if (elapsedUs < PID_INTERVAL_US) return;

  const float dt = elapsedUs * 1.0e-6f;
  position = axis.motor->currentPosition();
  const float error = (float)(axis.targetSteps - position);
  const float measuredSpeed = (position - axis.previousPosition) / dt;
  float requestedSpeed = 0.0f;

  if (fabsf(error) > PID_DEADBAND_STEPS) {
    const float integralLimit = axis.speedLimitStepsPerSecond / PID_KI;
    const float candidateIntegral = clampFloat(axis.integral + error * dt, -integralLimit, integralLimit);
    const float candidateOutput = PID_KP * error + PID_KI * candidateIntegral - PID_KD * measuredSpeed;
    const bool windingHigh = candidateOutput > axis.speedLimitStepsPerSecond && error > 0.0f;
    const bool windingLow = candidateOutput < -axis.speedLimitStepsPerSecond && error < 0.0f;
    if (!windingHigh && !windingLow) axis.integral = candidateIntegral;

    requestedSpeed = PID_KP * error + PID_KI * axis.integral - PID_KD * measuredSpeed;
    const float stoppingSpeed = sqrtf(2.0f * axis.accelerationStepsPerSecond2 * fabsf(error));
    const float outputLimit = fminf(axis.speedLimitStepsPerSecond, stoppingSpeed);
    requestedSpeed = clampFloat(requestedSpeed, -outputLimit, outputLimit);
  } else {
    axis.integral = 0.0f;
  }

  const float maxChange = axis.accelerationStepsPerSecond2 * dt;
  axis.speedCommandStepsPerSecond += clampFloat(requestedSpeed - axis.speedCommandStepsPerSecond,
                                                -maxChange, maxChange);
  if (fabsf(axis.speedCommandStepsPerSecond) < 0.5f && fabsf(error) <= PID_DEADBAND_STEPS) {
    axis.speedCommandStepsPerSecond = 0.0f;
  }
  axis.motor->setSpeed(axis.speedCommandStepsPerSecond);
  axis.previousPosition = position;
  axis.lastPidUpdateUs = now;
}

void setup() {
  Serial.begin(115200);
  delay(100);
  pinMode(YAW_STEP_PIN, OUTPUT);
  pinMode(YAW_DIR_PIN, OUTPUT);
  pinMode(PITCH_STEP_PIN, OUTPUT);
  pinMode(PITCH_DIR_PIN, OUTPUT);
  if (USE_ENABLE_PIN) {
    pinMode(ENABLE_PIN, OUTPUT);
    digitalWrite(ENABLE_PIN, HIGH);
  }

  yawStepper.setMaxSpeed(MAX_YAW_SPEED_STEPS_S);
  yawStepper.setPinsInverted(false, INVERT_YAW_DIRECTION, false);
  yawStepper.setCurrentPosition(0); // Assembly must be mechanically centered at boot.
  pitchStepper.setMaxSpeed(MAX_PITCH_SPEED_STEPS_S);
  pitchStepper.setPinsInverted(false, INVERT_PITCH_DIRECTION, false);
  pitchStepper.setCurrentPosition(0); // Camera must point level at boot.
  resetAxis(yawAxis, true);
  resetAxis(pitchAxis, true);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  if (!WiFi.softAP(AP_SSID)) {
    Serial.println("Could not start Wi-Fi AP");
    while (true) delay(1000);
  }
  Serial.printf("AP ready: %s, IP %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());

  Serial.println("Generating the local HTTPS certificate (first boot can take a while)...");
  tlsCert = new SSLCert();
  const int certResult = createSelfSignedCert(*tlsCert, KEYSIZE_2048,
      "CN=192.168.4.1,O=ESP32 Soccer Turret", "20261011000000", "20361011000000");
  if (certResult != 0) {
    Serial.printf("Certificate generation failed: 0x%02X\n", certResult);
    while (true) delay(1000);
  }

  httpsServer = new HTTPSServer(tlsCert, 8000, 2);
  httpsServer->registerNode(new ResourceNode("/", "GET", &handleRoot));
  httpsServer->registerNode(new ResourceNode("/status", "GET", &handleStatus));
  httpsServer->registerNode(new ResourceNode("/target", "GET", &handleTarget));
  httpsServer->registerNode(new ResourceNode("/aim", "GET", &handleAim));
  httpsServer->registerNode(new ResourceNode("/settings", "POST", &handleSettings));
  httpsServer->registerNode(new ResourceNode("/stop", "POST", &handleStop));
  httpsServer->start();
  if (!httpsServer->isRunning()) {
    Serial.println("HTTPS server failed to start");
    while (true) delay(1000);
  }
  Serial.println("Open https://192.168.4.1:8000 on the phone.");
  Serial.printf("Yaw limits: +/-%.0f deg; pitch limits: +%.0f/-%.0f deg\n",
                YAW_LIMIT_DEG, PITCH_UP_LIMIT_DEG, PITCH_DOWN_LIMIT_DEG);
}

void loop() {
  httpsServer->loop();
  const uint32_t now = micros();
  updateAxis(yawAxis, now);
  updateAxis(pitchAxis, now);
  delay(1);
}
