// Motor pins and speeds (percentage: 0-100%)
const byte E1 = 5, E2 = 6, M1 = 4, M2 = 7;
const byte FORWARD_SPEED_LEFT_PERCENT  = 51;
const byte FORWARD_SPEED_RIGHT_PERCENT = 50;
const byte BACKWARD_SPEED_LEFT_PERCENT  = 39;
const byte BACKWARD_SPEED_RIGHT_PERCENT = 39;
const byte TURN_SPEED_LEFT_PERCENT  = 39;
const byte TURN_SPEED_RIGHT_PERCENT = 39;

// Analog line sensors: readings are higher over the dark line.
// A2 is physically on the right; A3 is physically on the left.
const byte LINE_LEFT_PIN = A3;
const byte LINE_RIGHT_PIN = A2;
// Keep calibration paired with the sensor it was measured from.
const float LINE_LEFT_WHITE_VALUE = 24.8;   // A3
const float LINE_LEFT_DARK_VALUE = 1011.6;  // A3
const float LINE_RIGHT_WHITE_VALUE = 29.7;  // A2
const float LINE_RIGHT_DARK_VALUE = 1012.2; // A2
// Line-follow tuning (percentage of motor speed).
const byte LINE_BASE_SPEED_PERCENT = 34;
const byte LINE_MIN_SPEED_PERCENT = 26;
const byte LINE_CORNER_SLOWDOWN_PERCENT = 4;
const float LINE_KP = 10.0;
const float LINE_LOST_THRESHOLD = 0.12;
const float LINE_REACQUIRE_THRESHOLD = 0.30;
const float LINE_ERROR_DEADBAND = 0.08;
const float LINE_DIRECTION_THRESHOLD = 0.15;
const float LINE_SENSOR_FILTER_ALPHA = 0.45;
const unsigned long LINE_CONTROL_INTERVAL_MS = 10;
const byte LINE_MOTOR_SLEW_PERCENT_PER_UPDATE = 3;
const byte LINE_SEARCH_TURN_PERCENT = 32;
const byte LINE_LOST_CONFIRM_SAMPLES = 6;
const byte LINE_REACQUIRE_CONFIRM_SAMPLES = 2;
const byte LINE_SEARCH_SWEEPS = 4;
const unsigned long LINE_SEARCH_FIRST_SWEEP_MS = 400;

// HC-SR04 pins
const byte TRIG_PIN_1 = 8, ECHO_PIN_1 = 9;
const byte TRIG_PIN_2 = 10, ECHO_PIN_2 = 11;
const int SAFE_DISTANCE = 20;
const int TURN_90_TIME = 250;

// Romeo V2 button inputs are read from analog pin A0.
const byte BUTTON_PIN = A0;
const int BUTTON_LEVELS[] = {30, 150, 360, 535, 760};
const byte BUTTON_COUNT = 5;
enum CarMode { STOPPED, OBSTACLE_AVOIDANCE_MODE, LINE_FOLLOWING_MODE };
CarMode carMode = STOPPED;
int lastButton = -1;

#include "motor_control.h"
#include "line_following.h"
#include "distance_sensors.h"
#include "buttons_modes.h"

void setup() {
  pinMode(E1, OUTPUT);
  pinMode(E2, OUTPUT);
  pinMode(M1, OUTPUT);
  pinMode(M2, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(LINE_LEFT_PIN, INPUT);
  pinMode(LINE_RIGHT_PIN, INPUT);
  pinMode(TRIG_PIN_1, OUTPUT);
  pinMode(ECHO_PIN_1, INPUT);
  pinMode(TRIG_PIN_2, OUTPUT);
  pinMode(ECHO_PIN_2, INPUT);

  digitalWrite(TRIG_PIN_1, LOW);
  digitalWrite(TRIG_PIN_2, LOW);
  Serial.begin(19200);
  delay(500);
  selectStopMode();
}
void loop() {
  handleButtons();
  if (carMode == STOPPED) {
    delay(20);
    return;
  }
  if (carMode == LINE_FOLLOWING_MODE) {
    followLine();
    delay(5);
    return;
  }

  float distance1 = getDistance(TRIG_PIN_1, ECHO_PIN_1);
  delay(50);
  float distance2 = getDistance(TRIG_PIN_2, ECHO_PIN_2);
  if (isObstacle(distance1) || isObstacle(distance2)) {
    avoidObstacle();
    handleButtons();
  }
  if (carMode == OBSTACLE_AVOIDANCE_MODE) goForward();
  delay(carMode == LINE_FOLLOWING_MODE ? 5 : 50);
}
