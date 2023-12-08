#include "BLEDevice.h"
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <M5Core2.h>
#include "hugefile.h"
//#include "BLEScan.h"

//basic service and characteristic
#define SERVICE_UUID              "9f88320c-0223-402b-96d0-589e6b5ae335"
#define CHARACTERISTIC_UUID_BASIC "5198f135-81ca-4e28-a2c8-5afdc1ee7141"

//////////////////////////////////
//global variables
BLEServer *pServer = NULL;
BLEService *pService = NULL;
BLECharacteristic * pTxCharacteristic;
uint8_t txValue = 0;
bool deviceConnected = false;

////////////////////////////////////
//server callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {

      deviceConnected = true;
      Serial.println("device connected");
      M5.Lcd.fillScreen(GREEN);
      M5.Lcd.setCursor(10,10);
      M5.Lcd.println("Connected");

    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("device disconnected");
      M5.Lcd.fillScreen(RED);
      M5.Lcd.setCursor(10,10);
      M5.Lcd.println("Disconnected");
    }
};


void setup() {
  //idk. General?
  M5.begin(true, true, true, true, mbus_mode_t::kMBusModeOutput,true);
  //BLE
  Serial.begin(115200);
  BLEDevice::init("Byron's Server");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  pService = pServer->createService(SERVICE_UUID);
//Characteristic
  pTxCharacteristic = pService->createCharacteristic(
		CHARACTERISTIC_UUID_BASIC,
    BLECharacteristic::PROPERTY_WRITE_NR
		);
  pTxCharacteristic->addDescriptor(new BLE2902());
  //Menu
  M5.Lcd.fillScreen(WHITE);
  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.println("Setup complete. Press to");
  M5.Lcd.setCursor(10, 26);
  M5.Lcd.println("connect to client.");
  //wait for press
  while (M5.Touch.ispressed() == false){
    M5.Touch.update();
    delay(100);
  }
  while (M5.Touch.ispressed() == true){
    M5.Touch.update();
    delay(100);
  }
  //start advertising
  M5.Lcd.fillScreen(YELLOW);
  M5.Lcd.setCursor(10,10);
  M5.Lcd.println("Advertising...");
  delay(1000);

  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("Advertising...");
} // End of setup.


// This is the Arduino main loop function.
void loop() {
  //beeps if commanded to
  if(pTxCharacteristic->getValue() == "beeping"){
    M5.Axp.SetLDOEnable(3, true);
     M5.Spk.PlaySound(
                previewR,
                sizeof(
                    previewR));
      M5.Axp.SetLDOEnable(3, false);
      delay(1000);
  }
  delay(1000);
} // End of loop