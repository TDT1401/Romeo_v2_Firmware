#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

void stopCar() {
  analogWrite(E1, 0);
  analogWrite(E2, 0);
}

// Converts percentage to Arduino PWM (0-255).
byte percentToPwm(byte percent) {
  percent = min(percent, (byte)100);
  return (unsigned int)percent * 255 / 100;
}

void drive(byte leftSpeedPercent, byte rightSpeedPercent,
           bool leftForward, bool rightForward) {
  digitalWrite(M1, leftForward);
  digitalWrite(M2, rightForward);
  analogWrite(E1, percentToPwm(leftSpeedPercent));
  analogWrite(E2, percentToPwm(rightSpeedPercent));
}

void goForward() {
  drive(FORWARD_SPEED_LEFT_PERCENT, FORWARD_SPEED_RIGHT_PERCENT, HIGH, HIGH);
}

void goBackward() {
  drive(BACKWARD_SPEED_LEFT_PERCENT, BACKWARD_SPEED_RIGHT_PERCENT, LOW, LOW);
}

void turnRight() {
  drive(TURN_SPEED_LEFT_PERCENT, TURN_SPEED_RIGHT_PERCENT, HIGH, LOW);
}

void turnLeft() {
  drive(TURN_SPEED_LEFT_PERCENT, TURN_SPEED_RIGHT_PERCENT, LOW, HIGH);
}

#endif
