// Sensor test - first test of the force sensitive resistor (FSR)
// Reads the sensor on pin A0 ten times a second and prints the value,
// so it can be watched in the Arduino Serial Plotter (9600 baud).
// Results: about 2-3 at rest, and up to about 900 when squeezed hard.

void setup() {
  Serial.begin(9600);
}

void loop() {
  int squeeze = analogRead(A0);
  Serial.println(squeeze);
  delay(100);
}
