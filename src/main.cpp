#include <Arduino.h>
#include <NimBLEDevice.h>

static NimBLEAdvertisedDevice* foundDevice = nullptr;
static NimBLERemoteCharacteristic* notifyChar = nullptr;
static NimBLERemoteCharacteristic* rwChar = nullptr;

static bool doConnect = false;
static bool connected = false;

// ===== Notify Callback =====
void notifyCallback(NimBLERemoteCharacteristic* c, uint8_t* data, size_t len, bool isNotify) {
    Serial.print("Notify: ");
    Serial.write(data, len);
    Serial.println();
}

// ===== Client Callbacks =====
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* client) override {
        Serial.println("Connected to server");
    }

    void onDisconnect(NimBLEClient* client) override {
        Serial.println("Disconnected");
        connected = false;
    }
};

// ===== Scan Callback =====
class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* dev) override {
        if (dev->isAdvertisingService(NimBLEUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b"))) {
            Serial.println("Found target device");
            NimBLEDevice::getScan()->stop();
            foundDevice = dev;
            doConnect = true;
        }
    }
};

// ===== Connect to Server =====
bool connectToServer() {
    NimBLEClient* client = NimBLEDevice::createClient();
    client->setClientCallbacks(new ClientCallbacks());

    if (!client->connect(foundDevice)) {
        Serial.println("Failed to connect");
        return false;
    }

    NimBLERemoteService* service = client->getService("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
    if (!service) {
        Serial.println("Service not found");
        client->disconnect();
        return false;
    }

    notifyChar = service->getCharacteristic("beb5483e-36e1-4688-b7f5-ea07361b26a8");
    rwChar     = service->getCharacteristic("1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e");

    if (!notifyChar || !rwChar) {
        Serial.println("Characteristic missing");
        client->disconnect();
        return false;
    }

    if (notifyChar->canNotify()) {
        notifyChar->subscribe(true, notifyCallback);
    }

    connected = true;
    return true;
}

// ===== Setup =====
void setup() {
    Serial.begin(115200);
    Serial.println("Starting NimBLE Client...");

    NimBLEDevice::init("");

    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
    scan->setActiveScan(true);
    scan->start(5, false);
}

// ===== Loop =====
unsigned long lastWrite = 0;

void loop() {
    if (doConnect && !connected) {
        if (connectToServer()) {
            Serial.println("Connected to server");
        }
        doConnect = false;
    }

    if (connected && rwChar) {
        unsigned long now = millis();
        if (now - lastWrite > 5000) {
            lastWrite = now;

            if (rwChar->canRead()) {
                std::string v = rwChar->readValue();
                Serial.printf("Read: %s\n", v.c_str());
            }

            if (rwChar->canWrite()) {
                String msg = "Client random: " + String(random(-1000, 0));
                rwChar->writeValue(msg.c_str());
                Serial.printf("Wrote: %s\n", msg.c_str());
            }
        }
    }
}
