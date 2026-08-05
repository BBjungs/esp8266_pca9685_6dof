#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <cstdlib>
#include <cstring>
#include <strings.h>

#include "web_ui.h"

// ============================================================
// ESP8266 NodeMCU -> PCA9685
// D2 / GPIO4 = SDA
// D1 / GPIO5 = SCL
// PCA9685 I2C address = 0x40
// ============================================================

constexpr uint8_t SDA_PIN = D2;
constexpr uint8_t SCL_PIN = D1;
constexpr uint8_t PCA9685_ADDRESS = 0x40;
constexpr uint8_t SERVO_COUNT = 6;
constexpr float SERVO_FREQUENCY_HZ = 50.0F;
constexpr uint16_t STEP_DELAY_MS = 18;
constexpr char WIFI_AP_SSID[] = "RobotArm-6DOF";
constexpr char WIFI_AP_PASSWORD[] = "robotarm";
constexpr char MDNS_HOSTNAME[] = "robotarm";

Adafruit_PWMServoDriver pwm(PCA9685_ADDRESS);
ESP8266WebServer webServer(80);

struct JointConfig {
  const char *name;
  uint8_t channel;
  uint16_t minPulseUs;
  uint16_t maxPulseUs;
  int16_t minAngle;
  int16_t maxAngle;
  int16_t homeAngle;
  bool reversed;
};

/*
 * IMPORTANT:
 * These are conservative starting values, not final calibration values.
 * Adjust minAngle, maxAngle, homeAngle and reversed for the real arm.
 *
 * Joint mapping:
 * 0 Base
 * 1 Shoulder
 * 2 Elbow
 * 3 Wrist pitch
 * 4 Wrist rotate
 * 5 Gripper
 */
JointConfig joints[SERVO_COUNT] = {
    {"Base",         0, 1000, 2000, 10, 170, 90, false},
    {"Shoulder",     1, 1000, 2000, 10, 170, 90, false},
    {"Elbow",        2, 1000, 2000, 10, 170, 90, false},
    {"WristPitch",   3, 1000, 2000, 10, 170, 90, false},
    {"WristRotate",  4, 1000, 2000, 10, 170, 90, false},
    {"Gripper",      5, 1000, 2000, 10, 170, 80, false},
};

// -1 means the controller does not know the real physical position yet.
int16_t currentAngle[SERVO_COUNT] = {-1, -1, -1, -1, -1, -1};
int16_t targetAngle[SERVO_COUNT] = {-1, -1, -1, -1, -1, -1};
bool controlsLocked = true;
bool mdnsReady = false;
uint32_t lastMotionStepMs = 0;
int8_t homeJointId = -1;
uint32_t homeAdvanceAtMs = 0;

constexpr size_t COMMAND_BUFFER_SIZE = 64;
char commandBuffer[COMMAND_BUFFER_SIZE] = {};
size_t commandLength = 0;
bool commandOverflow = false;

bool parseInteger(const char *text, long &value) {
  if (text == nullptr || *text == '\0') {
    return false;
  }

  char *end = nullptr;
  value = strtol(text, &end, 10);
  return end != text && *end == '\0';
}

int16_t clampJointAngle(uint8_t jointId, int16_t angle) {
  return constrain(angle, joints[jointId].minAngle, joints[jointId].maxAngle);
}

uint16_t angleToPulseUs(uint8_t jointId, int16_t logicalAngle) {
  logicalAngle = clampJointAngle(jointId, logicalAngle);

  int16_t servoAngle = logicalAngle;
  if (joints[jointId].reversed) {
    servoAngle = 180 - logicalAngle;
  }

  return static_cast<uint16_t>(
      map(servoAngle, 0, 180,
          joints[jointId].minPulseUs,
          joints[jointId].maxPulseUs));
}

void writeJoint(uint8_t jointId, int16_t angle) {
  if (jointId >= SERVO_COUNT) {
    return;
  }

  angle = clampJointAngle(jointId, angle);
  pwm.writeMicroseconds(joints[jointId].channel,
                        angleToPulseUs(jointId, angle));
  currentAngle[jointId] = angle;
}

