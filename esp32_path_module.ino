#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>
#include <BLEScan.h>

// ===================== CONFIGURATION =====================
// Build this same sketch for each board and change MODULE_NUMBER.
// Naming scheme used: "Module No. x Path A".
static const int MODULE_NUMBER = 1;   // Set to 1, 2, or 3 per ESP32
static const char PATH_LETTER = 'A';  // Path type requested by user

static const int LED_PIN = 2;         // On-board LED for most ESP32 boards
static const int TRIGGER_PIN = 4;     // Button/sensor for Module 1 (active LOW)

// Shared BLE identifiers for this path network
static BLEUUID PATH_SERVICE_UUID("91bad492-b950-4226-aa2b-4ede9fa42f59");

// Advertisement payloads for each module in the chain
static const String MODULE1_SIGNAL = "M1_PATH_A_TRIGGER";
static const String MODULE2_SIGNAL = "M2_PATH_A_TRIGGER";

// ===================== GLOBALS =====================
BLEAdvertising *advertising = nullptr;
BLEScan *scanner = nullptr;

volatile bool pathATriggered = false;
volatile bool lastButtonState = HIGH;

String expectedIncomingSignal() {
  if (MODULE_NUMBER == 2) return MODULE1_SIGNAL;
  if (MODULE_NUMBER == 3) return MODULE2_SIGNAL;
  return "";
}

String outgoingSignal() {
  if (MODULE_NUMBER == 1) return MODULE1_SIGNAL;
  if (MODULE_NUMBER == 2) return MODULE2_SIGNAL;
  return "";
}

String moduleName() {
  return "Module No. " + String(MODULE_NUMBER) + " Path " + String(PATH_LETTER);
}

void pulseLed(int count = 2, int onMs = 250, int offMs = 180) {
  for (int i = 0; i < count; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(onMs);
    digitalWrite(LED_PIN, LOW);
    delay(offMs);
  }
}

void startBroadcast(const String &signalPayload, int durationMs = 3000) {
  BLEAdvertisementData advData;
  advData.setName(moduleName().c_str());
  advData.setServiceData(PATH_SERVICE_UUID, signalPayload.c_str());

  advertising->stop();
  advertising->setAdvertisementData(advData);
  advertising->start();

  Serial.printf("[TX] %s broadcasting: %s\n", moduleName().c_str(), signalPayload.c_str());
  delay(durationMs);
  advertising->stop();
}

class PathScanCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (!advertisedDevice.haveServiceData() || !advertisedDevice.haveServiceDataUUID()) {
      return;
    }

    if (!(advertisedDevice.getServiceDataUUID().equals(PATH_SERVICE_UUID))) {
      return;
    }

    std::string data = advertisedDevice.getServiceData();
    String payload = String(data.c_str());
    String expected = expectedIncomingSignal();

    if (payload == expected) {
      Serial.printf("[RX] %s received signal: %s\n", moduleName().c_str(), payload.c_str());
      pathATriggered = true;
    }
  }
};

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  if (MODULE_NUMBER == 1) {
    pinMode(TRIGGER_PIN, INPUT_PULLUP);
  }

  BLEDevice::init(moduleName().c_str());
  BLEServer *server = BLEDevice::createServer();
  (void)server;

  advertising = BLEDevice::getAdvertising();

  scanner = BLEDevice::getScan();
  scanner->setAdvertisedDeviceCallbacks(new PathScanCallbacks());
  scanner->setActiveScan(true);
  scanner->setInterval(300);
  scanner->setWindow(120);

  Serial.println("========================================");
  Serial.printf("Booted: %s\n", moduleName().c_str());
  Serial.println("Role behavior:");
  if (MODULE_NUMBER == 1) {
    Serial.println("- Wait for local trigger on TRIGGER_PIN.");
    Serial.println("- Send Bluetooth signal to Module No. 2 Path A.");
  } else if (MODULE_NUMBER == 2) {
    Serial.println("- Listen for Module No. 1 Path A signal.");
    Serial.println("- Light up and relay signal to Module No. 3 Path A.");
  } else if (MODULE_NUMBER == 3) {
    Serial.println("- Listen for Module No. 2 Path A signal.");
    Serial.println("- Light up to guide people to destination.");
  }
  Serial.println("========================================");
}

void handleModule1Trigger() {
  bool buttonState = digitalRead(TRIGGER_PIN);

  // Falling edge trigger (button/sensor active LOW)
  if (lastButtonState == HIGH && buttonState == LOW) {
    Serial.println("[LOCAL] Module No. 1 Path A trigger activated.");
    pulseLed(1, 400, 120);
    startBroadcast(outgoingSignal(), 2800);
  }

  lastButtonState = buttonState;
}

void loop() {
  if (MODULE_NUMBER == 1) {
    handleModule1Trigger();
    delay(30);
    return;
  }

  // Modules 2 and 3: scan continuously for incoming signal
  scanner->start(1, false);  // Scan in short windows repeatedly

  if (pathATriggered) {
    pathATriggered = false;
    pulseLed(3, 240, 120);

    if (MODULE_NUMBER == 2) {
      startBroadcast(outgoingSignal(), 2800);
    }
  }

  delay(80);
}
