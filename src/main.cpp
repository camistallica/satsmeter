#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_INA219.h>

const char* WIFI_SSID = "SYOS_IoT_2.4";
const char* WIFI_PASSWORD = "Syos#2020";

const char* MQTT_HOST = "ac70a5c501fd400da48ea4a2abde61d0.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8883;
const char* MQTT_USER = "esp32-satsmeter";
const char* MQTT_PASSWORD = "poggers@";
const char* MQTT_CLIENT_ID = "satsmeter-device-01";

const char* TOPIC_PUBLISH = "satsmeter/leituras";
const char* TOPIC_SUBSCRIBE = "satsmeter/comandos";

const int LED_PIN = 2;
const int RELAY_PIN = 5;


WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
Adafruit_INA219 ina219;

float energiaAcumulada_Wh = 0.0;
unsigned long ultimaLeituraEnergia_ms = 0;
bool sistemaAtivo = true;

void connectWiFi() {
    if (WiFi.status() == WL_CONNECTED)
        return;

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    Serial.print("Conectando ao Wi-Fi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nWi-Fi conectado!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String mensagem;

    for (unsigned int i = 0; i < length; i++) {
        mensagem += (char)payload[i];
    }

    Serial.println();
    Serial.println("========== MENSAGEM RECEBIDA ==========");
    Serial.print("Tópico : ");
    Serial.println(topic);
    Serial.print("Payload: ");
    Serial.println(mensagem);
    Serial.println("=======================================");

    if (mensagem == "1") {
        sistemaAtivo = true;
        digitalWrite(RELAY_PIN, HIGH);
        Serial.println("Sistema ligado: relé ativo, leitura retomada");
    } else if (mensagem == "0") {
        sistemaAtivo = false;
        digitalWrite(RELAY_PIN, LOW);
        Serial.println("Sistema desligado: relé cortado, leitura pausada");
    } else {
        Serial.println("Comando não reconhecido.");
    }
}

void connectMQTT() {
    while (!mqttClient.connected()) {
        Serial.print("Conectando ao broker MQTT... ");

        if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
            Serial.println("Conectado!");
            mqttClient.subscribe(TOPIC_SUBSCRIBE);
            Serial.print("Inscrito em: ");
            Serial.println(TOPIC_SUBSCRIBE);
        } else {
            Serial.print("Falha. Código: ");
            Serial.println(mqttClient.state());
            Serial.println("Nova tentativa em 5 segundos.");
            delay(5000);
        }
    }
}

String getDataHoraFormatada() {
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    char buffer[25];
    strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
    return String(buffer);
}

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("==============================");
    Serial.println("SATSMETER");
    Serial.println("==============================");

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, HIGH);

    Wire.begin();

    if (!ina219.begin()) {
        Serial.println("Falha ao encontrar o INA219. Verifique a fiação!");
        while (1) { delay(10); }
    }
    ina219.setCalibration_16V_400mA();
    Serial.println("INA219 inicializado.");

    connectWiFi();

    configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println("Sincronizando hora via NTP...");

    struct tm timeinfo;
    while (!getLocalTime(&timeinfo)) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("\nHora sincronizada!");

    espClient.setInsecure();
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(1024);

    ultimaLeituraEnergia_ms = millis();
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi desconectado.");
        connectWiFi();
    }

    if (!mqttClient.connected()) {
        connectMQTT();
    }

    mqttClient.loop();

    unsigned long agora = millis();

    if (sistemaAtivo) {
        if (agora - ultimaLeituraEnergia_ms >= 1000) {
            float potencia_mW = ina219.getPower_mW();
            float deltaHoras = (agora - ultimaLeituraEnergia_ms) / 3600000.0;
            energiaAcumulada_Wh += (potencia_mW / 1000.0) * deltaHoras;
            ultimaLeituraEnergia_ms = agora;
        }

        static unsigned long lastPublish = 0;

        if (agora - lastPublish >= 5000) {
            lastPublish = agora;

            float tensao_V = ina219.getBusVoltage_V();
            float corrente_A = ina219.getCurrent_mA() / 1000.0;
            float energia_kWh = energiaAcumulada_Wh / 1000.0;

            String payload = "{\n";
            payload += "  \"data_hora\":\"" + getDataHoraFormatada() + "\",\n";
            payload += "  \"tensao_v\":" + String(tensao_V, 2) + ",\n";
            payload += "  \"corrente_a\":" + String(corrente_A, 3) + ",\n";
            payload += "  \"energia_kwh\":" + String(energia_kWh, 6) + "\n";
            payload += "}";

            if (mqttClient.publish(TOPIC_PUBLISH, payload.c_str())) {
                Serial.println("Publicação periódica enviada:");
                Serial.println(payload);
                digitalWrite(LED_PIN, HIGH);
                delay(100);
                digitalWrite(LED_PIN, LOW);
            } else {
                Serial.println("Erro ao publicar mensagem periódica.");
            }
        }
    }
}