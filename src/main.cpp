#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>

const int LED_PIN = 2;
const char* WIFI_SSID = "SYOS_IoT_2.4";
const char* WIFI_PASSWORD = "Syos#2020";

const char* MQTT_HOST = "ac70a5c501fd400da48ea4a2abde61d0.s1.eu.hivemq.cloud";
const int MQTT_PORT = 8883;
const char* MQTT_USER = "esp32-satsmeter";
const char* MQTT_PASSWORD = "poggers@";
const char* MQTT_CLIENT_ID = "satsmeter-device-01";

const char* TOPIC_PUBLISH = "satsmeter/leituras";
const char* TOPIC_SUBSCRIBE = "satsmeter/comandos";

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

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

    pinMode(LED_PIN, OUTPUT);  
    digitalWrite(LED_PIN, LOW);
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

    if (Serial.available()) {
        String mensagem = Serial.readStringUntil('\n');
        mensagem.trim();

        if (mensagem.length() > 0) {
            if (mqttClient.publish(TOPIC_PUBLISH, mensagem.c_str())) {
                Serial.print("Mensagem publicada: ");
                Serial.println(mensagem);
            } else {
                Serial.println("Erro ao publicar mensagem.");
            }
        }
    }

    static unsigned long lastPublish = 0;

    if (millis() - lastPublish >= 1000) {
        lastPublish = millis();

        int leituraHall = hallRead();
        String payload = "{\"status\":\"ok\",\"data_hora\":\"" + getDataHoraFormatada() +
                        "\",\"hall\":" + String(leituraHall) + "}";

        if (mqttClient.publish(TOPIC_PUBLISH, payload.c_str())) {
        Serial.println("Publicação periódica enviada: " + payload);
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
    } else {
        Serial.println("Erro ao publicar mensagem periódica.");
    }
    }
}