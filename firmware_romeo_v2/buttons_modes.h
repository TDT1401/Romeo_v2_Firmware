#ifndef BUTTONS_MODES_H
#define BUTTONS_MODES_H

void selectStopMode() {
  stopAutonomousRun();
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
    case 0: selectStopMode();      break; // S1: stop at any point
    case 1: startAutonomousRun();  break; // S2: line follow + obstacle avoidance
  }
}

#endif
