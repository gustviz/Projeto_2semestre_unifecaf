#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <SinricPro.h>
#include <SinricProSwitch.h>
#include <SinricProLight.h>

// SINRIC PRO
#define APP_KEY       "c8a4e07e-6138-48fb-9669-be2d88e69cf6"
#define APP_SECRET    "4f350997-ce7a-40f0-8004-ab84832aa059-2a3ac1f7-24ac-43ee-9e26-89f2c78816d4"
#define SUB_ID       "6907ed316cf03d7232c29e7d"
#define SALA_ID       "6907f105fb7ad926788639ec"
#define FITALED_ID    "6907f1c1faf5b7278281cfe9"

const uint8_t RELAY_SALA_PIN = 4;   
const uint8_t RELAY_SUBWOOFER_PIN = 2;
const uint8_t RELAY_SPEAKER_ESQ_PIN = 5;
const uint8_t RELAY_SPEAKER_DIR_PIN = 18;
const uint8_t LED_DATA_PIN = 27;
const uint8_t SWITCH_SUBWOOFER_PIN = 17;
const uint8_t SWITCH_SALA_PIN = 19;
const uint8_t SWITCH_3_PIN = 14;
const uint8_t SWITCH_4_PIN = 16;
const uint8_t SWITCH_5_PIN = 25;
const uint8_t SWITCH_6_PIN = 26;
const uint8_t SWITCH_7_PIN = 32;
const uint8_t SWITCH_8_PIN = 33; 
#define NUM_LEDS 60         
#define LED_TYPE WS2812B  
#define LED_COLOR_ORDER GRB 

const char* MQTT_BROKER = "broker.hivemq.com"; 
const uint16_t MQTT_PORT = 1883; 
const char* MQTT_BASE = "home/automacao/esp32";               
const char* T_SET_SUBWOOFER = "home/automacao/esp32/subwoofer/set";  
const char* T_STATE_SUBWOOFER = "home/automacao/esp32/subwoofer/state"; 
const char* T_SET_SPEAKER_ESQ = "home/automacao/esp32/speaker_esq/set";  
const char* T_STATE_SPEAKER_ESQ = "home/automacao/esp32/speaker_esq/state"; 
const char* T_SET_SPEAKER_DIR = "home/automacao/esp32/speaker_dir/set";  
const char* T_STATE_SPEAKER_DIR = "home/automacao/esp32/speaker_dir/state"; 
const char* T_SET_SALA = "home/automacao/esp32/sala/set";   
const char* T_STATE_SALA = "home/automacao/esp32/sala/state";  
const char* T_LED_SET = "home/automacao/esp32/led/set";      
const char* T_LED_STATE = "home/automacao/esp32/led/state";
const char* T_SWITCH_STATE = "home/automacao/esp32/switch/state";
const char* T_SWITCH_SET = "home/automacao/esp32/switch/set";
const char* T_SWITCH_1_STATE = "home/automacao/esp32/switch/1/state";
const char* T_SWITCH_2_STATE = "home/automacao/esp32/switch/2/state";
const char* T_SWITCH_3_STATE = "home/automacao/esp32/switch/3/state";
const char* T_SWITCH_4_STATE = "home/automacao/esp32/switch/4/state";
const char* T_SWITCH_5_STATE = "home/automacao/esp32/switch/5/state";
const char* T_SWITCH_6_STATE = "home/automacao/esp32/switch/6/state";
const char* T_SWITCH_7_STATE = "home/automacao/esp32/switch/7/state";
const char* T_SWITCH_8_STATE = "home/automacao/esp32/switch/8/state";

bool subwooferOn = false;
bool speakerEsqOn = false;
bool speakerDirOn = false;
bool salaOn = false;

unsigned long lastSwitchSubwooferMs = 0;
unsigned long lastSwitchSalaMs = 0;
const unsigned long debounceMs = 150;

