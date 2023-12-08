/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
   Has a characteristic of: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E - used for receiving data with "WRITE" 
   Has a characteristic of: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E - used to send data with  "NOTIFY"

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   In this example rxValue is the data received (only accessible inside that function).
   And txValue is the data to be sent, in this example just a byte incremented every second. 
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <M5Core2.h>

BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID           "9f88320c-0223-402b-96d0-589e6b5ae335" // UART service UUID
#define CHARACTERISTIC_UUID_RX "27024b36-d3d1-4c2c-b05a-c2a317c43322"
#define CHARACTERISTIC_UUID_TX "5198f135-81ca-4e28-a2c8-5afdc1ee7141"

bool newPress = true;
bool beeping = false;

void DisplayInit(void) {       // Initialize the display. 显示屏初始化
    M5.Lcd.fillScreen(WHITE);  // Set the screen background color to white.
                               // 设置屏幕背景色为白色
    M5.Lcd.setTextColor(
        BLACK);  // Set the text color to black.  设置文字颜色为黑色
    M5.Lcd.setTextSize(2);  // Set font size to 2.  设置字体大小为2
}


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("device connected");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("device disconnected");
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");
        for (int i = 0; i < rxValue.length(); i++)
          Serial.print(rxValue[i]);

        Serial.println();
        Serial.println("*********");
      }
    }
};


void setup() {
  M5.begin(true, true, true, true, mbus_mode_t::kMBusModeOutput,
             true);  // Init M5Core2.  初始化 M5Core2
    DisplayInit();
    M5.Lcd.setTextColor(RED);
    M5.Lcd.setCursor(10,
                     10);  // Set the cursor at (10,10).  将光标设在（10，10）处
    M5.Lcd.printf("Press to beep other device");  // The screen prints the formatted string and
                                   // wraps it.  屏幕打印格式化字符串并换行

  Serial.begin(115200);

  // Create the BLE Device
  BLEDevice::init("UART Service");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
										CHARACTERISTIC_UUID_TX,
										BLECharacteristic::PROPERTY_NOTIFY
									);
                      
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
											 CHARACTERISTIC_UUID_RX,
											BLECharacteristic::PROPERTY_WRITE
										);

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
    if (deviceConnected) {
       TouchPoint_t pos =
        M5.Touch.getPressPoint();  // Stores the touch coordinates in pos.
                                   // 将触摸坐标存储在pos.内
        if (M5.Touch.ispressed() == true){
          if (newPress == true){
            newPress = false;
            beeping = !beeping;
          }
        }
        else {
          newPress = true;
        }
        M5.Lcd.setCursor(10, 10);
        if (beeping == true){
          M5.Lcd.fillScreen(GREEN);
          M5.Lcd.printf("Beeping!");
          pTxCharacteristic->setValue(&txValue, 1);
          pTxCharacteristic->notify();
          txValue++;
          delay(2000);
        }
        else {
          M5.Lcd.fillScreen(WHITE);
          M5.Lcd.printf("Press to beep other device");
          delay(500);
        }
	}

    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
		// do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
}