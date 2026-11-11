#ifndef DISTANCE_SENSORS_H
#define DISTANCE_SENSORS_H

float getDistance(byte trigPin, byte echoPin) {
  if (digitalRead(echoPin) == HIGH) return -1;

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 12000);
  return duration ? duration * 0.0343 / 2.0 : -1;
}

bool isObstacle(float distance) {
  return distance > 0 && distance <= SAFE_DISTANCE;
}

#endif
