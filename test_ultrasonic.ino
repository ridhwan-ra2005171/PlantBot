// testing HC-SR04 ultrasonic sensor
// prints distance to serial monitor every 500ms

#define TRIG_PIN 12  // D6
#define ECHO_PIN 16  // D0

void setup() {
  Serial.begin(115200);
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  Serial.println("HC-SR04 ultrasonic test");
  Serial.println("wiring: TRIG=D6, ECHO=D0, VCC=5V, GND=GND");
  Serial.println("---");
  delay(500);
}

int readDistance() {
  // send pulse
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // read echo with 30ms timeout
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  
  // no echo = nothing in range
  if (duration == 0) return -1;
  
  // convert to cm
  int dist = duration * 0.034 / 2;
  return dist;
}

void loop() {
  int dist = readDistance();
  
  if (dist == -1) {
    Serial.println("no echo - nothing detected or out of range");
  } else if (dist > 400) {
    Serial.println("out of range (>400cm)");
  } else {
    Serial.print("distance: ");
    Serial.print(dist);
    Serial.print(" cm");
    
    // give some context
    if (dist < 10) {
      Serial.print("  (very close)");
    } else if (dist < 30) {
      Serial.print("  (close - would trigger display)");
    } else if (dist < 100) {
      Serial.print("  (medium range)");
    } else {
      Serial.print("  (far)");
    }
    
    Serial.println();
  }
  
  delay(500);
}