bool switchStates[9] = {false, false, false, false, false, false, false, false, false};
int lastSwitchReadings[9] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
unsigned long lastSwitchDebounceMs[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

CRGB leds[NUM_LEDS];
String ledMode = "solid";
CRGB currentColor = CRGB::White;
uint8_t ledBrightness = 128;

String getChipId() {
  uint64_t mac = ESP.getEfuseMac();
  char buf[17];
  sprintf(buf, "%04X%08X", (uint16_t)(mac>>32), (uint32_t)mac);
  return String(buf);
}

void publishRelayState();
void publishLedState();
void applyRelayState();
void applyLedState();
void updateSinricLedEvents();
void publishSwitchStates();
void handleSwitchAction(uint8_t switchNum, bool state);

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String t = String(topic);
  String msg;
  msg.reserve(length);
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  msg.trim();

  if (t == T_SET_SUBWOOFER) {
    subwooferOn = msg.equalsIgnoreCase("on") || msg == "1" || msg.equalsIgnoreCase("true");
    applyRelayState();
    publishRelayState();
    SinricProSwitch& dev = SinricPro[SUB_ID];
    dev.sendPowerStateEvent(subwooferOn);
  } else if (t == T_SET_SPEAKER_ESQ) {
    speakerEsqOn = msg.equalsIgnoreCase("on") || msg == "1" || msg.equalsIgnoreCase("true");
    applyRelayState();
    publishRelayState();
  } else if (t == T_SET_SPEAKER_DIR) {
    speakerDirOn = msg.equalsIgnoreCase("on") || msg == "1" || msg.equalsIgnoreCase("true");
    applyRelayState();
    publishRelayState();
  } else if (t == T_SET_SALA) {
    salaOn = msg.equalsIgnoreCase("on") || msg == "1" || msg.equalsIgnoreCase("true");
    applyRelayState();
    publishRelayState();
    SinricProSwitch& dev = SinricPro[SALA_ID];
    dev.sendPowerStateEvent(salaOn);
  } else if (t == T_LED_SET) {
    
   
    String s = msg;
    s.replace(" ", "");
    s.replace("\n", "");
    auto getVal = [&](const String& key)->String{
      int k = s.indexOf("\"" + key + "\"");
      if (k < 0) return "";
      k = s.indexOf(":", k);
      if (k < 0) return "";
      int start = k + 1;
      if (s[start] == '"') {
        int end = s.indexOf('"', start + 1);
        if (end < 0) return "";
        return s.substring(start + 1, end);
      } else {
        int end = s.indexOf(',', start);
        if (end < 0) end = s.indexOf('}', start);
        if (end < 0) return "";
        return s.substring(start, end);
      }
    };

    String mode = getVal("mode");
    if (mode.length()) ledMode = mode;

    String col = getVal("color");
    if (col.startsWith("#") && col.length() == 7) {
      long rgb = strtol(col.substring(1).c_str(), nullptr, 16);
      currentColor = CRGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
    }

    String bri = getVal("brightness");
    if (bri.length()) {
      int b = bri.toInt();
      if (b < 0) b = 0; if (b > 255) b = 255;
      ledBrightness = (uint8_t)b;
    }

    applyLedState();
    publishLedState();
    updateSinricLedEvents();
  } else if (t == T_SWITCH_SET) {
    String s = msg;
    s.replace(" ", "");
    s.replace("\n", "");
    auto getVal = [&](const String& key)->String{
      int k = s.indexOf("\"" + key + "\"");
      if (k < 0) return "";
      k = s.indexOf(":", k);
      if (k < 0) return "";
      int start = k + 1;
      if (s[start] == '"') {
        int end = s.indexOf('"', start + 1);
        if (end < 0) return "";
        return s.substring(start + 1, end);
      } else {
        int end = s.indexOf(',', start);
        if (end < 0) end = s.indexOf('}', start);
        if (end < 0) return "";
        return s.substring(start, end);
      }
    };
    
    String swNum = getVal("switch");
    String swState = getVal("state");
    if (swNum.length() && swState.length()) {
      uint8_t num = swNum.toInt();
      if (num >= 1 && num <= 8) {
        bool state = swState.equalsIgnoreCase("on") || swState == "1" || swState.equalsIgnoreCase("true");
        switchStates[num] = state;
        handleSwitchAction(num, state);
        publishSwitchStates();
      }
    }
  }
}

