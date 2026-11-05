#ifndef BUTTONS_MODES_H
#define BUTTONS_MODES_H

void selectStopMode() {
  carMode = STOPPED;
  stopCar();
}

void selectObstacleAvoidanceMode() {
  carMode = OBSTACLE_AVOIDANCE_MODE;
  goForward();
}

void selectLineFollowingMode() {
  carMode = LINE_FOLLOWING_MODE;
  resetLineFollow();
  Serial.println("MODE: LINE_FOLLOWING (S3)");
  followLine();
}

int readButton() {
  int value = analogRead(BUTTON_PIN);
  for (byte i = 0; i < BUTTON_COUNT; i++) {
    if (value < BUTTON_LEVELS[i]) {
      return i;
    }
  }
  return -1;
}

void handleButtons() {
  int button = readButton();
  if (button == lastButton) return;
  delay(30);
  if (button != readButton()) return;
  lastButton = button;

  switch (button) {
    case 0: selectStopMode();              break;  // S1: stop
    case 1: selectObstacleAvoidanceMode(); break;  // S2: follow node
    case 2: selectLineFollowingMode();     break;  // S3: line-follow mode
  }
}

void avoidObstacle() {
  stopCar();
  delay(200);
  goBackward();
  delay(500);
  stopCar();
  delay(200);
  turnRight();
  delay(TURN_90_TIME);
  stopCar();
  delay(200);
}

#endif
