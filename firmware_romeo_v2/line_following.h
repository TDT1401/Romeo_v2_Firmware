#ifndef LINE_FOLLOWING_H
#define LINE_FOLLOWING_H

int lineLastDirection = 0; // -1 = left, 1 = right, 0 = not established
float previousLineError = 0.0;
bool hasPreviousLineError = false;
float filteredLeftLine = 0.0;
float filteredRightLine = 0.0;
bool hasFilteredLineReadings = false;
unsigned long previousLineUpdateMs = 0;
byte lineMissingSamples = 0;
byte lineFoundSamples = 0;
bool lineSearching = false;
byte lineSearchSweep = 0;
unsigned long lineSearchSweepStartMs = 0;

void resetLineFollow() {
  lineLastDirection = 0;
  previousLineError = 0.0;
  hasPreviousLineError = false;
  filteredLeftLine = filteredRightLine = 0.0;
  hasFilteredLineReadings = false;
  previousLineUpdateMs = 0;
  lineMissingSamples = 0;
  lineFoundSamples = 0;
  lineSearching = false;
  lineSearchSweep = 0;
  lineSearchSweepStartMs = 0;
}

float normalizeLineReading(int reading, float whiteValue, float darkValue) {
  return constrain((reading - whiteValue) / (darkValue - whiteValue), 0.0, 1.0);
}

byte clampLineMotorPercent(float speed) {
  return constrain((int)(speed + 0.5), LINE_MIN_SPEED_PERCENT, 100);
}

void startLineSearch(unsigned long nowMs) {
  lineSearching = true;
  lineSearchSweep = 0;
  lineSearchSweepStartMs = nowMs;
  lineFoundSamples = 0;
  hasPreviousLineError = false;
}

void searchForLine(unsigned long nowMs) {
  // Sweep toward the last visible side first, then across the other side.
  // Increasing sweep times make the scan cover both sides of the last heading.
  while (lineSearchSweep < LINE_SEARCH_SWEEPS &&
         nowMs - lineSearchSweepStartMs >=
             LINE_SEARCH_FIRST_SWEEP_MS * (lineSearchSweep + 1UL)) {
    lineSearchSweepStartMs += LINE_SEARCH_FIRST_SWEEP_MS * (lineSearchSweep + 1UL);
    lineSearchSweep++;
  }

  if (lineSearchSweep >= LINE_SEARCH_SWEEPS) {
    stopCar(); // Do not roam indefinitely if the line is outside the scan.
    return;
  }

  int direction = lineLastDirection == 0 ? -1 : lineLastDirection;
  if (lineSearchSweep & 1) direction = -direction;
  if (direction < 0) {
    drive(LINE_SEARCH_TURN_PERCENT, LINE_SEARCH_TURN_PERCENT, LOW, HIGH);
  } else {
    drive(LINE_SEARCH_TURN_PERCENT, LINE_SEARCH_TURN_PERCENT, HIGH, LOW);
  }
}

void followLine() {
  const unsigned long nowMs = millis();
  if (previousLineUpdateMs && nowMs - previousLineUpdateMs < LINE_CONTROL_INTERVAL_MS) return;
  const unsigned long elapsedMs = previousLineUpdateMs ? nowMs - previousLineUpdateMs : LINE_CONTROL_INTERVAL_MS;
  previousLineUpdateMs = nowMs;

  const float leftLineRaw = normalizeLineReading(analogRead(LINE_LEFT_PIN), LINE_LEFT_WHITE_VALUE, LINE_LEFT_DARK_VALUE);
  const float rightLineRaw = normalizeLineReading(analogRead(LINE_RIGHT_PIN), LINE_RIGHT_WHITE_VALUE, LINE_RIGHT_DARK_VALUE);

  if (lineSearching) {
    // Use fresh readings here: the filter can retain a dark value after the
    // sensor has already left the line. Pause briefly to confirm a real hit.
    if (leftLineRaw >= LINE_REACQUIRE_THRESHOLD || rightLineRaw >= LINE_REACQUIRE_THRESHOLD) {
      lineFoundSamples++;
      stopCar();
      if (lineFoundSamples < LINE_REACQUIRE_CONFIRM_SAMPLES) return;
      lineSearching = false;
      lineMissingSamples = 0;
      hasPreviousLineError = false;
      filteredLeftLine = leftLineRaw;
      filteredRightLine = rightLineRaw;
      hasFilteredLineReadings = true;
    } else {
      lineFoundSamples = 0;
      searchForLine(nowMs);
      return;
    }
  }

  if (leftLineRaw < LINE_LOST_THRESHOLD && rightLineRaw < LINE_LOST_THRESHOLD) {
    lineMissingSamples++;
    if (lineMissingSamples >= LINE_LOST_CONFIRM_SAMPLES) {
      startLineSearch(nowMs);
      searchForLine(nowMs);
    }
    // Keep the last steering command through one uncertain sample.
    return;
  }
  lineMissingSamples = 0;

  if (!hasFilteredLineReadings) {
    filteredLeftLine = leftLineRaw;
    filteredRightLine = rightLineRaw;
    hasFilteredLineReadings = true;
  } else {
    filteredLeftLine += LINE_SENSOR_FILTER_ALPHA * (leftLineRaw - filteredLeftLine);
    filteredRightLine += LINE_SENSOR_FILTER_ALPHA * (rightLineRaw - filteredRightLine);
  }

  float error = filteredLeftLine - filteredRightLine;
  if (error > -LINE_ERROR_DEADBAND && error < LINE_ERROR_DEADBAND) error = 0.0;
  if (error > LINE_DIRECTION_THRESHOLD) lineLastDirection = -1;
  else if (error < -LINE_DIRECTION_THRESHOLD) lineLastDirection = 1;

  if (error >= LINE_SHARP_CORNER_THRESHOLD) {
    drive(0, LINE_SHARP_CORNER_OUTER_PERCENT, HIGH, HIGH);
  } else if (error <= -LINE_SHARP_CORNER_THRESHOLD) {
    drive(LINE_SHARP_CORNER_OUTER_PERCENT, 0, HIGH, HIGH);
  } else {
    const float derivative = hasPreviousLineError ?
        (error - previousLineError) * LINE_CONTROL_INTERVAL_MS / elapsedMs : 0.0;
    const float correction = constrain(LINE_KP * error + LINE_KD * derivative,
                                       -LINE_MAX_CORRECTION_PERCENT, LINE_MAX_CORRECTION_PERCENT);
    const float absoluteError = error < 0.0 ? -error : error;
    const float cornerBaseSpeed = LINE_BASE_SPEED_PERCENT - LINE_CORNER_SLOWDOWN_PERCENT * absoluteError;
    const byte leftSpeed = clampLineMotorPercent(cornerBaseSpeed - correction);
    const byte rightSpeed = clampLineMotorPercent(cornerBaseSpeed + correction);
    drive(leftSpeed, rightSpeed, HIGH, HIGH);
  }

  previousLineError = error;
  hasPreviousLineError = true;
}

#endif
