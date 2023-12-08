#include "BLEDevice.h"
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <M5StickCPlus.h>
#include <cmath>
//for #1
/*
#define SERVICE_UUID              "9f88320c-0223-402b-96d0-589e6b5ae335"
#define CHARACTERISTIC_UUID_BASIC "5198f135-81ca-4e28-a2c8-5afdc1ee7141"
#define INFO_CHARACTERISTIC_UUID  "9d32b8fb-9184-4cae-9d1b-d385abce4c58"
#define IMU_UUID                  "c2b5bbf3-fe28-4d1a-88be-80a12cc8edb0"
//for #2
#define SERVICE_UUID              "83f22c98-61b2-450c-929a-3bdab75087df"
#define CHARACTERISTIC_UUID_BASIC "8c2937f0-438f-4665-bd50-bf7ba246679e"
#define INFO_CHARACTERISTIC_UUID  "8d137688-7797-4add-b556-248f175eb989"
#define IMU_UUID                  "289a0792-490e-4ee0-ac4c-e8638201115d"
*/
//for #3
#define SERVICE_UUID              "1e1338a8-4bc9-4f25-a870-9117050907e4"
#define CHARACTERISTIC_UUID_BASIC "a8239248-97e4-4c9b-819f-d378363b24f5"
#define INFO_CHARACTERISTIC_UUID  "25312303-4787-4c7f-8135-1c5e83e7cfe1"
#define IMU_UUID                  "9df6fbc6-fe88-461e-81b3-da4a2654239d"
/*
//for #4
#define SERVICE_UUID              "bb1edcc2-1d7b-4736-ae7b-b83e4b07ae9b"
#define CHARACTERISTIC_UUID_BASIC "3c37c743-66ed-4f58-9551-2f4ec42edce9"
#define INFO_CHARACTERISTIC_UUID  "c9ceb0dc-1065-458e-b0f5-e695d4c2a35a"
#define IMU_UUID                  "d7a1d53b-c825-4404-b085-169f26c735b9"
*/
//////////////////////////////////
//global variables
BLEServer *pServer = NULL;
BLEService *pService = NULL;
BLECharacteristic * pTxCharacteristic;
BLECharacteristic * infoCharacteristic;
BLECharacteristic * imuCharacteristic;
uint8_t txValue = 0;
bool deviceConnected = false;
bool connectionChange = false;
std::string temp = {};
int cyclesSinceMoved = 99;

float accX = 0.0F;
float accY = 0.0F;
float accZ = 0.0F;

////////////////////////////////////
//server callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      M5.Axp.ScreenBreath(80);
      connectionChange = true;
      deviceConnected = true;
      Serial.println("device connected");
      M5.Lcd.fillScreen(GREEN);
      M5.Lcd.setCursor(10,10);
      M5.Lcd.println("Connected");
      M5.IMU.getAccelData(&accX, &accY, &accZ);
    };

    void onDisconnect(BLEServer* pServer) {
      connectionChange = true;
      deviceConnected = false;
      BLEAdvertising *newAdvertise = pServer->getAdvertising();
      newAdvertise->addServiceUUID(SERVICE_UUID);
      newAdvertise->start();
    }
};


void setup() {
  //idk. General?
  M5.begin(true, true, true);
  //IMU
  M5.Imu.Init();
  //speaker
  ledcSetup(0, 50, 10);
  ledcAttachPin(26, 0);
  ledcWrite(0, 256);
  ledcWriteTone(0, 0);
  //BLE
  Serial.begin(115200);
  BLEDevice::init("Byron's Server2");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  pService = pServer->createService(SERVICE_UUID);
//Characteristics
  pTxCharacteristic = pService->createCharacteristic(
		CHARACTERISTIC_UUID_BASIC,
    BLECharacteristic::PROPERTY_WRITE_NR
		);
  pTxCharacteristic->setValue("Beep Characteristic");

  infoCharacteristic = pService->createCharacteristic(
    INFO_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ
    );
  infoCharacteristic->setValue("Device Name: Byron's Server, Service UUID: 9f88320c-0223-402b-96d0-589e6b5ae335");

  imuCharacteristic = pService->createCharacteristic(
    IMU_UUID,
    BLECharacteristic::PROPERTY_READ
  );
  imuCharacteristic->setValue("Not moving");

  //Menu
  M5.Lcd.fillScreen(WHITE);
  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.print("Setup");
  M5.Lcd.setCursor(10,26);
  M5.Lcd.print("complete.");
  M5.Lcd.setCursor(10,58);
  M5.Lcd.print("Press to");
  M5.Lcd.setCursor(10,74);
  M5.Lcd.print("connect to");
  M5.Lcd.setCursor(10,90);
  M5.Lcd.println("client.");
  //wait for press
  while (M5.BtnA.isReleased()){
    M5.update();
  }
  //start advertising
  M5.Lcd.fillScreen(YELLOW);
  M5.Lcd.setCursor(10,10);
  M5.Lcd.println("Advertis-");
  M5.Lcd.setCursor(10,26);
  M5.Lcd.print("ing...");
  delay(1000);

  pService->start();
  BLEAdvertising *newAdvertise = pServer->getAdvertising();
  newAdvertise->addServiceUUID(SERVICE_UUID);
  newAdvertise->start();
  Serial.println("Advertising...");
} // End of setup.


// This is the Arduino main loop function.
  //beeps if commanded to
void loop() {
  static float prevX, prevY, prevZ;
  if (connectionChange == true){
    delay(1500);
    connectionChange = false;
  }
  if (deviceConnected == true){
    if(pTxCharacteristic->getValue() == "beeping"){
      M5.Axp.ScreenBreath(80);
      M5.Lcd.fillScreen(RED);
      M5.Lcd.setCursor(10,10);
      M5.Lcd.print("BEEP");
      //speaker
      ledcWriteTone(0, 1250);
      //vibration
      delay(1000);
      ledcWriteTone(0, 0);
      for(int i = 0; i<5; i++){
        M5.Lcd.fillScreen(WHITE);
        delay(100);
        M5.Lcd.fillScreen(RED);
        delay(100);
      }
    }
    else {
      M5.Axp.ScreenBreath(0);
      M5.Lcd.fillScreen(BLACK);
    }
  }
  M5.update();
  //get IMU data
  prevX = accX;
  prevY = accY;
  prevZ = accZ;
  M5.IMU.getAccelData(&accX, &accY, &accZ);

  if ((abs(accX-prevX)+abs(accY-prevY)+abs(accZ-prevZ))/3 >= 0.2){
    cyclesSinceMoved = 0;
  }
  else {
    cyclesSinceMoved++;
  }
  if (cyclesSinceMoved <= 100){
    imuCharacteristic->setValue("Moving");
  }
  else if (cyclesSinceMoved > 1000){
    cyclesSinceMoved = 100;
    imuCharacteristic->setValue("Not moving");
  }
  else {
    imuCharacteristic->setValue("Not moving");
  }
  delay(100);
} // End of loop