void setJointTarget(uint8_t jointId, int16_t requestedAngle) {
  if (jointId >= SERVO_COUNT) {
    return;
  }

  requestedAngle = clampJointAngle(jointId, requestedAngle);

  // The first command cannot know the true servo position.
  // It sends one direct target. Later commands move smoothly.
  if (currentAngle[jointId] < 0) {
    writeJoint(jointId, requestedAngle);
    targetAngle[jointId] = requestedAngle;
    return;
  }

  targetAngle[jointId] = requestedAngle;
}

bool isJointMoving(uint8_t jointId) {
  return jointId < SERVO_COUNT &&
         currentAngle[jointId] >= 0 &&
         targetAngle[jointId] >= 0 &&
         currentAngle[jointId] != targetAngle[jointId];
}

void updateMotion() {
  const uint32_t now = millis();
  if (now - lastMotionStepMs < STEP_DELAY_MS) {
    return;
  }
  lastMotionStepMs = now;

  for (uint8_t jointId = 0; jointId < SERVO_COUNT; ++jointId) {
    if (!isJointMoving(jointId)) {
      continue;
    }

    currentAngle[jointId] +=
        (currentAngle[jointId] < targetAngle[jointId]) ? 1 : -1;
    writeJoint(jointId, currentAngle[jointId]);
  }
}

void updateHomeSequence() {
  if (homeJointId < 0 ||
      isJointMoving(static_cast<uint8_t>(homeJointId))) {
    return;
  }

  const uint32_t now = millis();
  if (homeAdvanceAtMs == 0) {
    homeAdvanceAtMs = now + 150;
    return;
  }
  if (static_cast<int32_t>(now - homeAdvanceAtMs) < 0) {
    return;
  }

  ++homeJointId;
  homeAdvanceAtMs = 0;
  if (homeJointId >= SERVO_COUNT) {
    homeJointId = -1;
    Serial.println(F("HOME complete."));
    return;
  }

  setJointTarget(static_cast<uint8_t>(homeJointId),
                 joints[homeJointId].homeAngle);
}

void disableJoint(uint8_t jointId) {
  if (jointId >= SERVO_COUNT) {
    return;
  }

  // setPin(channel, 0) sets the output fully OFF.
  pwm.setPin(joints[jointId].channel, 0);
  currentAngle[jointId] = -1;
  targetAngle[jointId] = -1;
}

void disableAllJoints() {
  homeJointId = -1;
  homeAdvanceAtMs = 0;
  for (uint8_t id = 0; id < SERVO_COUNT; ++id) {
    disableJoint(id);
  }
}

bool moveHome() {
  if (controlsLocked) {
    Serial.println(F("ERROR: Controls are locked. Use UNLOCK first."));
    return false;
  }

  Serial.println(F("Moving to HOME one joint at a time..."));
  for (uint8_t id = 0; id < SERVO_COUNT; ++id) {
    if (currentAngle[id] >= 0) {
      targetAngle[id] = currentAngle[id];
    }
  }
  homeJointId = 0;
  homeAdvanceAtMs = 0;
  setJointTarget(0, joints[0].homeAngle);
  return true;
}

void printStatus() {
  Serial.println();
  Serial.println(F("Joint status"));
  Serial.println(F("------------------------------------------"));
  Serial.printf("Controls: %s\n", controlsLocked ? "LOCKED" : "UNLOCKED");

  for (uint8_t id = 0; id < SERVO_COUNT; ++id) {
    Serial.printf(
        "J%u %-12s CH%u range=%d..%d home=%d current=",
        id,
        joints[id].name,
        joints[id].channel,
        joints[id].minAngle,
        joints[id].maxAngle,
        joints[id].homeAngle);

    if (currentAngle[id] < 0) {
      Serial.println(F("OFF/UNKNOWN"));
    } else {
      Serial.printf("%d deg\n", currentAngle[id]);
    }
  }

  Serial.println();
}

