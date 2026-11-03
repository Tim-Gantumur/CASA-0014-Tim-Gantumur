void setup() {
  Serial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  int squeeze = analogRead(A0);    // reads the sensor (0 to about 1000)

  int brightness = 0;              // start with the light off
  if (squeeze > 20) {              // ignores the tiny background wobble
    brightness = map(squeeze, 20, 1000, 0, 255);  // turns squeeze into brightness
  }
  brightness = constrain(brightness, 0, 255);     // keeps it in a safe range (not below 0 or above 100)

  analogWrite(LED_BUILTIN, brightness);  // sets the light's brightness
  Serial.println(squeeze);               // still show the graph for the plotter

  delay(20);
}