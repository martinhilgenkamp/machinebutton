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

// Setup storage
struct settings {
  char ssid[30];
  char password[30];
  char url[256];
  int machineId; // Add machine ID field
} memory = {};

// Setup Server for Wifi Page
ESP8266WebServer server(80);

// Setup Hardware peripherals
#define BUTTON_PIN D5
#define LED D8


// Auxiliary variables to store the current output state
String LEDState = "off";
HTTPClient http;
WiFiClient wifiClient;

void ledFeedback(bool success) {
  //Serial.println("LED Flash initiated");
  unsigned long startTime = millis(); // Get the current time

  // Blink the LED for 5 seconds
  while (millis() - startTime < 5000) { // Run for 5000ms (5 seconds)
    if (success) {
       Serial.println("Flash SUCCESS");
      // Blink the LED quickly twice for success
      for (int i = 0; i < 2; i++) {
        digitalWrite(LED, HIGH);
        delay(100);
        digitalWrite(LED, LOW);
        delay(100);
      }
    } else {
      // Blink the LED slowly five times for failure
      Serial.println("Flash FAILED");
      for (int i = 0; i < 5; i++) {
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

void registerMachine() {
  // Function to register machine input
  String URL = String(memory.url);

  // Check if memory.url is null-terminated
  if (isNullTerminated(memory.url)) {
    // Check if the URL contains "http"
    if (URL.indexOf("http") != -1) {
      Serial.println("URL contains 'http'");
    } else {
      Serial.println("URL does not contain 'http'");
      ledFeedback(false);
      return; // Exit function early on error
    }
    Serial.println();
    Serial.print("Destination URL: ");
    Serial.print(URL);
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
    // Display setup form
    String formHtml = "<!doctype html><html lang='en'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><title>Machine Setup</title><style>*,::after,::before{box-sizing:border-box;}body{margin:0;font-family:'Segoe UI',Roboto,'Helvetica Neue',Arial,'Noto Sans','Liberation Sans';font-size:1rem;font-weight:400;line-height:1.5;color:#212529;background-color:#f5f5f5;}.form-control{display:block;width:100%;height:calc(1.5em + .75rem + 2px);border:1px solid #ced4da;}button{cursor: pointer;border:1px solid transparent;color:#fff;background-color:#007bff;border-color:#007bff;padding:.5rem 1rem;font-size:1.25rem;line-height:1.5;border-radius:.3rem;width:100%}.form-signin{width:100%;max-width:400px;padding:15px;margin:auto;}h1{text-align: center}</style> </head> <body><main class='form-signin'> <form action='/' method='post'> <h1 class=''>Machine Setup</h1><br/><div class='form-floating'><label>SSID</label><input type='text' class='form-control' name='ssid' value='" + String(memory.ssid) + "' > </div><div class='form-floating'><br/><label>Password</label><input type='password' class='form-control' name='password' value='" + String(memory.password) + "'></div><div class='form-floating'><br/><label>URL</label><input type='text' class='form-control' name='url' value='" + String(memory.url) + "'></div><br/><br/><div class='form-floating'><br/><label>Machine ID</label><input type='number' class='form-control' name='machineId' value='" + String(memory.machineId) + "'></div><br/><br/><button type='submit'>Save</button><br/><br/><button type='button' onclick='resetEEPROM()'>Reset</button><br/><br/><button type='button' onclick='rebootDevice()'>Reboot</button><p style='text-align: right'><a href='https://pruim.nl' style='color: #32C5FF'>Pruim Automatisering</a></p></form></main> <script>function resetEEPROM() { if (confirm('Are you sure you want to reset to factory defaults? This action cannot be undone.')) { fetch('/reset-eeprom', { method: 'POST' }).then(response => { if (!response.ok) { console.error('Error resetting EEPROM'); } else { alert('EEPROM reset successful'); location.reload(); } }); } } function rebootDevice() { if (confirm('Are you sure you want to reboot the device?')) { fetch('/reboot-device', { method: 'POST' }).then(response => { if (!response.ok) { console.error('Error rebooting device'); } else { alert('Device rebooted successfully'); } }); } }</script> </body></html>";
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

  // Turn off LED
  digitalWrite(LED, HIGH);

  // Loading Wifi Setup from EEPROM
  EEPROM.begin(sizeof(struct settings));
  EEPROM.get(0, memory);

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
      WiFi.mode(WIFI_AP);
      // Get the MAC address of the device
      uint8_t mac[6];
      WiFi.macAddress(mac);

      // Convert the MAC address to a string
      char macStr[18];
      sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

      // Create the SoftAP SSID with the device's MAC address
      String softAPSSID = "Setup Portal-" + String(macStr);

      WiFi.softAP(softAPSSID, "1234567890");
      Serial.println();
      Serial.println("WiFi Failed, Switching to AP mode.");
      Serial.print("SSID: ");
      Serial.println(softAPSSID);
      Serial.print("AP IP address: ");
      Serial.println(WiFi.softAPIP());
      break;
    }
  }

  if (WiFi.getMode() == WIFI_STA) {
    Serial.println();
    Serial.print("Wifi connected, local IP: ");
    Serial.print(WiFi.localIP());
    Serial.println();
    Serial.print(WiFi.macAddress());
    Serial.println();
  }

  // Define Pinmodes.
  pinMode(LED, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  server.on("/", handlePortal);
  server.on("/reset-eeprom", HTTP_POST, handleResetEEPROM);
  server.on("/reboot-device", HTTP_POST, handleRebootDevice);
  server.begin();
}

void loop() {
  server.handleClient();

  // Check the physical button state
  int buttonState = digitalRead(BUTTON_PIN);

  // If the button is pressed, toggle the LED state
  if (buttonState == HIGH) {
    registerMachine();
    Serial.println("Button Pressed");
    delay(250);
  }
  Serial.print(".");
  delay(100);
}