void ensureMqttConnected() {
  if (mqttClient.connected()) return;
  String clientId = String("ESP32-QQ-") + getChipId();
  while (!mqttClient.connected()) {
    if (mqttClient.connect(clientId.c_str())) {
      mqttClient.subscribe(T_SET_SUBWOOFER);
      mqttClient.subscribe(T_SET_SPEAKER_ESQ);
      mqttClient.subscribe(T_SET_SPEAKER_DIR);
      mqttClient.subscribe(T_SET_SALA);
      mqttClient.subscribe(T_LED_SET);
      mqttClient.subscribe(T_SWITCH_SET);
      publishRelayState();
      publishLedState();
      publishSwitchStates();
    } else {
      delay(1500);
    }
  }
}

bool onPowerStateCoz(const String& deviceId, bool &state) {
  subwooferOn = state;
  applyRelayState();
  publishRelayState();
  return true;
}

bool onPowerStateSala(const String& deviceId, bool &state) {
  salaOn = state;
  applyRelayState();
  publishRelayState();
  return true;
}

bool onLedPower(const String& deviceId, bool &state) {
  if (deviceId != FITALED_ID) return false;
  ledMode = state ? (ledMode == "off" ? "solid" : ledMode) : "off";
  applyLedState();
  publishLedState();
  return true;
}

bool onLedBrightness(const String& deviceId, int &brightness) {
  if (deviceId != FITALED_ID) return false;
  int b = constrain(brightness, 0, 100);
  ledBrightness = (uint8_t) map(b, 0, 100, 0, 255);
  if (ledBrightness == 0) ledMode = "off"; else if (ledMode == "off") ledMode = "solid";
  applyLedState();
  publishLedState();
  return true;
}
bool onLedColor(const String& deviceId, byte &r, byte &g, byte &b) {
  if (deviceId != FITALED_ID) return false;
  currentColor = CRGB(r, g, b);
  if (ledMode == "off" && (ledBrightness > 0)) ledMode = "solid";
  applyLedState();
  publishLedState();
  return true;
}

void setupSinric() {
  SinricProSwitch& subwoofer = SinricPro[SUB_ID];
  SinricProSwitch& sala    = SinricPro[SALA_ID];
  subwoofer.onPowerState(onPowerStateCoz);
  sala.onPowerState(onPowerStateSala);

  SinricProLight& fita = SinricPro[FITALED_ID];
  fita.onPowerState(onLedPower);
  fita.onBrightness(onLedBrightness);
  fita.onColor(onLedColor);
  SinricPro.begin(APP_KEY, APP_SECRET);
}

void applyRelayState() {
  digitalWrite(RELAY_SUBWOOFER_PIN, subwooferOn ? HIGH : LOW);
  digitalWrite(RELAY_SPEAKER_ESQ_PIN, speakerEsqOn ? HIGH : LOW);
  digitalWrite(RELAY_SPEAKER_DIR_PIN, speakerDirOn ? HIGH : LOW);
  digitalWrite(RELAY_SALA_PIN, salaOn ? HIGH : LOW);
}

void publishRelayState() {
  mqttClient.publish(T_STATE_SUBWOOFER, subwooferOn ? "on" : "off", true);
  mqttClient.publish(T_STATE_SPEAKER_ESQ, speakerEsqOn ? "on" : "off", true);
  mqttClient.publish(T_STATE_SPEAKER_DIR, speakerDirOn ? "on" : "off", true);
  mqttClient.publish(T_STATE_SALA, salaOn ? "on" : "off", true);
}

