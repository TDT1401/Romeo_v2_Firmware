#ifndef LINE_FOLLOWING_H
#define LINE_FOLLOWING_H

struct LineState {
  int direction; // -1 = left, 1 = right, 0 = unknown
  float leftFiltered, rightFiltered;
  byte leftSpeed, rightSpeed, missing, found;
  bool filtered, motorCommand, searching;
  unsigned long lastUpdateMs, searchStartMs;
};
LineState lineState = {};

void resetLineFollow() {
  lineState = LineState();
}

float normalizeLineReading(int reading, float whiteValue, float darkValue) {
  return constrain((reading - whiteValue) / (darkValue - whiteValue), 0.0, 1.0);
}

byte clampLineMotorPercent(float speed) {
  return constrain((int)(speed + 0.5), LINE_MIN_SPEED_PERCENT, 100);
}

byte approachLineMotorSpeed(byte current, byte target) {
  if (target > current + LINE_MOTOR_SLEW_PERCENT_PER_UPDATE) {
    return current + LINE_MOTOR_SLEW_PERCENT_PER_UPDATE;
  }
  if (current > target + LINE_MOTOR_SLEW_PERCENT_PER_UPDATE) {
    return current - LINE_MOTOR_SLEW_PERCENT_PER_UPDATE;
  }
  return target;
}

void driveLineSmoothly(byte leftTarget, byte rightTarget) {
  if (lineState.motorCommand) {
    leftTarget = approachLineMotorSpeed(lineState.leftSpeed, leftTarget);
    rightTarget = approachLineMotorSpeed(lineState.rightSpeed, rightTarget);
  }
  drive(leftTarget, rightTarget, HIGH, HIGH);
  lineState.leftSpeed = leftTarget;
  lineState.rightSpeed = rightTarget;
  lineState.motorCommand = true;
}

void startLineSearch(unsigned long nowMs) {
  lineState.searching = true;
  lineState.searchStartMs = nowMs;
  lineState.found = 0;
  lineState.motorCommand = false;
}

void searchForLine(unsigned long nowMs) {
  // Turn toward the last visible side, then sweep the other side.
  unsigned long elapsed = nowMs - lineState.searchStartMs;
  byte sweep = 0;
  while (sweep < LINE_SEARCH_SWEEPS && elapsed >= LINE_SEARCH_FIRST_SWEEP_MS * (sweep + 1UL)) {
    elapsed -= LINE_SEARCH_FIRST_SWEEP_MS * (sweep + 1UL);
    sweep++;
  }

  if (sweep >= LINE_SEARCH_SWEEPS) {
    stopCar(); // Do not roam indefinitely if the line is outside the scan.
    return;
  }

  int direction = lineState.direction == 0 ? -1 : lineState.direction;
  if (sweep & 1) direction = -direction;
  if (direction < 0) {
    drive(LINE_SEARCH_TURN_PERCENT, LINE_SEARCH_TURN_PERCENT, LOW, HIGH);
  } else {
    drive(LINE_SEARCH_TURN_PERCENT, LINE_SEARCH_TURN_PERCENT, HIGH, LOW);
  }
}

void followLine() {
  const unsigned long nowMs = millis();
  if (lineState.lastUpdateMs && nowMs - lineState.lastUpdateMs < LINE_CONTROL_INTERVAL_MS) return;
  lineState.lastUpdateMs = nowMs;

  const float leftLineRaw = normalizeLineReading(analogRead(LINE_LEFT_PIN), LINE_LEFT_WHITE_VALUE, LINE_LEFT_DARK_VALUE);
  const float rightLineRaw = normalizeLineReading(analogRead(LINE_RIGHT_PIN), LINE_RIGHT_WHITE_VALUE, LINE_RIGHT_DARK_VALUE);

  if (LINE_WHITE_WHITE_IS_CENTERED &&
      leftLineRaw < LINE_LOST_THRESHOLD && rightLineRaw < LINE_LOST_THRESHOLD) {
    // In this sensor layout the line is between the sensors. Keep moving
    // straight instead of treating two white readings as a lost line.
    lineState.searching = false;
    lineState.missing = 0;
    lineState.found = 0;
    lineState.leftFiltered = leftLineRaw;
    lineState.rightFiltered = rightLineRaw;
    lineState.filtered = true;
    driveLineSmoothly(LINE_BASE_SPEED_PERCENT, LINE_BASE_SPEED_PERCENT);
    return;
  }

  if (lineState.searching) {
    // Use fresh readings here: the filter can retain a dark value after the
    // sensor has already left the line. Pause briefly to confirm a real hit.
    if (leftLineRaw >= LINE_REACQUIRE_THRESHOLD || rightLineRaw >= LINE_REACQUIRE_THRESHOLD) {
      lineState.found++;
      stopCar();
      if (lineState.found < LINE_REACQUIRE_CONFIRM_SAMPLES) return;
      lineState.searching = false;
      lineState.missing = 0;
      lineState.motorCommand = false;
      lineState.leftFiltered = leftLineRaw;
      lineState.rightFiltered = rightLineRaw;
      lineState.filtered = true;
    } else {
      lineState.found = 0;
      searchForLine(nowMs);
      return;
    }
  }

  if (leftLineRaw < LINE_LOST_THRESHOLD && rightLineRaw < LINE_LOST_THRESHOLD) {
    lineState.missing++;
    if (lineState.missing >= LINE_LOST_CONFIRM_SAMPLES) {
      startLineSearch(nowMs);
      searchForLine(nowMs);
    }
    // Keep the last steering command through one uncertain sample.
    return;
  }
  lineState.missing = 0;

  if (!lineState.filtered) {
    lineState.leftFiltered = leftLineRaw;
    lineState.rightFiltered = rightLineRaw;
    lineState.filtered = true;
  } else {
    lineState.leftFiltered += LINE_SENSOR_FILTER_ALPHA * (leftLineRaw - lineState.leftFiltered);
    lineState.rightFiltered += LINE_SENSOR_FILTER_ALPHA * (rightLineRaw - lineState.rightFiltered);
  }

  float error = lineState.leftFiltered - lineState.rightFiltered;
  if (error > -LINE_ERROR_DEADBAND && error < LINE_ERROR_DEADBAND) error = 0.0;
  if (error > LINE_DIRECTION_THRESHOLD) lineState.direction = -1;
  else if (error < -LINE_DIRECTION_THRESHOLD) lineState.direction = 1;

  const float correction = LINE_KP * error;
  const float absoluteError = error < 0.0 ? -error : error;
  const float cornerBaseSpeed = LINE_BASE_SPEED_PERCENT - LINE_CORNER_SLOWDOWN_PERCENT * absoluteError;
  const byte leftSpeed = clampLineMotorPercent(cornerBaseSpeed - correction);
  const byte rightSpeed = clampLineMotorPercent(cornerBaseSpeed + correction);
  driveLineSmoothly(leftSpeed, rightSpeed);
}

#endif