void printHelp() {
  Serial.println();
  Serial.println(F("ESP8266 + PCA9685 + 6DOF Robot Arm"));
  Serial.println(F("========================================"));
  Serial.println(F("Commands:"));
  Serial.println(F("  M <joint> <angle>  Move one joint"));
  Serial.println(F("  <joint> <angle>    Short form"));
  Serial.println(F("  HOME               Move all joints home"));
  Serial.println(F("  LOCK               Block movement commands"));
  Serial.println(F("  UNLOCK             Allow movement commands"));
  Serial.println(F("  OFF                Disable all servo outputs"));
  Serial.println(F("  OFF <joint>        Disable one servo output"));
  Serial.println(F("  STATUS             Show joint configuration"));
  Serial.println(F("  HELP               Show this help"));
  Serial.println();
  Serial.println(F("Examples:"));
  Serial.println(F("  M 0 90"));
  Serial.println(F("  1 75"));
  Serial.println(F("  OFF 5"));
  Serial.println();
  Serial.println(F("Joint numbers:"));
  Serial.println(F("  0 Base"));
  Serial.println(F("  1 Shoulder"));
  Serial.println(F("  2 Elbow"));
  Serial.println(F("  3 Wrist pitch"));
  Serial.println(F("  4 Wrist rotate"));
  Serial.println(F("  5 Gripper"));
  Serial.println();
}

void executeMove(const char *jointText, const char *angleText) {
  long jointValue = 0;
  long angleValue = 0;

  if (!parseInteger(jointText, jointValue) ||
      !parseInteger(angleText, angleValue)) {
    Serial.println(F("ERROR: Use M <joint 0..5> <angle>."));
    return;
  }

  if (jointValue < 0 || jointValue >= SERVO_COUNT) {
    Serial.println(F("ERROR: Joint must be 0..5."));
    return;
  }

  if (controlsLocked) {
    Serial.println(F("ERROR: Controls are locked. Use UNLOCK first."));
    return;
  }

  const uint8_t jointId = static_cast<uint8_t>(jointValue);
  int16_t safeAngle = 0;
  if (angleValue < joints[jointId].minAngle) {
    safeAngle = joints[jointId].minAngle;
  } else if (angleValue > joints[jointId].maxAngle) {
    safeAngle = joints[jointId].maxAngle;
  } else {
    safeAngle = static_cast<int16_t>(angleValue);
  }

  if (safeAngle != angleValue) {
    Serial.printf(
        "Angle limited from %ld to %d deg for J%u.\n",
        angleValue, safeAngle, jointId);
  }

  Serial.printf("Moving J%u %-12s -> %d deg\n",
                jointId, joints[jointId].name, safeAngle);
  homeJointId = -1;
  homeAdvanceAtMs = 0;
  setJointTarget(jointId, safeAngle);
  Serial.println(F("OK: target queued"));
}

void processCommand(char *line) {
  char *savePtr = nullptr;
  char *first = strtok_r(line, " \t", &savePtr);

  if (first == nullptr) {
    return;
  }

  if (strcasecmp(first, "HELP") == 0 ||
      strcmp(first, "?") == 0) {
    printHelp();
    return;
  }

  if (strcasecmp(first, "STATUS") == 0) {
    printStatus();
    return;
  }

  if (strcasecmp(first, "LOCK") == 0) {
    controlsLocked = true;
    homeJointId = -1;
    homeAdvanceAtMs = 0;
    Serial.println(F("Movement controls are LOCKED."));
    return;
  }

  if (strcasecmp(first, "UNLOCK") == 0) {
    controlsLocked = false;
    Serial.println(F("Movement controls are UNLOCKED."));
    return;
  }

  if (strcasecmp(first, "HOME") == 0) {
    moveHome();
    return;
  }

  if (strcasecmp(first, "OFF") == 0) {
    char *jointText = strtok_r(nullptr, " \t", &savePtr);

    if (jointText == nullptr) {
      disableAllJoints();
      Serial.println(F("All servo outputs are OFF."));
      return;
    }

    long jointValue = 0;
    if (!parseInteger(jointText, jointValue) ||
        jointValue < 0 || jointValue >= SERVO_COUNT) {
      Serial.println(F("ERROR: OFF joint must be 0..5."));
      return;
    }

    disableJoint(static_cast<uint8_t>(jointValue));
    Serial.printf("J%ld output is OFF.\n", jointValue);
    return;
  }

  if (strcasecmp(first, "M") == 0 ||
      strcasecmp(first, "MOVE") == 0) {
    executeMove(
        strtok_r(nullptr, " \t", &savePtr),
        strtok_r(nullptr, " \t", &savePtr));
    return;
  }

  // Short command form: <joint> <angle>
  long possibleJoint = 0;
  if (parseInteger(first, possibleJoint)) {
    executeMove(first, strtok_r(nullptr, " \t", &savePtr));
    return;
  }

  Serial.println(F("ERROR: Unknown command. Type HELP."));
}

