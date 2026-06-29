// Plant Monitoring System
// Made for Interactive Systems project
// Uses ESP8266 with various sensors

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ESP8266HTTPClient.h>
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

// moisture calibration values - tested ridhwan
#define DRY_VAL 634
#define WET_VAL 401

// temp sensor setup - had to calibrate because readings were off
#define DHT_TYPE DHT11
DHT dht(DHT_PIN, DHT_TYPE);
#define TEMP_OFFSET 20.0
#define HUMIDITY_OFFSET 35.0

// credentials (not pushed to GitHub)
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
String plantName = "My Salad";
bool displayActive = false;
int proximityDist = 60; // in cm

// sensor readings
int moisture = 0;
float temp = 0;
float humidity = 0;

// history tracking — one reading every 10 min = ~3.5 days of data
#define HIST_SIZE 432           // 3 days at 10 min intervals
#define LOG_INTERVAL 600000UL   // 10 minutes in ms
const char* graphServer = "https://plantbot.hidenfree.com/generate_graph";

uint8_t moistureHist[HIST_SIZE];
float tempHist[HIST_SIZE];
float humidityHist[HIST_SIZE];

int histIdx = 0;
bool histFull = false;
unsigned long lastLog = 0;

void requestGraph(String chatId) {

  WiFiClientSecure graphClient;
  graphClient.setInsecure();

  HTTPClient http;

  if (!http.begin(graphClient, graphServer)) {
    return;
  }

  http.addHeader("Content-Type", "application/json");

  int count = histFull ? HIST_SIZE : histIdx;

  if (count == 0) {
    bot.sendMessage(chatId, "No history yet, try again later", "");
    http.end();
    return;
  }

  String payload = "{";
  payload += "\"chat_id\":\"" + chatId + "\",";
  payload += "\"plant\":\"" + plantName + "\",";

  // moisture array
  payload += "\"moisture\":[";
  for (int i = 0; i < count; i++) {
    payload += String((int)moistureHist[i]);
    if (i < count - 1) payload += ",";
  }
  payload += "],";

  // temperature array
  payload += "\"temperature\":[";
  for (int i = 0; i < count; i++) {
    payload += String((int)tempHist[i]);
    if (i < count - 1) payload += ",";
  }
  payload += "],";

  // humidity array
  payload += "\"humidity\":[";
  for (int i = 0; i < count; i++) {
    payload += String(humidityHist[i], 1);
    if (i < count - 1) payload += ",";
  }
  payload += "]}";

  int code = http.POST(payload);

  Serial.print("Graph request status: ");
  Serial.println(code);

  http.end();
}