void applyLedState() {
  FastLED.setBrightness(ledBrightness);
  if (ledMode == "off" || ledBrightness == 0) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
  } else if (ledMode == "solid") {
    fill_solid(leds, NUM_LEDS, currentColor);
  } else if (ledMode == "rainbow") {
    static uint8_t hue = 0;
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CHSV(hue + i * 4, 255, 255);
    }
    hue += 2;
  } else {
    fill_solid(leds, NUM_LEDS, currentColor);
  }
  FastLED.show();
}

String colorToHex(const CRGB& c) {
  char buf[8];
  sprintf(buf, "#%02X%02X%02X", c.r, c.g, c.b);
  return String(buf);
}

void publishLedState() {
  String json = String("{\"mode\":\"") + ledMode + "\"," +
                "\"color\":\"" + colorToHex(currentColor) + "\"," +
                "\"brightness\":" + String(ledBrightness) + "}";
  mqttClient.publish(T_LED_STATE, json.c_str(), true);
}

void updateSinricLedEvents() {
  bool power = !(ledMode == "off" || ledBrightness == 0);
  SinricProLight& dev = SinricPro[FITALED_ID];
  dev.sendPowerStateEvent(power);
  int bri100 = map(ledBrightness, 0, 255, 0, 100);
  dev.sendBrightnessEvent(bri100);
  dev.sendColorEvent(currentColor.r, currentColor.g, currentColor.b);
}

void publishSwitchStates() {
  for (uint8_t i = 1; i <= 8; i++) {
    const char* topics[] = {
      "",
      T_SWITCH_1_STATE,
      T_SWITCH_2_STATE,
      T_SWITCH_3_STATE,
      T_SWITCH_4_STATE,
      T_SWITCH_5_STATE,
      T_SWITCH_6_STATE,
      T_SWITCH_7_STATE,
      T_SWITCH_8_STATE
    };
    mqttClient.publish(topics[i], switchStates[i] ? "on" : "off", true);
  }
  
  String json = "{";
  for (uint8_t i = 1; i <= 8; i++) {
    json += "\"" + String(i) + "\":" + (switchStates[i] ? "true" : "false");
    if (i < 8) json += ",";
  }
  json += "}";
  mqttClient.publish(T_SWITCH_STATE, json.c_str(), true);
}

void handleSwitchAction(uint8_t switchNum, bool turnOn) {
  switch (switchNum) {
    case 1:
      subwooferOn = turnOn;
      applyRelayState();
      publishRelayState();
      {
        SinricProSwitch& dev = SinricPro[SUB_ID];
        dev.sendPowerStateEvent(subwooferOn);
      }
      break;
    case 2:
      salaOn = turnOn;
      applyRelayState();
      publishRelayState();
      {
        SinricProSwitch& dev = SinricPro[SALA_ID];
        dev.sendPowerStateEvent(salaOn);
      }
      break;
    case 3:
      speakerEsqOn = turnOn;
      applyRelayState();
      publishRelayState();
      break;
    case 4:
      speakerDirOn = turnOn;
      applyRelayState();
      publishRelayState();
      break;
    case 5:
      if (turnOn) {
        ledMode = "solid";
        if (ledBrightness == 0) ledBrightness = 128;
      } else {
        ledMode = "off";
      }
      applyLedState();
      publishLedState();
      updateSinricLedEvents();
      break;
    case 6:
      if (turnOn) {
        ledMode = "rainbow";
        if (ledBrightness == 0) ledBrightness = 128;
      } else {
        ledMode = "solid";
      }
      applyLedState();
      publishLedState();
      updateSinricLedEvents();
      break;
    case 7:
    case 8:
      break;
  }
}

