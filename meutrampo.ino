// arquivo: pir_lamp_pir_auto_on.ino
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
const char* default_TOPICO_PUBLISH_1 = "/TEF/lamp001/attrs";    // publica estado da lâmpada (s|on / s|off)
const char* default_TOPICO_PUBLISH_2 = "/TEF/lamp001/attrs/m";  // publica motion 1/0

const char* default_ID_MQTT = "fiware_001";
const int default_D4 = 2; // saída para LED/lâmpada

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
char EstadoSaida = '0'; // '1' = ligado, '0' = desligado (inicia desligado)

void initSerial() {
    Serial.begin(115200);
    delay(5);
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
    initSerial();
    InitOutput();
    pinMode(PIR_PIN, INPUT);
    initWiFi();
    initMQTT();
    delay(1000);

    // Publica estado inicial (desligado)
    MQTT.publish(TOPICO_PUBLISH_1, "s|off");
    Serial.println("Estado inicial publicado: s|off");
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
            // publica movimento
            MQTT.publish(TOPICO_PUBLISH_2, "1");
            // Modo C: liga automaticamente a lâmpada (mas não desliga automaticamente)
            if (EstadoSaida != '1') {
                digitalWrite(D4, HIGH);  // liga
                EstadoSaida = '1';
                MQTT.publish(TOPICO_PUBLISH_1, "s|on");
                Serial.println("Lamp ligado automaticamente por movimento.");
            }
        } else {
            Serial.println("Sem movimento.");
            // publica ausência de movimento
            MQTT.publish(TOPICO_PUBLISH_2, "0");
            // NÃO desliga a lâmpada automaticamente (Opção C)
            Serial.println("Lamp NÃO será desligado automaticamente (modo C).");
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

    // Não forçar estado do D4 aqui (estado controlado por InitOutput / comandos)
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    String msg;

    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }

    Serial.print("Mensagem recebida: ");
    Serial.println(msg);

    // formatação esperada: "lamp001@on|" ou "lamp001@off|"
    String onTopic  = String(topicPrefix) + "@on|";
    String offTopic = String(topicPrefix) + "@off|";

    if (msg.equals(onTopic)) {
        digitalWrite(D4, HIGH);
        EstadoSaida = '1';
        MQTT.publish(TOPICO_PUBLISH_1, "s|on");
        Serial.println("Comando MQTT: Ligar (on)");
    }

    if (msg.equals(offTopic)) {
        digitalWrite(D4, LOW);
        EstadoSaida = '0';
        MQTT.publish(TOPICO_PUBLISH_1, "s|off");
        Serial.println("Comando MQTT: Desligar (off)");
    }
}

void VerificaConexoesWiFIEMQTT() {
    if (!MQTT.connected())
        reconnectMQTT();
    reconectWiFi();
}

void EnviaEstadoOutputMQTT() {
    // envia periodicamente o estado atual (reduzido: só quando conectado)
    if (!MQTT.connected()) return;

    if (EstadoSaida == '1') {
        MQTT.publish(TOPICO_PUBLISH_1, "s|on");
        Serial.println("- Estado: LED ligado");
    } else {
        MQTT.publish(TOPICO_PUBLISH_1, "s|off");
        Serial.println("- Estado: LED desligado");
    }

    // evita delay grande no loop principal; mantenha curto
    delay(1000);
}

void InitOutput() {
    pinMode(D4, OUTPUT);
    // Inicialmente desligado
    digitalWrite(D4, LOW);
    EstadoSaida = '0';

    // pequeno blink de inicialização (2 piscadas)
    for (int i = 0; i < 2; i++) {
        digitalWrite(D4, HIGH);
        delay(150);
        digitalWrite(D4, LOW);
        delay(150);
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
