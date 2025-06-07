#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <liz99-project-1_inferencing.h>  // Edge Impulse SDK for local inference

// WiFi credentials
const char* ssid     = "oplus_co_apqtyc";
const char* password = "swlv3455";

// Local Flask server endpoint (replace with your PC's LAN IP)
const char* serverUrl = "http://192.168.1.159:5000/predict";  // e.g., http://<PC_IP>:5000/predict

// Confidence threshold (%)
#define CONFIDENCE_THRESHOLD 80.0

// MPU6050 instance
Adafruit_MPU6050 mpu;

// Sampling parameters
#define SAMPLE_RATE_MS      10
#define CAPTURE_DURATION_MS 1500
#define FEATURE_SIZE        EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE

// Pins
#define BUTTON_PIN A2
#define LED_PIN    A1

// HTTP client
HTTPClient http;

// Data buffer
float features[FEATURE_SIZE];

// State variables
bool capturing = false;
unsigned long lastSampleTime = 0;
unsigned long captureStartTime = 0;
int sampleCount = 0;
bool lastButtonState = HIGH;

// Edge Impulse callback (required for raw data)
int raw_feature_get_data(size_t offset, size_t length, float *out_ptr) {
    memcpy(out_ptr, features + offset, length * sizeof(float));
    return 0;
}

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Connect to WiFi
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print('.');
    }
    Serial.println(" connected");

    // Initialize MPU6050
    if (!mpu.begin()) {
        Serial.println("MPU6050 init failed");
        while (1) delay(10);
    }
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    Serial.println("MPU6050 ready. Press button to capture");
}

void loop() {
    bool curr = digitalRead(BUTTON_PIN);
    if (lastButtonState == HIGH && curr == LOW && !capturing) {
        capturing = true;
        captureStartTime = millis();
        lastSampleTime = millis();
        sampleCount = 0;
        Serial.println("Button pressed - start capture");
        digitalWrite(LED_PIN, HIGH);
    }
    lastButtonState = curr;

    if (capturing) {
        if (millis() - lastSampleTime >= SAMPLE_RATE_MS) {
            lastSampleTime = millis();
            sensors_event_t a, g, t;
            mpu.getEvent(&a, &g, &t);
            if (sampleCount * 3 < FEATURE_SIZE) {
                int idx = sampleCount * 3;
                features[idx]   = a.acceleration.x;
                features[idx+1] = a.acceleration.y;
                features[idx+2] = a.acceleration.z;
                sampleCount++;
            }
        }
        if (millis() - captureStartTime >= CAPTURE_DURATION_MS) {
            capturing = false;
            digitalWrite(LED_PIN, LOW);
            Serial.println("Capture complete - running inference");
            run_inference();
        }
    }
}

void run_inference() {
    // Local inference
    signal_t signal;
    signal.total_length = FEATURE_SIZE;
    signal.get_data = raw_feature_get_data;
    ei_impulse_result_t result;
    if (run_classifier(&signal, &result, false) != EI_IMPULSE_OK) {
        Serial.println("ERR: Classifier failed");
        return;
    }
    int bestI = 0;
    float bestV = result.classification[0].value;
    for (int i = 1; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        if (result.classification[i].value > bestV) {
            bestV = result.classification[i].value;
            bestI = i;
        }
    }
    float confPct = bestV * 100;
    const char* gesture = ei_classifier_inferencing_categories[bestI];
    Serial.printf("Local -> %s (%.1f%%)\n", gesture, confPct);

    // Offload to local Flask server if low confidence
    if (confPct < CONFIDENCE_THRESHOLD) {
        Serial.println("Low confidence - sending raw data to server...");
        sendRawDataToServer();
    } else {
        display_gesture_with_led(gesture);
    }
}

void sendRawDataToServer() {
    // Build JSON payload: { "data": [ [f0,f1,...] ] }
    size_t cap = JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(FEATURE_SIZE) + FEATURE_SIZE * 8;
    DynamicJsonDocument doc(cap);
    JsonArray outer = doc.createNestedArray("data");
    JsonArray arr = outer.createNestedArray();
    for (int i = 0; i < FEATURE_SIZE; i++) arr.add(features[i]);
    String payload;
    serializeJson(doc, payload);

    // Send HTTP request to local server
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(payload);
    Serial.print("HTTP Response code: "); Serial.println(code);
    if (code > 0) {
        String resp = http.getString();
        Serial.println("Server response: " + resp);
        DynamicJsonDocument rd(256);
        if (deserializeJson(rd, resp) == DeserializationError::Ok) {
            const char* sg = rd["gesture"];
            float cf = rd["confidence"];
            Serial.println("Server Inference Result:");
            Serial.print("Gesture: "); Serial.println(sg);
            Serial.print("Confidence: "); Serial.print(cf); Serial.println("%");
            display_gesture_with_led(sg);
        } else {
            Serial.println("JSON parse error");
        }
    } else {
        Serial.printf("Error sending POST: %s\n", http.errorToString(code).c_str());
    }
    http.end();
}

void display_gesture_with_led(const char* g) {
    digitalWrite(LED_PIN, LOW);
    delay(300);
    if (strcmp(g, "z") == 0 || strcmp(g, "Z") == 0) {
        for (int i = 0; i < 3; i++) { digitalWrite(LED_PIN, HIGH); delay(100); digitalWrite(LED_PIN, LOW); delay(100);}    
    } else if (strcmp(g, "o") == 0 || strcmp(g, "O") == 0) {
        for (int i = 0; i < 2; i++) { digitalWrite(LED_PIN, HIGH); delay(500); digitalWrite(LED_PIN, LOW); delay(300);}    
    } else if (strcmp(g, "v") == 0 || strcmp(g, "V") == 0) {
        digitalWrite(LED_PIN, HIGH); delay(800); digitalWrite(LED_PIN, LOW); delay(200);
        for (int i = 0; i < 2; i++) { digitalWrite(LED_PIN, HIGH); delay(200); digitalWrite(LED_PIN, LOW); delay(200);}    
    } else {
        digitalWrite(LED_PIN, HIGH); delay(1000); digitalWrite(LED_PIN, LOW);
    }
}