void readSerialCommands() {
  while (Serial.available() > 0) {
    const char received = static_cast<char>(Serial.read());

    if (received == '\r') {
      continue;
    }

    if (received == '\n') {
      if (commandOverflow) {
        Serial.println(F("ERROR: Command is too long."));
      } else if (commandLength > 0) {
        commandBuffer[commandLength] = '\0';
        processCommand(commandBuffer);
      }

      commandLength = 0;
      commandOverflow = false;
      commandBuffer[0] = '\0';
      continue;
    }

    if (commandOverflow) {
      continue;
    }

    if (commandLength < COMMAND_BUFFER_SIZE - 1) {
      commandBuffer[commandLength++] = received;
    } else {
      commandOverflow = true;
    }
  }
}

void sendApiError(int statusCode, const __FlashStringHelper *message) {
  String json;
  json.reserve(128);
  json += F(R"json({"ok":false,"error":")json");
  json += message;
  json += F(R"json("})json");
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send(statusCode, "application/json; charset=utf-8", json);
}

void sendApiOk() {
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send(200, "application/json", R"json({"ok":true})json");
}

bool parseWebInteger(const char *argumentName, long &value) {
  if (!webServer.hasArg(argumentName)) {
    return false;
  }

  const String text = webServer.arg(argumentName);
  if (text.length() == 0) {
    return false;
  }

  const char *start = text.c_str();
  char *end = nullptr;
  value = strtol(start, &end, 10);
  return end != start && *end == '\0';
}

void sendStatusJson() {
  String json;
  json.reserve(1400);
  json += F(R"json({"ok":true,"ssid":")json");
  json += WIFI_AP_SSID;
  json += F(R"json(","ip":")json");
  json += WiFi.softAPIP().toString();
  json += F(R"json(","locked":)json");
  json += controlsLocked ? F("true") : F("false");
  json += F(R"json(,"uptimeMs":)json");
  json += millis();
  json += F(R"json(,"joints":[)json");

  for (uint8_t id = 0; id < SERVO_COUNT; ++id) {
    if (id > 0) {
      json += ',';
    }
    json += F(R"json({"id":)json");
    json += static_cast<unsigned int>(id);
    json += F(R"json(,"name":")json");
    json += joints[id].name;
    json += F(R"json(","channel":)json");
    json += static_cast<unsigned int>(joints[id].channel);
    json += F(R"json(,"min":)json");
    json += joints[id].minAngle;
    json += F(R"json(,"max":)json");
    json += joints[id].maxAngle;
    json += F(R"json(,"home":)json");
    json += joints[id].homeAngle;
    json += F(R"json(,"current":)json");
    json += currentAngle[id];
    json += F(R"json(,"target":)json");
    json += targetAngle[id];
    json += F(R"json(,"enabled":)json");
    json += currentAngle[id] >= 0 ? F("true") : F("false");
    json += F(R"json(,"moving":)json");
    json += isJointMoving(id) ? F("true") : F("false");
    json += '}';
  }

  json += F("]}");
  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send(200, "application/json", json);
}

void handleJointRequest() {
  if (controlsLocked) {
    sendApiError(423, F("ระบบถูกล็อก กรุณาปลดล็อกก่อน"));
    return;
  }

  long jointValue = 0;
  long angleValue = 0;
  if (!parseWebInteger("id", jointValue) ||
      jointValue < 0 || jointValue >= SERVO_COUNT) {
    sendApiError(400, F("หมายเลขข้อต่อไม่ถูกต้อง"));
    return;
  }
  if (!parseWebInteger("angle", angleValue)) {
    sendApiError(400, F("ค่ามุมไม่ถูกต้อง"));
    return;
  }

  const uint8_t jointId = static_cast<uint8_t>(jointValue);
  if (angleValue < joints[jointId].minAngle ||
      angleValue > joints[jointId].maxAngle) {
    sendApiError(400, F("ค่ามุมอยู่นอกช่วงปลอดภัย"));
    return;
  }

  homeJointId = -1;
  homeAdvanceAtMs = 0;
  setJointTarget(jointId, static_cast<int16_t>(angleValue));
  sendApiOk();
}

