// Plant Monitoring System
// Made for Interactive Systems project
// Uses ESP8266 with various sensors

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <DHT.h>

// display setup
#define SCREEN_W 128
#define SCREEN_H 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, OLED_RESET);

// sensor pins
#define MOISTURE_PIN A0
#define DHT_PIN 2  
#define TRIG_PIN 12
#define ECHO_PIN 16

// LED outputs
#define RED_LED 13
#define YELLOW_LED 15
#define GREEN_LED 14

// moisture calibration values (found through testing)
#define DRY_VAL 634
#define WET_VAL 380

// temp sensor setup - had to calibrate because readings were off
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);
#define TEMP_OFFSET 20.0
#define HUMIDITY_OFFSET 35.0

// credentials stored in separate file (not pushed to GitHub)
#include "credentials.h"

WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

// timing variables
unsigned long lastBotCheck;
unsigned long lastAlert = 0;
unsigned long lastWatered = 0;
unsigned long displayTimer = 0;

// settings
int alertThreshold = 30;
String plantName = "My Plant";
bool displayActive = false;
int proximityDist = 60; //this is in cm

// sensor readings
int moisture = 0;
float temp = 0;
float humidity = 0;

// history tracking
int moistureHist[10];
int histIdx = 0;
bool histFull = false;

void setup() {
  Serial.begin(115200);
  Wire.begin(4, 5);
  
  // init display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Display init failed");
    while(1);
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Plant Monitor");
  display.println("Starting up...");
  display.display();
  
  // setup pins
  pinMode(MOISTURE_PIN, INPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // turn off LEDs
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  
  // init DHT sensor
  pinMode(DHT_PIN, INPUT_PULLUP);
  dht.begin();
  delay(2000);  // wait for sensor to stabilize
  
  // test DHT
  float testTemp = dht.readTemperature();
  if (isnan(testTemp)) {
    Serial.println("DHT not responding");
    display.setCursor(0, 30);
    display.println("Temp sensor error");
    display.display();
  } else {
    Serial.print("DHT working, temp: ");
    Serial.println(testTemp);
  }
  
  // connect wifi
  Serial.print("Connecting to WiFi");
  display.setCursor(0, 20);
  display.println("WiFi connecting...");
  display.display();
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected!");
    display.println("WiFi OK");
    display.display();
  }
  
  client.setInsecure();
  
  // send startup msg to telegram
  bot.sendMessage(chatID, "Plant monitor online! Type /help for commands", "");
  Serial.println("Telegram msg sent");
  
  delay(2000);
  displayActive = true;
  displayTimer = millis();
}

// reading distance from ultrasonic
int getDistance() { //ultrasonic sensor sometimes return a junk value, so we take the median
  const int samples = 5;
  int readings[samples];

  for (int i = 0; i < samples; i++) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    int dist = duration * 0.034 / 2;
    if (dist == 0) dist = 999;   // timeout = nothing in range
    readings[i] = dist;
    delay(10);                   // small gap so the previous echo settles
  }

  // sort the readings (simple bubble sort, only 5 values)
  for (int i = 0; i < samples - 1; i++) {
    for (int j = i + 1; j < samples; j++) {
      if (readings[j] < readings[i]) {
        int tmp = readings[i];
        readings[i] = readings[j];
        readings[j] = tmp;
      }
    }
  }

  return readings[samples / 2];  // the middle value = median
}

// read all sensors
void readSensors() {
  // moisture
  int raw = analogRead(MOISTURE_PIN);
  moisture = map(raw, WET_VAL, DRY_VAL, 100, 0);
  moisture = constrain(moisture, 0, 100);
  
  // save to history
  moistureHist[histIdx] = moisture;
  histIdx++;
  if (histIdx >= 10) {
    histIdx = 0;
    histFull = true;
  }
  
  // temp and humidity
  float rawT = dht.readTemperature();
  float rawH = dht.readHumidity();
  
  // retry if failed
  if (isnan(rawT) || isnan(rawH)) {
    delay(100);
    rawT = dht.readTemperature();
    rawH = dht.readHumidity();
  }
  
  // apply calibration
  if (!isnan(rawT)) {
    temp = rawT + TEMP_OFFSET;
    Serial.print("Temp raw: ");
    Serial.print(rawT, 1);
    Serial.print("C -> ");
    Serial.print(temp, 1);
    Serial.print("C | ");
  } else {
    temp = 0;
    Serial.print("Temp failed | ");
  }
  
  if (!isnan(rawH)) {
    humidity = rawH + HUMIDITY_OFFSET;
    humidity = constrain(humidity, 0, 100);
    Serial.print("Hum raw: ");
    Serial.print(rawH, 1);
    Serial.print("% -> ");
    Serial.print(humidity, 1);
    Serial.println("%");
  } else {
    humidity = 0;
    Serial.println("Humidity failed");
  }
  
  Serial.print("Moisture: ");
  Serial.print(moisture);
  Serial.print("% | Temp: ");
  Serial.print(temp);
  Serial.print("C | Hum: ");
  Serial.print(humidity);
  Serial.println("%");
}

