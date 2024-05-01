// Loading Libraries
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <HTTPClient.h>
#include <EEPROM.h>
#include <ESPTelnet.h>
#include <ArduinoOTA.h>

//test
//Version number
#define SOFTWARE_VERSION "1.0.4"

// Globale variabelen voor de knop
unsigned long firstPressTime = 0;
int pressCount = 0;
const int pressThreshold = 5;
const unsigned long timeThreshold = 3000;  // 3000 milliseconden = 3 seconden

// Setup storage
struct settings {
  char ssid[30];
  char password[30];
  char url[256];
  unsigned int machineId; // Add machine ID field
} memory = {};

// Setup Server for Wifi Page
ESP8266WebServer server(80);
ESPTelnet telnet;
IPAddress ip;
uint16_t  port = 23;

// Setup Hardware peripherals
#define BUTTON_PIN D5
#define LED D8


// Auxiliary variables to store the current output state
String LEDState = "off";
HTTPClient http;
WiFiClient wifiClient;

void ledFeedback(bool success) {
  unsigned long startTime = millis(); // Get the current time

  if (success) {
    Serial.println("Flash SUCCESS");
  } else {
    Serial.println("Flash FAILED");
  }

  // Blink the LED for 5 seconds
  while (millis() - startTime < 5000) { // Run for 5000ms (5 seconds)
    if (success) {
       
      // Blink the LED quickly twice for success
      for (unsigned int i = 0; i < 2; i++) {
        digitalWrite(LED, HIGH);
        delay(100);
        digitalWrite(LED, LOW);
        delay(100);
      }
    } else {
      // Blink the LED slowly five times for failure
      
      for (unsigned int i = 0; i < 5; i++) {
        digitalWrite(LED, HIGH);
        delay(500);
        digitalWrite(LED, LOW);
        delay(500);
      }
    }
  }
  // Reset the LED after the loop
  digitalWrite(LED, HIGH);
}

bool isNullTerminated(const char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        // Iterate until null terminator is found
    }
    // If the loop reached here, it means a null terminator was found
    return true;
}
//LED knipperen op Flash commando - Koen
void flashLED(int duration, int blinkRate) {
  bool initialLEDState = digitalRead(LED);  // Opslaan van de huidige LED-status

  unsigned long startTime = millis();
  while (millis() - startTime < duration) {
    digitalWrite(LED, !initialLEDState);  // Zet de LED aan/uit tegenovergesteld van de beginstand
    delay(blinkRate);
    digitalWrite(LED, initialLEDState);  // Zet de LED terug naar de beginstand
    delay(blinkRate);
  }

  digitalWrite(LED, initialLEDState);  // Herstel de originele LED-status na het knipperen
}

//Mac adres ophalen
String getMacAddress() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(macStr);
}

//Define a function to calculate uptime: - Koen
String formatUptime() {
  unsigned long seconds = millis() / 1000;
  unsigned long days = seconds / 86400;
  seconds %= 86400;
  unsigned long hours = seconds / 3600;
  seconds %= 3600;
  unsigned long minutes = seconds / 60;
  seconds %= 60;
  
  String uptime = String(days) + "d " + String(hours) + "h " + String(minutes) + "m " + String(seconds) + "s";
  return uptime;
}

/* ------------------------------------------------- */
// (optional) callback functions for telnet events
void onTelnetConnect(String ip) {
  Serial.print("- Telnet: ");
  Serial.print(ip);
  Serial.println(" connected");
  
  //telnet.println("\nWelkom " + telnet.getIP());
  telnet.print("Je bent verbonden met Machine: ");
  telnet.println("Versie: " + String(SOFTWARE_VERSION));
  telnet.println("MAC Adres: " + getMacAddress());
  telnet.println(memory.machineId);
  telnet.println(" - Type exit to disconnect.");
  telnet.println(" - Type info to view machine info.");
  telnet.println(" - Type reboot to reboot the machine.");
  telnet.println(" - Type uptime to view device uptime.");
  telnet.println(" - Type LED to flash the LED.");
}

