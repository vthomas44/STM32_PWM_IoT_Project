#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define PWM_INPUT_PIN 4 // GPIO pin for PWM input

// Wi-Fi Credentials
const char* ssid = "WIFI SSID";
const char* password = "WIFI PASSWORD";

// MQTT Broker Details
const char* mqtt_server = "test.mosquitto.org";
const int mqtt_port = 1883;
const char* mqtt_topic = "pwmdatavonVarghese/pwm_data"; // Topic to publish

WiFiClient espClient;
PubSubClient client(espClient);

// I2C pins for Seeed XIAO ESP32-S3
#define sda_pin 5
#define scl_pin 6
// OLED settings
#define oled_width 128
#define oled_height 64
#define oled_i2c_address 0x3C
Adafruit_SSD1306 display(oled_width, oled_height, &Wire, -1);

volatile unsigned long risingedgetime = 0;
volatile unsigned long fallingedgetime = 0;
volatile unsigned long lastperiod = 0;
volatile unsigned long lasthightime = 0;
volatile bool newvalue = false;

void IRAM_ATTR handleinterrupt() {
    unsigned long now = micros();

    if (digitalRead(PWM_INPUT_PIN) == HIGH) {
        risingedgetime = now;
        if (fallingedgetime > 0 && (risingedgetime > fallingedgetime)) {
            unsigned long period = risingedgetime - fallingedgetime;
            if (period > 500) {
                lastperiod = period;
            }
        }
    } else {
        fallingedgetime = now;
        if (risingedgetime > 0 && (fallingedgetime > risingedgetime)) {
            unsigned long highperiod = fallingedgetime - risingedgetime;
            if (highperiod < lastperiod) { 
                lasthightime = highperiod;
                newvalue = true;
            }
        }
    }
}

void connectToWiFi() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
}

void connectToMQTT() {
    while (!client.connected()) {
        Serial.println("Connecting to MQTT...");
        if (client.connect("ESP32_Client")) {
            Serial.println("Connected to MQTT broker");
        } else {
            Serial.print("Failed, rc=");
            Serial.print(client.state());
            Serial.println(" Retrying in 5 seconds...");
            delay(5000); //delay for 5 seconds
        }
    }
}

void setup() {
    Serial.begin(115200);

    // Initialize OLED display
    Wire.begin(sda_pin, scl_pin);
    if (!display.begin(SSD1306_SWITCHCAPVCC, oled_i2c_address)) {
        Serial.println(F("OLED initialization failed!"));
        while (1);
    }
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Starting...");
    display.display();

    // Connect to WiFi
    connectToWiFi();

    // Set up MQTT
    client.setServer(mqtt_server, mqtt_port);

    // Configure PWM input pin
    pinMode(PWM_INPUT_PIN, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(PWM_INPUT_PIN), handleinterrupt, CHANGE);
}

void loop() {
    if (!client.connected()) {
        connectToMQTT();
    }
    client.loop();

    if (newvalue) {
        noInterrupts(); // Temporarily disable interrupts to safely read shared variables
        unsigned long period = lastperiod;
        unsigned long highTime = lasthightime;
        newvalue = false;
        interrupts();

        if (period > 0) {
            float frequency = 1000000.0 / period;
            float dutyCycle = (highTime * 100.0) / period;

            if (dutyCycle >= 0.0 && dutyCycle <= 100.0) {
                // Create JSON payload
                char payload[128];
                snprintf(payload, sizeof(payload), "{\"frequency\":%.2f,\"duty_cycle\":%.2f}", frequency, dutyCycle);

                // Publish JSON payload to MQTT topic
                client.publish(mqtt_topic, payload);

                // Display on OLED
                display.clearDisplay();
                display.setCursor(0, 0);
                display.setTextSize(1); // Adjust text size as needed
                display.printf("PWM Read by Varghese Thomas \n \n"); 
                display.printf("Frequency: %.2f Hz\n", frequency); // Print frequency to OLED
                display.printf("Duty Cycle: %.2f %%\n", dutyCycle); // Print duty cycle to OLED
                display.display(); // Update the OLED screen

                Serial.println(payload); // Print JSON payload to Serial Monitor
            }
        }
    }

    delay(100);
}
