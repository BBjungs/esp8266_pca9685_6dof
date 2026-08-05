# ESP8266 Web Controller

## Quick start

1. Build and upload the firmware to the NodeMCU ESP8266.
2. On a phone or computer, connect to Wi-Fi:
   - SSID: RobotArm-6DOF
   - Password: robotarm
3. Open http://192.168.4.1/ in a browser.
4. Press Unlock before sending movement commands.
5. Select a joint from the sidebar and use the slider, angle input, or step buttons.

The controller also advertises http://robotarm.local/ through mDNS. The IP
address works on devices that do not support mDNS.

## Safety behavior

- The controller starts locked with every PCA9685 output disabled.
- Joint requests outside the configured safe range are rejected.
- Home moves one joint at a time.
- Lock blocks new movement commands but keeps active servos holding position.
- Emergency Stop disables every PWM output and locks the controller.
- A disabled servo may move under gravity. Support the arm before using Stop.

Use a separate regulated supply for the servos. Connect the servo supply ground,
PCA9685 ground, and ESP8266 ground together. Do not power six servos from the
NodeMCU 5 V or 3.3 V pin.

## Configuration

Wi-Fi settings are near the top of src/main.cpp:

- WIFI_AP_SSID
- WIFI_AP_PASSWORD
- MDNS_HOSTNAME

Joint channel, safe angle, home angle, pulse width, and reverse direction are in
the joints array. Calibrate one joint at a time with the arm unloaded.

## HTTP API

All POST endpoints use application/x-www-form-urlencoded values.

| Method | Path | Values | Purpose |
| --- | --- | --- | --- |
| GET | /api/status | - | Read lock, network, and joint state |
| POST | /api/lock | locked=0 or 1 | Unlock or lock controls |
| POST | /api/joint | id=0..5, angle=safe range | Move one joint |
| POST | /api/home | - | Start the Home sequence |
| POST | /api/off | id=0..5 | Disable one joint |
| POST | /api/off | - | Disable every joint |
| POST | /api/stop | - | Emergency stop and lock |

Serial commands remain available at 115200 baud. The new LOCK and UNLOCK
commands use the same safety lock as the web interface.