void setup() {
  pinMode(RELAY_SUBWOOFER_PIN, OUTPUT);
  pinMode(RELAY_SPEAKER_ESQ_PIN, OUTPUT);
  pinMode(RELAY_SPEAKER_DIR_PIN, OUTPUT);
  pinMode(RELAY_SALA_PIN, OUTPUT);
  pinMode(SWITCH_SUBWOOFER_PIN, INPUT_PULLUP);
  pinMode(SWITCH_SALA_PIN, INPUT_PULLUP);
  pinMode(SWITCH_3_PIN, INPUT_PULLUP);
  pinMode(SWITCH_4_PIN, INPUT_PULLUP);
  pinMode(SWITCH_5_PIN, INPUT_PULLUP);
  pinMode(SWITCH_6_PIN, INPUT_PULLUP);
  pinMode(SWITCH_7_PIN, INPUT_PULLUP);
  pinMode(SWITCH_8_PIN, INPUT_PULLUP);
  
  for (uint8_t i = 1; i <= 8; i++) {
    const uint8_t pins[] = {0, SWITCH_SUBWOOFER_PIN, SWITCH_SALA_PIN, SWITCH_3_PIN, SWITCH_4_PIN, 
                             SWITCH_5_PIN, SWITCH_6_PIN, SWITCH_7_PIN, SWITCH_8_PIN};
    lastSwitchReadings[i] = digitalRead(pins[i]);
    switchStates[i] = (lastSwitchReadings[i] == LOW);
  }
  
  applyRelayState();

  FastLED.addLeds<LED_TYPE, LED_DATA_PIN, LED_COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.clear(true);
  FastLED.setBrightness(ledBrightness);

  Serial.begin(115200);
  delay(100);

  WiFiManager wm;
  String apName = String("ESP32-Automacao-") + getChipId();
  wm.setConfigPortalTimeout(180);
  if (!wm.autoConnect(apName.c_str())) {
    ESP.restart();
  }

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  setupSinric();
}

void loop() {
  ensureMqttConnected();
  mqttClient.loop();
  SinricPro.handle();

  unsigned long now = millis();
  int swSub = digitalRead(SWITCH_SUBWOOFER_PIN);
  int swS = digitalRead(SWITCH_SALA_PIN);

  static int lastSwSub = HIGH;
  static int lastSwS = HIGH;

  if (swSub != lastSwSub) {
    if ((now - lastSwitchSubwooferMs) > debounceMs) {
      lastSwitchSubwooferMs = now;
      lastSwSub = swSub;
      subwooferOn = !subwooferOn; 
      applyRelayState();
      publishRelayState();
      SinricProSwitch& dev = SinricPro[SUB_ID];
      dev.sendPowerStateEvent(subwooferOn);
    }
  }

  if (swS != lastSwS) {
    if ((now - lastSwitchSalaMs) > debounceMs) {
      lastSwitchSalaMs = now;
      lastSwS = swS;
      salaOn = !salaOn; 
      applyRelayState();
      publishRelayState();
      SinricProSwitch& dev = SinricPro[SALA_ID];
      dev.sendPowerStateEvent(salaOn);
    }
  }

  const uint8_t switchPins[] = {0, SWITCH_SUBWOOFER_PIN, SWITCH_SALA_PIN, SWITCH_3_PIN, SWITCH_4_PIN, 
                                SWITCH_5_PIN, SWITCH_6_PIN, SWITCH_7_PIN, SWITCH_8_PIN};
  
  for (uint8_t i = 1; i <= 8; i++) {
    int reading = digitalRead(switchPins[i]);
    
    if (reading == LOW && lastSwitchReadings[i] == HIGH) {
      if ((now - lastSwitchDebounceMs[i]) > debounceMs) {
        switchStates[i] = !switchStates[i];
        handleSwitchAction(i, switchStates[i]);
        publishSwitchStates();
        lastSwitchDebounceMs[i] = now;
      }
    } else if (reading == HIGH) {
      lastSwitchDebounceMs[i] = now;
    }
    
    lastSwitchReadings[i] = reading;
  }

  static unsigned long lastLedAnim = 0;
  if (ledMode == "rainbow") {
    if (now - lastLedAnim >= 20) {
      lastLedAnim = now;
      applyLedState();
    }
  }
}