void onTelnetDisconnect(String ip) {
  Serial.print("- Telnet: ");
  Serial.print(ip);
  Serial.println(" disconnected");
}

void onTelnetReconnect(String ip) {
  Serial.print("- Telnet: ");
  Serial.print(ip);
  Serial.println(" reconnected");
}

void onTelnetConnectionAttempt(String ip) {
  Serial.print("- Telnet: ");
  Serial.print(ip);
  Serial.println(" tried to connected");
}

void onTelnetInput(String str) {
  // checks for a certain command
  if (str == "info") {
    telnet.print("  Machine: ");
    telnet.println(memory.machineId);
    telnet.println("Versie: " + String(SOFTWARE_VERSION));
    telnet.println("MAC Adres: " + getMacAddress());
    telnet.print("  Wifi Naam: ");
    telnet.println(WiFi.SSID());
    telnet.print("  IP Address: ");
    telnet.println(WiFi.localIP());
    telnet.printf("  Wifi Stertke: %d dBm\n", WiFi.RSSI());
    telnet.println();
    telnet.print("  API URL: ");
    telnet.println(memory.url);
    
  // disconnect the client
  } else if (str == "exit") {
    telnet.println("   disconnecting you...");
    telnet.disconnectClient();
  // do a full reboot of esp device.
  } else if (str == "reboot") {
    telnet.println("   Rebooting device...");
    delay(1000); // Vertraging om de reactie te verzenden
    ESP.restart(); // Herstart het apparaat
  } else if (str == "uptime") {
    String uptime = formatUptime();
    telnet.println("Device Uptime: " + uptime);
  } else if (str == "LED","led") {
    telnet.println("Flashing LED for 15 seconds...");
    flashLED(15000, 50);  // Flash LED for 15 seconds with a blink rate of 500ms
  }  else {
    telnet.print("Unknown command: ");
    telnet.println(str);
    telnet.println(" - Type exit to disconnect.");
    telnet.println(" - Type info to view machine info.");
    telnet.println(" - Type reboot to reboot the machine.");
    telnet.println(" - Type uptime to view device uptime.");
    telnet.println(" - Type LED to flash the LED.");
  }
}

void setupTelnet() {  
  // passing on functions for various telnet events
  telnet.onConnect(onTelnetConnect);
  telnet.onConnectionAttempt(onTelnetConnectionAttempt);
  telnet.onReconnect(onTelnetReconnect);
  telnet.onDisconnect(onTelnetDisconnect);
  telnet.onInputReceived(onTelnetInput);

  Serial.print("- Telnet: ");
  if (telnet.begin(port)) {
    Serial.print("Telnet running on: ");
    Serial.print(ip);
    Serial.print(":");
    Serial.print(port);
  } else {
    Serial.println("Telnet - Error.");
  }
}

/* ------------------------------------------------- */

void registerMachine() {
  // Function to register machine input
  String URL = String(memory.url);

  // Check if memory.url is null-terminated
  if (isNullTerminated(memory.url)) {
    // Check if the URL contains "http"
    if (URL.indexOf("http") != -1) {
      //Serial Feedback
      Serial.println();
      Serial.print("Destination URL: ");
      Serial.print(URL);
      // Telnet Feedback
      telnet.print("  Destination URL: ");
      telnet.println(URL);
    } else {
      Serial.println("Error - URL does not contain 'http'");
      telnet.println("  Error - URL does not contain 'http'");
      ledFeedback(false);
      return; // Exit function early on error
    }
    
  } else {
    Serial.println("Error: The URL in EEPROM is not null-terminated.");
    return; // Exit function early on error
  }

  // Prepare JSON document
  DynamicJsonDocument doc(2048);
  doc["machine"] = memory.machineId; // Use configurable machine ID
  doc["action"] = "register";

  // Serialize JSON document
  String json;
  serializeJson(doc, json);

  Serial.println();
  Serial.print("POST DATA: ");
  Serial.println(json);

  http.begin(wifiClient, URL);
  http.POST(json);
  String payload = http.getString();
  // Check if payload contains "success"
  if (payload.indexOf("opgeslagen") != -1) {
    ledFeedback(true);
  } else {
    ledFeedback(false);
  }
  Serial.println(payload);
  telnet.print("  Respone: ");
  telnet.println(payload);
  http.end();
}

