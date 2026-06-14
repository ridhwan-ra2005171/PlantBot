// DHT Auto-Detect Test - Tries both DHT11 and DHT22

#include <DHT.h>

#define DHT_PIN 2  // D4

DHT dht11(DHT_PIN, DHT11);
DHT dht22(DHT_PIN, DHT22);

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== DHT Auto-Detect Test ===\n");
  
  pinMode(DHT_PIN, INPUT_PULLUP);
  
  dht11.begin();
  delay(2000);
  
  Serial.println("Testing both DHT11 and DHT22...\n");
}

void loop() {
  Serial.println("--- Reading as DHT11 ---");
  float temp11 = dht11.readTemperature();
  float hum11 = dht11.readHumidity();
  
  if (!isnan(temp11)) {
    Serial.print("DHT11 Temp: ");
    Serial.print(temp11);
    Serial.println(" °C");
    Serial.print("DHT11 Humidity: ");
    Serial.print(hum11);
    Serial.println(" %");
  } else {
    Serial.println("DHT11 FAILED");
  }
  
  delay(500);
  
  Serial.println("\n--- Reading as DHT22 ---");
  float temp22 = dht22.readTemperature();
  float hum22 = dht22.readHumidity();
  
  if (!isnan(temp22)) {
    Serial.print("DHT22 Temp: ");
    Serial.print(temp22);
    Serial.println(" °C");
    Serial.print("DHT22 Humidity: ");
    Serial.print(hum22);
    Serial.println(" %");
  } else {
    Serial.println("DHT22 FAILED");
  }
  
  Serial.println("\n=============================");
  
  // Determine which one looks correct
  if (!isnan(temp11) && temp11 > 10 && temp11 < 40) {
    Serial.println(" DHT11 looks correct!");
  }
  if (!isnan(temp22) && temp22 > 10 && temp22 < 40) {
    Serial.println(" DHT22 looks correct!");
  }
  
  Serial.println("=============================\n");
  
  delay(3000);
}
