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

// Wi-Fi access point
static const char *AP_SSID = "HandTracker-ESP32";

// STEP/DIR driver wiring for a common ESP32 DevKit
static constexpr int STEP_PIN = 26;
static constexpr int DIR_PIN = 27;
static constexpr int ENABLE_PIN = 25; // Optional, active LOW for A4988/DRV8825
static constexpr bool USE_ENABLE_PIN = true;
static constexpr bool INVERT_DIRECTION = false;

// Adjust to your motor, microstep jumper setting, and any gearing.
static constexpr float STEPS_PER_REV = 200.0f;
static constexpr float MICROSTEPS = 16.0f;
static constexpr float GEAR_RATIO = 1.0f;
static constexpr float MAX_ANGLE_DEG = 60.0f;
static constexpr float MAX_SPEED_STEPS_S = 900.0f;
static constexpr float ACCEL_STEPS_S2 = 700.0f;
static constexpr float STEPS_PER_DEG = (STEPS_PER_REV * MICROSTEPS * GEAR_RATIO) / 360.0f;
static constexpr long MAX_STEPS = (long)(MAX_ANGLE_DEG * STEPS_PER_DEG);
static constexpr float MIN_SPEED_DEG_S = 5.0f;
static constexpr float MAX_SPEED_DEG_S = MAX_SPEED_STEPS_S / STEPS_PER_DEG;
static constexpr float DEFAULT_SPEED_DEG_S = 45.0f;
static constexpr float PID_KP = 6.0f;     // proportional: (steps/s) per step
static constexpr float PID_KI = 0.03f;    // integral: (steps/s) per step-second
static constexpr float PID_KD = 0.30f;    // derivative damping against estimated motor speed
static constexpr uint32_t PID_INTERVAL_US = 20000; // 50 Hz controller; step pulses still run every loop

SSLCert *tlsCert = nullptr;
HTTPSServer *httpsServer = nullptr;
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);
bool motorEnabled = false;
long targetSteps = 0;
float speedLimitStepsPerSec = DEFAULT_SPEED_DEG_S * STEPS_PER_DEG;
float speedCommandStepsPerSec = 0.0f;
float pidIntegral = 0.0f;
long previousPidPosition = 0;
uint32_t lastPidUpdateUs = 0;

static float clampFloat(float value, float low, float high) {
  return fmaxf(low, fminf(high, value));
}

static void disableMotor() {
  motorEnabled = false;
  targetSteps = stepper.currentPosition();
  speedCommandStepsPerSec = 0.0f;
  pidIntegral = 0.0f;
  stepper.setSpeed(0.0f);
  if (USE_ENABLE_PIN) digitalWrite(ENABLE_PIN, HIGH);
}

void handleRoot(HTTPRequest *req, HTTPResponse *res) {
  (void)req;
  res->setHeader("Content-Type", "text/html; charset=utf-8");
  res->setHeader("Cache-Control", "no-store");
  res->print(TRACKER_HTML);
}

void handleTarget(HTTPRequest *req, HTTPResponse *res) {
  req->discardRequestBody();
  ResourceParameters *params = req->getParams();
  std::string value;
  if (!params->getQueryParameter("x", value)) {
    res->setStatusCode(400);
    res->setStatusText("Missing x");
    res->setHeader("Content-Type", "text/plain");
    res->print("Expected normalized x from 0 to 1");
    return;
  }

  char *end = nullptr;
  const float x = strtof(value.c_str(), &end);
  if (end == value.c_str() || *end != '\0' || !isfinite(x) || x < 0.0f || x > 1.0f) {
    res->setStatusCode(400);
    res->setStatusText("Invalid x");
    res->setHeader("Content-Type", "text/plain");
    res->print("x must be from 0 to 1");
    return;
  }

  if (!motorEnabled) {
    res->setHeader("Content-Type", "application/json");
    res->print("{\"ok\":false,\"enabled\":false}");
    return;
  }

  // x=0 (left edge) => +MAX angle; x=1 (right edge) => -MAX angle.
  const float angleDeg = (0.5f - x) * 2.0f * MAX_ANGLE_DEG;
  targetSteps = constrain(lroundf(angleDeg * STEPS_PER_DEG), -MAX_STEPS, MAX_STEPS);
  res->setHeader("Content-Type", "application/json");
  res->print("{\"ok\":true}");
}

void handleSettings(HTTPRequest *req, HTTPResponse *res) {
  req->discardRequestBody();
  ResourceParameters *params = req->getParams();
  std::string enabledValue, speedValue;
  if (!params->getQueryParameter("enabled", enabledValue) ||
      !params->getQueryParameter("speed", speedValue) ||
      (enabledValue != "0" && enabledValue != "1")) {
    res->setStatusCode(400);
    res->setStatusText("Invalid settings");
    res->setHeader("Content-Type", "application/json");
    res->print("{\"ok\":false,\"error\":\"Expected enabled=0|1 and speed in degrees per second\"}");
    return;
  }

  char *end = nullptr;
  const float speedDegPerSec = strtof(speedValue.c_str(), &end);
  if (end == speedValue.c_str() || *end != '\0' || !isfinite(speedDegPerSec) ||
      speedDegPerSec < MIN_SPEED_DEG_S || speedDegPerSec > MAX_SPEED_DEG_S) {
    res->setStatusCode(400);
    res->setStatusText("Invalid speed");
    res->setHeader("Content-Type", "application/json");
    res->print("{\"ok\":false,\"error\":\"Speed is outside the supported range\"}");
    return;
  }

  speedLimitStepsPerSec = speedDegPerSec * STEPS_PER_DEG;
  const bool requestedEnabled = enabledValue == "1";
  if (!requestedEnabled) {
    disableMotor();
  } else {
    if (!motorEnabled) {
      targetSteps = stepper.currentPosition();
      speedCommandStepsPerSec = 0.0f;
      pidIntegral = 0.0f;
      previousPidPosition = stepper.currentPosition();
      lastPidUpdateUs = micros();
      motorEnabled = true;
    }
    if (USE_ENABLE_PIN) digitalWrite(ENABLE_PIN, LOW);
  }

  res->setHeader("Content-Type", "application/json");
  res->print(motorEnabled ? "{\"ok\":true,\"enabled\":true}" : "{\"ok\":true,\"enabled\":false}");
}

