#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWiFiManager.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <ezButton.h>

#define TAG "PrinterShutdown"

#define PIN_BTN 39
#define PIN_BTN_LED 40

// WiFi stuff
WiFiEvent_t wifiState;
AsyncWebServer server(80);
DNSServer dns;
AsyncWiFiManager wifiManager(&server, &dns);
WiFiClient client; // or WiFiClientSecure for HTTPS
HTTPClient httpPrinter, httpPower;

// settings
uint16_t checkPeriod = 5000;
unsigned long checkTime = millis();
String printerAddr = "192.168.100.4";
String printerStatusURL = "";
String powerOffURL = "http://tasmota-printe/cm?cmnd=Power%20off";
bool shutdownMode = false;

ezButton button(PIN_BTN); // create ezButton object that attach to pin 7;

// functions
void WiFiEvent(WiFiEvent_t event);

void setup()
{
  // Serial
  Serial.begin(115200);
  Serial.setDebugOutput(true);

  // I/O
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_BTN_LED, OUTPUT);

  button.setDebounceTime(100); // set debounce time to 50 milliseconds

  // WiFI
  WiFi.onEvent(WiFiEvent);
  WiFi.setAutoReconnect(true);
  digitalWrite(LED_BUILTIN, HIGH);
  if (!wifiManager.autoConnect("printer-shutdown"))
  {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
  }
  ESP_LOGI(TAG, "IP: %s\n", WiFi.localIP().toString().c_str());

  // setup http requests
  printerStatusURL = "http://" + printerAddr + "/rr_status?type=3";
  httpPrinter.useHTTP10(true);
  httpPrinter.begin(client, printerStatusURL);
  httpPrinter.setTimeout(5000);
  httpPower.begin(client, powerOffURL);
  httpPower.setTimeout(5000);
}

void loop()
{
  button.loop(); // MUST call the loop() function first

  int btnState = button.getState();
  // ESP_LOGI(TAG, "Button state %d", btnState);
  if (button.isPressed())
  {
    ESP_LOGI(TAG, "Button pressed. Toggle shutdown mode");
    shutdownMode = !shutdownMode;
  }

  digitalWrite(PIN_BTN_LED, shutdownMode);

  if (shutdownMode && millis() - checkTime > checkPeriod)
  {
    // send request
    auto httpRetCode = httpPrinter.GET();
    ESP_LOGI(TAG, "Get printer status return code %d", httpRetCode);

    if (httpRetCode == 200)
    {
      // Parse response
      JsonDocument doc;
      deserializeJson(doc, httpPrinter.getStream());

      // read interesting fields
      float progress = doc["fractionPrinted"].as<float>();
      float bedTemp = doc["temps"]["bed"]["current"].as<float>();
      String status = doc["status"].as<String>();

      // print to console
      // serializeJsonPretty(doc, Serial);
      ESP_LOGI(TAG, "WiFI IP=%s RSSI=%d", WiFi.localIP().toString().c_str(), WiFi.RSSI());
      ESP_LOGI(TAG, "Status=%s Progress=%.2f bed_temp=%.1f", status.c_str(), progress, bedTemp);

      checkTime = millis();

      if (status == "I" && progress == 0 && bedTemp < 50)
      {
        ESP_LOGI(TAG, "Shutdown printer!");
        auto httpRetCode = httpPower.GET();
        ESP_LOGI(TAG, "Shutdown printer got return code %d", httpRetCode);
        if (httpRetCode == 200)
        {
          shutdownMode = false;
        }
      }
    }
  }
}

void WiFiEvent(WiFiEvent_t event)
{
  ESP_LOGI(TAG, "WiFi mode %d", WiFi.getMode());

  switch (event)
  {
  case ARDUINO_EVENT_WIFI_AP_START:
    ESP_LOGI(TAG, "AP Started");
    digitalWrite(LED_BUILTIN, HIGH);
    break;
  case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
    ESP_LOGI(TAG, "AP user connected");
    break;
  case ARDUINO_EVENT_WIFI_AP_STOP:
    ESP_LOGI(TAG, "AP Stopped");
    break;
  case ARDUINO_EVENT_WIFI_STA_START:
    ESP_LOGI(TAG, "STA Started");
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    break;
  case ARDUINO_EVENT_WIFI_STA_CONNECTED:
    ESP_LOGI(TAG, "STA Connected");
    break;
  case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    ESP_LOGI(TAG, "STA IPv4: ");
    ESP_LOGI(TAG, "%s", WiFi.localIP());
    digitalWrite(LED_BUILTIN, LOW);
    break;
  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
  case ARDUINO_EVENT_WIFI_STA_LOST_IP:
    ESP_LOGI(TAG, "STA Disconnected -> reconnect");
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    // WiFi.begin();
    break;
  case ARDUINO_EVENT_WIFI_STA_STOP:
    ESP_LOGI(TAG, "STA Stopped");
    break;
  default:
    break;
  }
  // ESP_LOGI(TAG, "WiFi mode %d", WiFi.getMode());
}