void handlePortal() {
  if (server.method() == HTTP_POST) {
    // Parse form data and store in memory
    strncpy(memory.ssid, server.arg("ssid").c_str(), sizeof(memory.ssid));
    strncpy(memory.password, server.arg("password").c_str(), sizeof(memory.password));
    strncpy(memory.url, server.arg("url").c_str(), sizeof(memory.url));

    // Ensure null termination for memory.ssid and memory.password
    memory.ssid[server.arg("ssid").length()] = '\0';
    memory.password[server.arg("password").length()] = '\0';
    memory.url[server.arg("url").length()] = '\0';

    // Parse and store machine ID
    memory.machineId = server.arg("machineId").toInt();

    // Store settings in EEPROM
    EEPROM.put(0, memory);
    EEPROM.commit();

    server.send(200, "text/html", "<!doctype html> <html lang='en'> <head> <meta charset='utf-8'> <meta name='viewport' content='width=device-width, initial-scale=1'> <title>Machine Setup</title> <style> *, ::after, ::before { box-sizing: border-box; } body { margin: 0; font-family: 'Segoe UI', Roboto, 'Helvetica Neue', Arial, 'Noto Sans', 'Liberation Sans'; font-size: 1rem; font-weight: 400; line-height: 1.5; color: #212529; background-color: #f5f5f5; } .form-control { display: block; width: 100%; height: calc(1.5em + .75rem + 2px); border: 1px solid #ced4da; } button { border: 1px solid transparent; color: #fff; background-color: #007bff; border-color: #007bff; padding: .5rem 1rem; font-size: 1.25rem; line-height: 1.5; border-radius: .3rem; width: 100% } .form-signin { width: 100%; max-width: 400px; padding: 15px; margin: auto; } h1, p { text-align: center } </style> </head> <body> <main class='form-signin'> <h1>Machine Setup</h1> <br /> <p>Your settings have been saved successfully!<br />Please restart the device.</p> <button type='button' onclick='rebootDevice()'>Reboot</button> </main> <script>function rebootDevice() { if (confirm('Are you sure you want to reboot the device?')) { fetch('/reboot-device', { method: 'POST' }).then(response => { if (!response.ok) { console.error('Error rebooting device'); } else { alert('Device rebooted successfully'); } }); } }</script> </body> </html>");
  } else {
    // Display setup form with version and MAC address information and fixed button and field spacing
    String formHtml = "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Machine Setup</title><style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);margin-bottom:10px;padding:0.375rem 0.75rem;border:1px solid #ced4da;border-radius:0.25rem;}button{cursor: pointer;border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%;margin-bottom:10px;}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}.form-floating{margin-bottom:20px;}h1{text-align: center}p{margin-top:20px;}</style> </head> <body><main class='form-signin'> <form action='/' method='post'> <h1>Machine Setup</h1><div class='form-floating'><label>SSID</label><input type='text' class='form-control' name='ssid' value='" + String(memory.ssid) + "' required></div><div class='form-floating'><label>Password</label><input type='password' class='form-control' name='password' value='" + String(memory.password) + "' required></div><div class='form-floating'><label>URL</label><input type='text' class='form-control' name='url' value='" + String(memory.url) + "' required></div><div class='form-floating'><label>Machine ID</label><input type='number' class='form-control' name='machineId' value='" + String(memory.machineId) + "' required></div><button type='submit'>Save Settings</button><button type='button' onclick='resetEEPROM()'>Reset</button><button type='button' onclick='rebootDevice()'>Reboot</button><p>Versie: " + String(SOFTWARE_VERSION) + "<br>MAC Adres: " + getMacAddress() + "</p><p style='text-align: right'><a href='https://pruim.nl' style='color: #32C5FF'>Pruim IT</a></p></form></main> <script>function resetEEPROM() { if (confirm('Are you sure you want to reset to factory defaults? This action cannot be undone.')) { fetch('/reset-eeprom', { method: 'POST' }).then(response => { if (!response.ok) { console.error('Error resetting EEPROM'); } else { alert('EEPROM reset successful'); location.reload(); } }); } } function rebootDevice() { if (confirm('Are you sure you want to reboot the device?')) { fetch('/reboot-device', { method: 'POST' }).then(response => { if (!response.ok) { console.error('Error rebooting device'); } else { alert('Device rebooted successfully'); } }); } }</script> </body></html>";
    server.send(200, "text/html", formHtml);
  }


}