// control LEDs based on moisture
void updateLEDs() {
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  
  if (moisture < 20) {
    digitalWrite(RED_LED, HIGH);
  } else if (moisture < 40) {
    digitalWrite(YELLOW_LED, HIGH);
  } else {
    digitalWrite(GREEN_LED, HIGH);
  }
}

// update OLED display
void updateDisplay() {
  if (!displayActive) {
    display.clearDisplay();
    display.display();
    return;
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(0, 0);
  display.println(plantName);
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  
  // show moisture big
  display.setTextSize(2);
  display.setCursor(0, 15);
  display.print("M:");
  display.print(moisture);
  display.println("%");
  
  // temp and humidity
  display.setTextSize(1);
  display.setCursor(0, 35);
  if (temp > 0) {
    display.print("T: ");
    display.print(temp, 1);
    display.println("C");
  }
  
  display.setCursor(0, 45);
  if (humidity > 0) {
    display.print("H: ");
    display.print(humidity, 1);
    display.println("%");
  }
  
  // status
  display.setCursor(0, 55);
  if (moisture < 20) {
    display.print("VERY DRY");
  } else if (moisture < 40) {
    display.print("DRY");
  } else if (moisture < 70) {
    display.print("OK");
  } else {
    display.print("WET");
  }
  
  // wifi indicator
  display.setCursor(110, 0);
  if (WiFi.status() == WL_CONNECTED) {
    display.print("W");
  }
  
  display.display();
}

// check proximity and turn display on/off
void checkProximity() {
  int dist = getDistance();
  
  Serial.print("Distance: ");
  Serial.print(dist);
  Serial.println("cm");
  
  // turn on if someone nearby
  if (dist < proximityDist && dist > 0) {
    if (!displayActive) {
      Serial.println("Person detected");
    }
    displayActive = true;
    displayTimer = millis();
  }
  
  // auto off after timeout
  if (displayActive && (millis() - displayTimer > 20000)) {
    Serial.println("Display timeout");
    displayActive = false; //set true if, theres no HC-SR04 ultrasonic
  }
}

// send alert if moisture low
void checkAlert() {
  if (moisture < alertThreshold && (millis() - lastAlert > 300000)) {
    String msg = "Alert! ";
    msg += plantName;
    msg += " needs water\n\n";
    msg += "Moisture: ";
    msg += String(moisture);
    msg += "%\n";
    if (temp > 0) {
      msg += "Temp: ";
      msg += String(temp, 1);
      msg += "C\n";
    }
    if (humidity > 0) {
      msg += "Humidity: ";
      msg += String(humidity, 1);
      msg += "%\n";
    }
    
    bot.sendMessage(chatID, msg, "");
    lastAlert = millis();
    Serial.println("Alert sent");
  }
}

// handle telegram commands
void handleMessages(int numMsgs) {
  for (int i = 0; i < numMsgs; i++) {
    String chat = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    
    Serial.println("Got: " + text);
    
    if (text == "/start") {
      String msg = "Plant Monitor Bot\n\n";
      msg += "Monitoring: " + plantName + "\n\n";
      msg += "Commands:\n/status /help";
      bot.sendMessage(chat, msg, "");
    }
    else if (text == "/status") {
      String msg = plantName + " Status:\n\n";
      msg += "Moisture: " + String(moisture) + "%\n";
      if (temp > 0) {
        msg += "Temp: " + String(temp, 1) + "C\n";
      }
      if (humidity > 0) {
        msg += "Humidity: " + String(humidity, 1) + "%\n";
      }
      msg += "Alert level: " + String(alertThreshold) + "%\n\n";
      
      if (moisture < 20) {
        msg += "Status: VERY DRY\nWater now!";
      } else if (moisture < 40) {
        msg += "Status: DRY\nWater soon";
      } else if (moisture < 70) {
        msg += "Status: GOOD\nPlant happy";
      } else {
        msg += "Status: WET\nDont water";
      }
      
      if (lastWatered > 0) {
        unsigned long hrs = (millis() - lastWatered) / 3600000;
        msg += "\n\nLast watered: " + String(hrs) + "h ago";
      }
      
      bot.sendMessage(chat, msg, "");
    }
    else if (text == "/help") {
      String msg = "Commands:\n\n";
      msg += "Monitoring:\n";
      msg += "/status - current readings\n";
      msg += "/history - past readings\n";
      msg += "/environment - temp & humidity\n\n";
      msg += "Watering:\n";
      msg += "/water - mark as watered\n";
      msg += "/lastwatered - time since watering\n\n";
      msg += "Settings:\n";
      msg += "/setname <name>\n";
      msg += "/setalert <value>\n";
      msg += "/displayon - keep display on\n";
      bot.sendMessage(chat, msg, "");
    }
    else if (text == "/environment") {
      String msg = "Environment:\n\n";
      if (temp > 0) {
        msg += "Temp: " + String(temp, 1) + "C\n";
        if (temp < 15) msg += "Too cold\n";
        else if (temp > 30) msg += "Too hot\n";
        else msg += "Good\n";
      }
      if (humidity > 0) {
        msg += "\nHumidity: " + String(humidity, 1) + "%\n";
        if (humidity < 30) msg += "Too dry\n";
        else if (humidity > 70) msg += "Too humid\n";
        else msg += "Good\n";
      }
      msg += "\nMoisture: " + String(moisture) + "%";
      bot.sendMessage(chat, msg, "");
    }
    else if (text == "/water") {
      lastWatered = millis();
      lastAlert = millis();
      String msg = plantName + " watered!\n\n";
      msg += "Moisture: " + String(moisture) + "%\n";
      msg += "Will remind if it gets dry";
      bot.sendMessage(chat, msg, "");
    }
    else if (text == "/lastwatered") {
      if (lastWatered == 0) {
        bot.sendMessage(chat, "No watering recorded\n\nUse /water after watering", "");
      } else {
        unsigned long hrs = (millis() - lastWatered) / 3600000;
        unsigned long mins = ((millis() - lastWatered) % 3600000) / 60000;
        String msg = "Last watered:\n\n";
        msg += String(hrs) + "h " + String(mins) + "m ago\n\n";
        msg += "Moisture: " + String(moisture) + "%";
        bot.sendMessage(chat, msg, "");
      }
    }
    else if (text == "/history") {
      String msg = "Moisture History:\n\n";
      int count = histFull ? 10 : histIdx;
      if (count == 0) {
        msg += "No data yet";
      } else {
        int start = histFull ? histIdx : 0;
        for (int j = 0; j < count; j++) {
          int idx = (start + j) % 10;
          msg += String(count - j) + ". " + String(moistureHist[idx]) + "%\n";
        }
        // calc average
        int sum = 0;
        for (int j = 0; j < count; j++) {
          sum += moistureHist[j];
        }
        int avg = sum / count;
        msg += "\nAverage: " + String(avg) + "%";
      }
      bot.sendMessage(chat, msg, "");
    }
    else if (text == "/displayon") {
      displayActive = true;
      displayTimer = millis() + 3600000;
      bot.sendMessage(chat, "Display on for 1 hour", "");
    }
    else if (text.startsWith("/setname ")) {
      plantName = text.substring(9);
      plantName.trim();
      if (plantName.length() > 0) {
        bot.sendMessage(chat, "Name set to: " + plantName, "");
      } else {
        bot.sendMessage(chat, "Please provide name\n\nExample: /setname Basil", "");
      }
    }
    else if (text.startsWith("/setalert ")) {
      int val = text.substring(10).toInt();
      if (val > 0 && val <= 100) {
        alertThreshold = val;
        String msg = "Alert set to " + String(alertThreshold) + "%";
        bot.sendMessage(chat, msg, "");
      } else {
        bot.sendMessage(chat, "Invalid value\n\nExample: /setalert 25", "");
      }
    }
    else {
      bot.sendMessage(chat, "Unknown command\nTry /help", "");
    }
  }
}

void loop() {
  readSensors();
  updateLEDs();
  checkProximity();
  updateDisplay();
  checkAlert();
  
  // check telegram
  if (millis() - lastBotCheck > 1000) {
    int numNew = bot.getUpdates(bot.last_message_received + 1);
    
    if (numNew) {
      handleMessages(numNew);
    }
    
    lastBotCheck = millis();
  }
  
  delay(500);
}