void handleStop(HTTPRequest *req, HTTPResponse *res) {
  req->discardRequestBody();
  disableMotor();
  res->setHeader("Content-Type", "application/json");
  res->print("{\"ok\":true}");
}

void runPidController() {
  if (!motorEnabled) return;

  // AccelStepper emits at most one pulse per call, so service it every loop.
  stepper.runSpeed();

  const uint32_t now = micros();
  const uint32_t elapsedUs = now - lastPidUpdateUs;
  if (elapsedUs < PID_INTERVAL_US) return;

  const float dt = elapsedUs * 1.0e-6f;
  const long position = stepper.currentPosition();
  const float error = (float)(targetSteps - position);
  const float measuredSpeed = (position - previousPidPosition) / dt;
  const float deadbandSteps = 2.0f;
  float requestedSpeed = 0.0f;

  if (fabsf(error) > deadbandSteps) {
    const float integralLimit = speedLimitStepsPerSec / PID_KI;
    const float candidateIntegral = clampFloat(pidIntegral + error * dt, -integralLimit, integralLimit);
    const float candidateOutput = PID_KP * error + PID_KI * candidateIntegral - PID_KD * measuredSpeed;
    const bool windingFurtherHigh = candidateOutput > speedLimitStepsPerSec && error > 0.0f;
    const bool windingFurtherLow = candidateOutput < -speedLimitStepsPerSec && error < 0.0f;
    if (!windingFurtherHigh && !windingFurtherLow) pidIntegral = candidateIntegral;

    requestedSpeed = PID_KP * error + PID_KI * pidIntegral - PID_KD * measuredSpeed;
    // Respect the physical braking distance as well as the user speed limit.
    const float stoppingSpeed = sqrtf(2.0f * ACCEL_STEPS_S2 * fabsf(error));
    const float outputLimit = fminf(speedLimitStepsPerSec, stoppingSpeed);
    requestedSpeed = clampFloat(requestedSpeed, -outputLimit, outputLimit);
  } else {
    pidIntegral = 0.0f;
  }

  // Slew-limit the PID output to keep motion smooth when the target changes.
  const float maxSpeedChange = ACCEL_STEPS_S2 * dt;
  speedCommandStepsPerSec += clampFloat(requestedSpeed - speedCommandStepsPerSec,
                                        -maxSpeedChange, maxSpeedChange);
  if (fabsf(speedCommandStepsPerSec) < 0.5f && fabsf(error) <= deadbandSteps) {
    speedCommandStepsPerSec = 0.0f;
  }
  stepper.setSpeed(speedCommandStepsPerSec);

  previousPidPosition = position;
  lastPidUpdateUs = now;
}

void setup() {
  Serial.begin(115200);
  delay(100);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  if (USE_ENABLE_PIN) {
    pinMode(ENABLE_PIN, OUTPUT);
    digitalWrite(ENABLE_PIN, HIGH);
  }
  stepper.setMaxSpeed(MAX_SPEED_STEPS_S);
  stepper.setPinsInverted(false, INVERT_DIRECTION, false);
  stepper.setCurrentPosition(0); // Assumes the assembly starts centered.
  stepper.setSpeed(0.0f);
  previousPidPosition = stepper.currentPosition();
  lastPidUpdateUs = micros();

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  if (!WiFi.softAP(AP_SSID)) { // Open access point: no Wi-Fi password
    Serial.println("Could not start Wi-Fi AP");
    while (true) delay(1000);
  }
  Serial.printf("AP ready: %s, IP %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());

  Serial.println("Generating the local HTTPS certificate (first boot can take a while)...");
  tlsCert = new SSLCert();
  const int certResult = createSelfSignedCert(*tlsCert, KEYSIZE_2048,
      "CN=192.168.4.1,O=ESP32 Hand Tracker", "20261011000000", "20361011000000");
  if (certResult != 0) {
    Serial.printf("Certificate generation failed: 0x%02X\n", certResult);
    while (true) delay(1000);
  }

  httpsServer = new HTTPSServer(tlsCert, 8000, 2);
  httpsServer->registerNode(new ResourceNode("/", "GET", &handleRoot));
  httpsServer->registerNode(new ResourceNode("/target", "GET", &handleTarget));
  httpsServer->registerNode(new ResourceNode("/settings", "POST", &handleSettings));
  httpsServer->registerNode(new ResourceNode("/stop", "POST", &handleStop));
  httpsServer->start();
  if (!httpsServer->isRunning()) {
    Serial.println("HTTPS server failed to start");
    while (true) delay(1000);
  }
  Serial.println("Open https://192.168.4.1:8000 on the phone.");
}

void loop() {
  httpsServer->loop();
  runPidController();
  delay(1);
}