void handleLockRequest() {
  long lockedValue = 0;
  if (!parseWebInteger("locked", lockedValue) ||
      (lockedValue != 0 && lockedValue != 1)) {
    sendApiError(400, F("สถานะล็อกไม่ถูกต้อง"));
    return;
  }

  controlsLocked = lockedValue == 1;
  if (controlsLocked) {
    homeJointId = -1;
    homeAdvanceAtMs = 0;
  }
  sendApiOk();
}

void handleHomeRequest() {
  if (!moveHome()) {
    sendApiError(423, F("ระบบถูกล็อก กรุณาปลดล็อกก่อน"));
    return;
  }
  sendApiOk();
}

void handleOffRequest() {
  homeJointId = -1;
  homeAdvanceAtMs = 0;

  if (!webServer.hasArg("id")) {
    disableAllJoints();
    sendApiOk();
    return;
  }

  long jointValue = 0;
  if (!parseWebInteger("id", jointValue) ||
      jointValue < 0 || jointValue >= SERVO_COUNT) {
    sendApiError(400, F("หมายเลขข้อต่อไม่ถูกต้อง"));
    return;
  }

  disableJoint(static_cast<uint8_t>(jointValue));
  sendApiOk();
}

void handleEmergencyStop() {
  disableAllJoints();
  controlsLocked = true;
  Serial.println(F("EMERGENCY STOP: All outputs OFF and controls locked."));
  sendApiOk();
}

void setupWiFiAndWeb() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);

  if (!WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD)) {
    Serial.println(F("ERROR: Could not start the Wi-Fi access point."));
    return;
  }

  webServer.on("/", HTTP_GET, []() {
    webServer.send_P(200, PSTR("text/html; charset=utf-8"), WEB_UI_HTML);
  });
  webServer.on("/api/status", HTTP_GET, sendStatusJson);
  webServer.on("/api/joint", HTTP_POST, handleJointRequest);
  webServer.on("/api/lock", HTTP_POST, handleLockRequest);
  webServer.on("/api/home", HTTP_POST, handleHomeRequest);
  webServer.on("/api/off", HTTP_POST, handleOffRequest);
  webServer.on("/api/off/joint", HTTP_POST, handleOffRequest);
  webServer.on("/api/stop", HTTP_POST, handleEmergencyStop);
  webServer.on("/favicon.ico", HTTP_GET, []() {
    webServer.send(204, "text/plain", "");
  });
  webServer.onNotFound([]() {
    sendApiError(404, F("ไม่พบเส้นทางที่ร้องขอ"));
  });
  webServer.begin();

  mdnsReady = MDNS.begin(MDNS_HOSTNAME);
  if (mdnsReady) {
    MDNS.addService("http", "tcp", 80);
  }

  Serial.println();
  Serial.println(F("Web controller ready"));
  Serial.printf("  Wi-Fi:    %s\n", WIFI_AP_SSID);
  Serial.printf("  Password: %s\n", WIFI_AP_PASSWORD);
  Serial.printf("  Open:     http://%s/\n",
                WiFi.softAPIP().toString().c_str());
  if (mdnsReady) {
    Serial.printf("  Or open:  http://%s.local/\n", MDNS_HOSTNAME);
  }
}

void setup() {
  Serial.begin(115200);
  delay(250);

  Serial.println();
  Serial.println(F("Starting 6DOF robot arm controller..."));

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  if (!pwm.begin()) {
    Serial.println(F("FATAL: PCA9685 not found at I2C address 0x40."));
    Serial.println(F("Check VCC, GND, SDA and SCL wiring."));

    while (true) {
      delay(1000);
      yield();
    }
  }

  pwm.setPWMFreq(SERVO_FREQUENCY_HZ);
  delay(10);

  // Do not move the arm automatically at boot.
  disableAllJoints();

  setupWiFiAndWeb();

  Serial.println(F("PCA9685 detected. Servo outputs remain OFF."));
  Serial.println(F("Controls start LOCKED. Use the web UI or type UNLOCK."));
  Serial.println(F("Type HELP and press Enter for serial commands."));
}

void loop() {
  readSerialCommands();
  updateMotion();
  updateHomeSequence();
  webServer.handleClient();
  if (mdnsReady) {
    MDNS.update();
  }
  yield();
}
