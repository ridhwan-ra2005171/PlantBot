// testing WiFi connection and Telegram bot
// sends a test message to confirm everything works

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// credentials in separate file
#include "credentials.h"

WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

void setup() {
  Serial.begin(115200);
  Serial.println("\nWiFi + Telegram test");
  
  // connect wifi
  Serial.print("connecting to: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFAILED to connect to WiFi");
    Serial.println("check SSID and password in credentials.h");
    return;
  }
  
  Serial.println("\nWiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Signal: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  
  client.setInsecure();
  
  // send test message
  Serial.println("\nsending test message to Telegram...");
  bool sent = bot.sendMessage(chatID, "test message from ESP8266 - WiFi and Telegram working!", "");
  
  if (sent) {
    Serial.println("message sent OK!");
    Serial.println("check your Telegram bot");
  } else {
    Serial.println("message failed");
    Serial.println("check bot token and chat ID in credentials.h");
  }
}

void loop() {
  // check for incoming messages
  static unsigned long lastCheck = 0;
  
  if (millis() - lastCheck > 2000) {
    int msgs = bot.getUpdates(bot.last_message_received + 1);
    
    if (msgs > 0) {
      Serial.print("got ");
      Serial.print(msgs);
      Serial.println(" message(s):");
      
      for (int i = 0; i < msgs; i++) {
        Serial.print("  from: ");
        Serial.print(bot.messages[i].from_name);
        Serial.print(" | text: ");
        Serial.println(bot.messages[i].text);
        
        // echo back
        bot.sendMessage(bot.messages[i].chat_id, "received: " + bot.messages[i].text, "");
      }
    }
    
    lastCheck = millis();
  }
}