void handleResetEEPROM() {
  EEPROM.begin(sizeof(struct settings));
  for (int i = 0; i < sizeof(struct settings); ++i) {
    EEPROM.write(i, 0); // Clear EEPROM content
  }
  EEPROM.commit();
  server.send(200, "text/plain", "EEPROM reset successful");
}

void handleRebootDevice() {
  server.send(200, "text/plain", "Rebooting device...");
  delay(1000); // Delay to allow response to be sent
  ESP.restart(); // Reboot the Device
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Systeem wordt opgestart.");

  // Loading Wifi Setup from EEPROM
  EEPROM.begin(sizeof(struct settings));
  EEPROM.get(0, memory);

  // Turn off LED
  digitalWrite(LED, HIGH);

  // Format the new hostname using version number and machine ID
  String hostname = "ESP_V" + String(SOFTWARE_VERSION) + "_MACHINE" + String(memory.machineId);

  // Setup OTA with the new hostname
  ArduinoOTA.setHostname(hostname.c_str());
  ArduinoOTA.begin();
  Serial.print("OTA Initialized with Hostname: ");
  Serial.println(hostname);

  byte tries = 0;

  WiFi.mode(WIFI_STA);
  WiFi.begin(memory.ssid, memory.password);

  Serial.print("Connecting To: ");
  Serial.println(memory.ssid);
  Serial.print("Wifi wordt verbonden.");

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    if (tries++ > 30) {
      String softAPSSID = "Setup Portal-V" + String(SOFTWARE_VERSION);

      WiFi.softAP(softAPSSID.c_str(), "1234567890");
      Serial.println();
      Serial.println("WiFi Failed, Switching to AP mode.");
      Serial.print("SSID: ");
      Serial.println(softAPSSID);
      Serial.print("AP IP address: ");
      Serial.println(WiFi.softAPIP());
      ip = WiFi.softAPIP();
      break;
    }
  }

  if (WiFi.getMode() == WIFI_STA) {
    Serial.println();
    Serial.print("Wifi connected, local IP: ");
    ip = WiFi.localIP();
    Serial.print(ip);
    Serial.println();
    Serial.print(WiFi.macAddress());
    Serial.println();
  }

  // Define Pinmodes.
  pinMode(LED, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Create Telnet server
  setupTelnet();

  // Create webserver handles
  server.on("/", handlePortal);
  server.on("/reset-eeprom", HTTP_POST, handleResetEEPROM);
  server.on("/reboot-device", HTTP_POST, handleRebootDevice);
  server.begin();
}


void loop() {
  // Handle OTA updates
  ArduinoOTA.handle();

  server.handleClient();

  // Check the physical button state
  int buttonState = digitalRead(BUTTON_PIN);
  
  telnet.loop();

  // send serial input to telnet as output
  if (Serial.available()) {
    telnet.print('Hallo wereld.');
  }

  // If the button is pressed, toggle the LED state
  if (buttonState == HIGH) {
    telnet.println("Button Pressed");
    Serial.println("Button Pressed");
    registerMachine(); 
  }

  //Serial.print(".");
  //delay(100);
}
