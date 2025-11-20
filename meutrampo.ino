#include <WiFi.h>
#include <PubSubClient.h>

// =============================
// CONFIGURAÇÕES EDITÁVEIS
// =============================
const char* default_SSID = "Wokwi-GUEST";
const char* default_PASSWORD = "";
const char* default_BROKER_MQTT = "20.163.23.245";
const int default_BROKER_PORT = 1883;

const char* default_TOPICO_SUBSCRIBE = "/TEF/lamp001/cmd";
const char* default_TOPICO_PUBLISH_1 = "/TEF/lamp001/attrs";
const char* default_TOPICO_PUBLISH_2 = "/TEF/lamp001/attrs/l";

const char* default_ID_MQTT = "fiware_001";
const int default_D4 = 2;

const char* topicPrefix = "lamp001";

// Variáveis configuráveis
char* SSID =      (char*)default_SSID;
char* PASSWORD =  (char*)default_PASSWORD;
char* BROKER_MQTT = (char*)default_BROKER_MQTT;
int   BROKER_PORT = default_BROKER_PORT;

char* TOPICO_SUBSCRIBE  = (char*)default_TOPICO_SUBSCRIBE;
char* TOPICO_PUBLISH_1  = (char*)default_TOPICO_PUBLISH_1;
char* TOPICO_PUBLISH_2  = (char*)default_TOPICO_PUBLISH_2;
char* ID_MQTT           = (char*)default_ID_MQTT;
int D4 = default_D4;

// =============================
// CONFIGURAÇÃO DO SENSOR PIR
// =============================
const int PIR_PIN = 13;  
int lastState = -1;

// =============================

WiFiClient espClient;
PubSubClient MQTT(espClient);
char EstadoSaida = '0';

void initSerial() {
    Serial.begin(115200);
}

void initWiFi() {
    delay(10);
    Serial.println("------ Conexão WI-FI ------");
    Serial.print("Conectando-se à rede: ");
    Serial.println(SSID);
    Serial.println("Aguarde...");
    reconectWiFi();
}

void initMQTT() {
    MQTT.setServer(BROKER_MQTT, BROKER_PORT);
    MQTT.setCallback(mqtt_callback);
}

void setup() {
    InitOutput();
    initSerial();
    pinMode(PIR_PIN, INPUT);
    initWiFi();
    initMQTT();
    delay(3000);
    
    MQTT.publish(TOPICO_PUBLISH_1, "s|on");
}

// =============================
// FUNÇÃO DO SENSOR PIR
// =============================
void handlePIR() {
    int state = digitalRead(PIR_PIN);

    if (state != lastState) {  
        lastState = state;

        if (state == HIGH) {
            Serial.println("Movimento detectado!");
            MQTT.publish(TOPICO_PUBLISH_2, "1");  
        } else {
            Serial.println("Sem movimento.");
            MQTT.publish(TOPICO_PUBLISH_2, "0");  
        }
    }
}
// =============================

void loop() {
    VerificaConexoesWiFIEMQTT();
    EnviaEstadoOutputMQTT();
    handlePIR();  
    MQTT.loop();
}

void reconectWiFi() {
    if (WiFi.status() == WL_CONNECTED)
        return;

    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(200);
        Serial.print(".");
    }
    Serial.println();
    Serial.println("WiFi conectado com sucesso!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    digitalWrite(D4, LOW);
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    String msg;

    for (int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }

    Serial.print("Mensagem recebida: ");
    Serial.println(msg);

    String onTopic  = String(topicPrefix) + "@on|";
    String offTopic = String(topicPrefix) + "@off|";

    if (msg.equals(onTopic)) {
        digitalWrite(D4, HIGH);
        EstadoSaida = '1';
    }

    if (msg.equals(offTopic)) {
        digitalWrite(D4, LOW);
        EstadoSaida = '0';
    }
}

void VerificaConexoesWiFIEMQTT() {
    if (!MQTT.connected())
        reconnectMQTT();
    reconectWiFi();
}

void EnviaEstadoOutputMQTT() {
    if (EstadoSaida == '1') {
        MQTT.publish(TOPICO_PUBLISH_1, "s|on");
        Serial.println("- LED ligado");
    }

    if (EstadoSaida == '0') {
        MQTT.publish(TOPICO_PUBLISH_1, "s|off");
        Serial.println("- LED desligado");
    }

    Serial.println("- Estado enviado ao broker.");
    delay(1000);
}

void InitOutput() {
    pinMode(D4, OUTPUT);
    digitalWrite(D4, HIGH);

    for (int i = 0; i < 10; i++) {
        digitalWrite(D4, !digitalRead(D4));
        delay(200);
    }
}

void reconnectMQTT() {
    while (!MQTT.connected()) {
        Serial.print("Tentando conectar ao broker MQTT: ");
        Serial.println(BROKER_MQTT);

        if (MQTT.connect(ID_MQTT)) {
            Serial.println("Conectado ao broker!");
            MQTT.subscribe(TOPICO_SUBSCRIBE);
        } else {
            Serial.println("Falha ao conectar. Tentando novamente...");
            delay(2000);
        }
    }
}
