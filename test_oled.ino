// testing OLED display (SSD1306)
// checks I2C connection and basic drawing functions

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_W 128
#define SCREEN_H 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);
  Wire.begin(4, 5);  // SDA=D2, SCL=D1 on ESP8266
  
  Serial.println("testing OLED...");
  
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found! Check wiring");
    Serial.println("- VCC -> 3.3V");
    Serial.println("- GND -> GND");
    Serial.println("- SDA -> D2 (GPIO4)");
    Serial.println("- SCL -> D1 (GPIO5)");
    while (1);
  }
  
  Serial.println("OLED found OK");
  
  // test 1 - basic text
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Test 1: text");
  display.println("Hello World");
  display.display();
  Serial.println("Test 1 done - basic text");
  delay(2000);
  
  // test 2 - large text
  display.clearDisplay();
  display.setTextSize(3);
  display.setCursor(0, 10);
  display.println("BIG");
  display.display();
  Serial.println("Test 2 done - large text");
  delay(2000);
  
  // test 3 - draw line
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Test 3: lines");
  display.drawLine(0, 20, 128, 20, SSD1306_WHITE);
  display.drawLine(0, 40, 128, 40, SSD1306_WHITE);
  display.display();
  Serial.println("Test 3 done - lines");
  delay(2000);
  
  // test 4 - draw rectangle
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Test 4: shapes");
  display.drawRect(10, 15, 50, 30, SSD1306_WHITE);
  display.fillRect(70, 15, 50, 30, SSD1306_WHITE);
  display.display();
  Serial.println("Test 4 done - shapes");
  delay(2000);
  
  // test 5 - all pixels on
  display.clearDisplay();
  display.fillScreen(SSD1306_WHITE);
  display.display();
  Serial.println("Test 5 done - all pixels on");
  delay(1000);
  
  // test 6 - all pixels off
  display.clearDisplay();
  display.display();
  Serial.println("Test 6 done - all pixels off");
  delay(1000);
  
  Serial.println("all tests passed! OLED working fine");
}

void loop() {
  // show a simple counter to confirm display keeps working
  static int count = 0;
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("OLED OK");
  display.setTextSize(3);
  display.setCursor(20, 20);
  display.println(count);
  display.display();
  
  count++;
  delay(1000);
}