// Generate 4 dummy data points with constant values
void generateDummyData() {
  Serial.println("Loading 4 constant dummy data points...");
  for (int i = 0; i < 4; i++) {
    moistureHist[i] = 60;
    tempHist[i]     = 24.2;
    humidityHist[i] = 55.8;
  }
  
  // Set index to 4 so the next real reading is saved at index 4
  histIdx = 4;
  histFull = false;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(4, 5);

  // init display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Display init failed");
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Plant Monitor");
  display.println("Starting up...");
  display.display();

  // Populate history arrays with our 4 constant dummy points so that we have some data to graph
  generateDummyData();

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
  delay(2000); // wait for sensor to stabilize

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

  if (WiFi.status() == WL_CONNECTED) {
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

// read distance from ultrasonic (median of 5 readings to reduce jumpiness)
int getDistance() {
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
    if (dist == 0) dist = 999; // timeout = nothing in range
    readings[i] = dist;
    delay(10); // small gap so the previous echo settles
  }

  // sort readings (bubble sort, only 5 values)
  for (int i = 0; i < samples - 1; i++) {
    for (int j = i + 1; j < samples; j++) {
      if (readings[j] < readings[i]) {
        int tmp = readings[i];
        readings[i] = readings[j];
        readings[j] = tmp;
      }
    }
  }

  return readings[samples / 2]; // middle value = median
}

// read all sensors
void readSensors() {
  // moisture
  int raw = analogRead(MOISTURE_PIN);
  moisture = map(raw, WET_VAL, DRY_VAL, 100, 0);
  moisture = constrain(moisture, 0, 100);

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

// draw face on right side of OLED based on moisture level
void drawFace() {
  int fx = 96;
  int fy = 40;
  int fr = 18;

  display.drawCircle(fx, fy, fr, SSD1306_WHITE);
  display.fillCircle(fx - 6, fy - 5, 2, SSD1306_WHITE); // left eye
  display.fillCircle(fx + 6, fy - 5, 2, SSD1306_WHITE); // right eye

  if (moisture >= 50) {
    // HAPPY - parabola curving upward (y decreases toward edges)
    for (int i = -6; i <= 6; i++) {
      int sx = fx + i;
      int sy = fy + 10 - (i * i) / 9;  // curves up = smile
      display.drawPixel(sx, sy, SSD1306_WHITE);
      display.drawPixel(sx, sy + 1, SSD1306_WHITE);
    }
  } else if (moisture >= 30) {
    // NEUTRAL - flat line
    display.drawLine(fx - 6, fy + 7, fx + 6, fy + 7, SSD1306_WHITE);
  } else {
    // SAD - parabola curving downward (y increases toward edges)
    for (int i = -6; i <= 6; i++) {
      int sx = fx + i;
      int sy = fy + 6 + (i * i) / 9;  // curves down = frown
      display.drawPixel(sx, sy, SSD1306_WHITE);
      display.drawPixel(sx, sy - 1, SSD1306_WHITE);
    }
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

  // plant name + divider
  display.setCursor(0, 0);
  display.println(plantName);
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // moisture reading (big, left side)
  display.setTextSize(2);
  display.setCursor(0, 13);
  display.print("M:");
  display.print(moisture);
  display.println("%");

  // temp and humidity (small, left side)
  display.setTextSize(1);
  display.setCursor(0, 35);
  if (temp > 0) {
    display.print("T:");
    display.print(temp, 1);
    display.println("C");
  }
  display.setCursor(0, 45);
  if (humidity > 0) {
    display.print("H:");
    display.print(humidity, 1);
    display.println("%");
  }

  // status text bottom left
  display.setCursor(0, 56);
  if (moisture < 20) {
    display.print("VERY DRY");
  } else if (moisture < 40) {
    display.print("DRY");
  } else if (moisture < 70) {
    display.print("OK");
  } else {
    display.print("WET");
  }

  // wifi indicator top right
  display.setCursor(100, 0);
  if (WiFi.status() == WL_CONNECTED) {
    display.print("Wifi");
  }

  // draw face on right side
  drawFace();

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

  // auto off after 20 seconds
  if (displayActive && (millis() - displayTimer > 20000)) {
    Serial.println("Display timeout");
    displayActive = false; // set true if no HC-SR04 connected
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
      msg += "/graph - show a 3-day data chart\n";
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
      int count = histFull ? HIST_SIZE : histIdx;
      if (count == 0) {
        msg += "No data yet";
      } else {
        int start = histFull ? histIdx : 0;
        // show last 10 entries max to keep message short
        int show = min(count, 10);
        int offset = count - show;
        for (int j = 0; j < show; j++) {
          int idx = (start + offset + j) % HIST_SIZE;
          msg += String(show - j) + ". " + String((int)moistureHist[idx]) + "%\n";
        }
        // calc average over all stored readings
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
    else if (text == "/graph") {
      bot.sendMessage(chat, "Generating graph, please wait...", "");
      requestGraph(chat);
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

  // log to history on interval, not every loop
  if (millis() - lastLog >= LOG_INTERVAL) {
    moistureHist[histIdx] = (uint8_t)moisture;
    tempHist[histIdx]     = temp; 
    humidityHist[histIdx] = humidity;
    
    histIdx++;
    if (histIdx >= HIST_SIZE) {
      histIdx = 0;
      histFull = true;
    }
    lastLog = millis();
    Serial.println("History saved");
  }

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