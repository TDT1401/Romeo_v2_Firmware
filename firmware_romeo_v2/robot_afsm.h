#ifndef ROBOT_AFSM_H
#define ROBOT_AFSM_H

enum AvoidStage { TURN_OUT, MOVE_OFFSET, TURN_PARALLEL, PASS_OBSTACLE, TURN_IN };
enum ReturnStage { SEARCH_LINE, ALIGN_HEADING };

AvoidStage avoidStage = TURN_OUT;
ReturnStage returnStage = SEARCH_LINE;
unsigned long stageStartMs = 0;
unsigned long lastSonarSampleMs = 0;
byte nextSonar = 0;
byte obstacleHits[2] = {0, 0};
byte returnLineHits = 0;
unsigned long lastReturnSampleMs = 0;

void resetObstacleDetection() {
  obstacleHits[0] = obstacleHits[1] = 0;
  nextSonar = 0;
  lastSonarSampleMs = millis() - SONAR_SAMPLE_INTERVAL_MS;
}

void stopAutonomousRun() {
  robotState = STOPPED;
  stopCar();
  resetObstacleDetection();
  Serial.println("AFSM: STOPPED (S1)");
}

void startAutonomousRun() {
  resetLineFollow();
  resetObstacleDetection();
  robotState = LINE_FOLLOW;
  Serial.println("AFSM: LINE_FOLLOW (S2)");
  followLine();
}

void startAvoidance(unsigned long nowMs) {
  robotState = AVOID_OBSTACLE;
  avoidStage = TURN_OUT;
  stageStartMs = nowMs;
  turnRight(); // Leave the original line on the right side.
  Serial.println("AFSM: AVOID_OBSTACLE");
}

void stopOnReturnFailure() {
  robotState = FAULT;
  stopCar();
  Serial.println("AFSM: FAULT - line not found; press S2 to retry");
}

void updateObstacleDetection(unsigned long nowMs) {
  if (nowMs - lastSonarSampleMs < SONAR_SAMPLE_INTERVAL_MS) return;
  lastSonarSampleMs = nowMs;

  const byte sensor = nextSonar;
  nextSonar = 1 - nextSonar;
  const float distance = sensor == 0
      ? getDistance(TRIG_PIN_1, ECHO_PIN_1)
      : getDistance(TRIG_PIN_2, ECHO_PIN_2);

  // Invalid/no-echo readings cannot count as an obstacle.
  if (isObstacle(distance)) {
    if (obstacleHits[sensor] < OBSTACLE_CONFIRM_SAMPLES) obstacleHits[sensor]++;
    if (obstacleHits[sensor] >= OBSTACLE_CONFIRM_SAMPLES) startAvoidance(millis());
  } else {
    obstacleHits[sensor] = 0;
  }
}

void updateAvoidance(unsigned long nowMs) {
  switch (avoidStage) {
    case TURN_OUT:
      if (nowMs - stageStartMs < TURN_90_TIME) return;
      avoidStage = MOVE_OFFSET;
      goForward();
      break;
    case MOVE_OFFSET:
      if (nowMs - stageStartMs < AVOID_OFFSET_MS) return;
      avoidStage = TURN_PARALLEL;
      turnLeft();
      break;
    case TURN_PARALLEL:
      if (nowMs - stageStartMs < TURN_90_TIME) return;
      avoidStage = PASS_OBSTACLE;
      goForward();
      break;
    case PASS_OBSTACLE:
      if (nowMs - stageStartMs < AVOID_PASS_MS) return;
      avoidStage = TURN_IN;
      turnLeft();
      break;
    case TURN_IN:
      if (nowMs - stageStartMs < TURN_90_TIME) return;
      robotState = RETURN_TO_LINE;
      returnStage = SEARCH_LINE;
      returnLineHits = 0;
      lastReturnSampleMs = nowMs - LINE_CONTROL_INTERVAL_MS;
      goForward();
      Serial.println("AFSM: RETURN_TO_LINE");
      break;
  }
  stageStartMs = nowMs;
}

bool lineSensorSeesBlack() {
  const float left = normalizeLineReading(analogRead(LINE_LEFT_PIN), LINE_LEFT_WHITE_VALUE, LINE_LEFT_DARK_VALUE);
  const float right = normalizeLineReading(analogRead(LINE_RIGHT_PIN), LINE_RIGHT_WHITE_VALUE, LINE_RIGHT_DARK_VALUE);
  return left >= LINE_REACQUIRE_THRESHOLD || right >= LINE_REACQUIRE_THRESHOLD;
}

void updateReturnToLine(unsigned long nowMs) {
  if (returnStage == SEARCH_LINE) {
    if (nowMs - stageStartMs >= RETURN_SEARCH_TIMEOUT_MS) {
      stopOnReturnFailure();
      return;
    }
    if (nowMs - lastReturnSampleMs < LINE_CONTROL_INTERVAL_MS) return;
    lastReturnSampleMs = nowMs;
    if (!lineSensorSeesBlack()) {
      if (returnLineHits) goForward();
      returnLineHits = 0;
      return;
    }
    if (++returnLineHits < RETURN_LINE_CONFIRM_SAMPLES) {
      stopCar(); // Hold position while confirming the line.
      return;
    }
    returnStage = ALIGN_HEADING;
    stageStartMs = nowMs;
    turnRight(); // Restore the original forward heading.
    return;
  }

  if (nowMs - stageStartMs < TURN_90_TIME) return;
  resetLineFollow();
  resetObstacleDetection();
  robotState = LINE_FOLLOW;
  Serial.println("AFSM: LINE_FOLLOW (resumed)");
  followLine();
}

void updateRobotAfsm() {
  const unsigned long nowMs = millis();
  switch (robotState) {
    case LINE_FOLLOW:
      followLine();
      updateObstacleDetection(nowMs);
      break;
    case AVOID_OBSTACLE:
      updateAvoidance(nowMs);
      break;
    case RETURN_TO_LINE:
      updateReturnToLine(nowMs);
      break;
    case STOPPED:
    case FAULT:
      break;
  }
}

#endif
