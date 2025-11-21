// arquivo: pir_lamp_pir_auto_on_robust.ino
#include <WiFi.h>
#include <PubSubClient.h>

// =============================
// CONFIGURAÇÕES EDITÁVEIS
// =============================
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";

const char* MQTT_BROKER = "4.172.212.188"; // ajuste conforme seu broker/host
const int   MQTT_PORT   = 1883;

// principais tópicos (onde o esp publica)
const char* TOPIC_PUB_A = "/TEF/device001/attrs";
const char* TOPIC_PUB_B = "/smart/device001/attrs";
const char* TOPIC_PUB_C = "/device001/attrs";

// tópicos de motion
const char* TOPIC_M_A = "/TEF/device001/attrs/m";
const char* TOPIC_M_B = "/smart/device001/attrs/m";
const char* TOPIC_M_C = "/device001/attrs/m";

// iremos subscrever vários tópicos para garantir entrega do comando
const char* SUB_TOPICS[] = {
  "/TEF/device001/cmd",
  "/smart/device001/cmd",
  "/device001/cmd",
  "device001/cmd"
};
const int SUB_TOPICS_COUNT = sizeof(SUB_TOPICS) / sizeof(SUB_TOPICS[0]);

const char* DEVICE_PREFIX = "device001"; // prefixo usado nos comandos: device001@on|

const int LED_PIN = 2;   // pino do LED/lâmpada
const int PIR_PIN = 27;  // seu PIR está em GPIO 27

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

char lampState = '0'; // '1' = on, '0' = off
int lastPIR = -1;

// utility: unique client id
String makeClientId() {
  uint32_t id = (uint32_t)ESP.getEfuseMac();
  return String("esp32_") + String(id & 0xFFFFFF, HEX);
}

void connectWiFi() {
  Serial.print("Conectando WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 200) {
    delay(100);
    Serial.print(".");
    tries++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi conectado. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi não conectado (timeout).");
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.print("MQTT RECEIVED on [");
  Serial.print(topic);
  Serial.print("] => ");
  Serial.println(msg);

  // aceitar várias formas:
  // device001@on|  OR  on  OR {"value":""} etc.
  String onPattern  = String(DEVICE_PREFIX) + "@on|";
  String offPattern = String(DEVICE_PREFIX) + "@off|";

  // normaliza
  String trimmed = msg;
  trimmed.trim();

  if (trimmed.equalsIgnoreCase(onPattern) || trimmed.equalsIgnoreCase("on")) {
    Serial.println("CMD -> ON recebido");
    digitalWrite(LED_PIN, HIGH);
    lampState = '1';
    // publicar estado em todos tópicos (redundância)
    mqtt.publish(TOPIC_PUB_A, "s|on");
    mqtt.publish(TOPIC_PUB_B, "s|on");
    mqtt.publish(TOPIC_PUB_C, "s|on");
  } else if (trimmed.equalsIgnoreCase(offPattern) || trimmed.equalsIgnoreCase("off")) {
    Serial.println("CMD -> OFF recebido");
    digitalWrite(LED_PIN, LOW);
    lampState = '0';
    mqtt.publish(TOPIC_PUB_A, "s|off");
    mqtt.publish(TOPIC_PUB_B, "s|off");
    mqtt.publish(TOPIC_PUB_C, "s|off");
  } else {
    // alguns IoT Agents enviam payload JSON quando usando /iot/json; verifique e parse
    if (trimmed.startsWith("{") && trimmed.indexOf("on") >= 0) {
      digitalWrite(LED_PIN, HIGH);
      lampState = '1';
      mqtt.publish(TOPIC_PUB_A, "s|on");
      mqtt.publish(TOPIC_PUB_B, "s|on");
      mqtt.publish(TOPIC_PUB_C, "s|on");
      Serial.println("Heurística: ligando por JSON contendo 'on'.");
    } else if (trimmed.startsWith("{") && trimmed.indexOf("off") >= 0) {
      digitalWrite(LED_PIN, LOW);
      lampState = '0';
      mqtt.publish(TOPIC_PUB_A, "s|off");
      mqtt.publish(TOPIC_PUB_B, "s|off");
      mqtt.publish(TOPIC_PUB_C, "s|off");
      Serial.println("Heurística: desligando por JSON contendo 'off'.");
    } else {
      Serial.println("Mensagem desconhecida recebida (ignorando).");
    }
  }
}

void connectMQTT() {
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  String clientId = makeClientId();
  while (!mqtt.connected()) {
    Serial.print("Conectando MQTT com clientId ");
    Serial.print(clientId);
    Serial.print(" ... ");
    if (mqtt.connect(clientId.c_str())) {
      Serial.println("Conectado.");
      // subscrever em vários tópicos
      for (int i = 0; i < SUB_TOPICS_COUNT; i++) {
        mqtt.subscribe(SUB_TOPICS[i]);
        Serial.print("Subscribed to: ");
        Serial.println(SUB_TOPICS[i]);
      }
    } else {
      Serial.print("Falha, rc=");
      Serial.print(mqtt.state());
      Serial.println(". novo try em 2s.");
      delay(2000);
    }
  }
}

void publishStateAll(const char* payload) {
  mqtt.publish(TOPIC_PUB_A, payload);
  mqtt.publish(TOPIC_PUB_B, payload);
  mqtt.publish(TOPIC_PUB_C, payload);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(PIR_PIN, INPUT);

  // blink init
  digitalWrite(LED_PIN, HIGH); delay(120);
  digitalWrite(LED_PIN, LOW);  delay(120);
  digitalWrite(LED_PIN, HIGH); delay(120);
  digitalWrite(LED_PIN, LOW);

  connectWiFi();
  connectMQTT();

  // publica estado inicial desligado
  publishStateAll("s|off");
  mqtt.publish(TOPIC_M_A, "0");
  mqtt.publish(TOPIC_M_B, "0");
  mqtt.publish(TOPIC_M_C, "0");
}

void handlePIR() {
  int state = digitalRead(PIR_PIN);
  if (state != lastPIR) {
    lastPIR = state;
    if (state == HIGH) {
      Serial.println("PIR: movimento detectado!");
      // publicar motion
      mqtt.publish(TOPIC_M_A, "1");
      mqtt.publish(TOPIC_M_B, "1");
      mqtt.publish(TOPIC_M_C, "1");
      // ligar lamp se estiver desligada (modo C)
      if (lampState != '1') {
        digitalWrite(LED_PIN, HIGH);
        lampState = '1';
        publishStateAll("s|on");
        Serial.println("Lamp ligada automaticamente (PIR).");
      }
    } else {
      Serial.println("PIR: sem movimento.");
      mqtt.publish(TOPIC_M_A, "0");
      mqtt.publish(TOPIC_M_B, "0");
      mqtt.publish(TOPIC_M_C, "0");
      Serial.println("Modo C: lâmpada permanece no estado atual.");
    }
  }
}

void loop() {
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();

  handlePIR();

  // re-publish state periodicamente (mantém orion atualizado)
  if (lampState == '1') publishStateAll("s|on");
  else publishStateAll("s|off");

  delay(1000);
}