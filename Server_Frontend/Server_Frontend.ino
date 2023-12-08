#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <M5Core2.h>
#include <vector>
////////////////////////////////////
//Definitions
#define SERVICE_UUID              "9f88320c-0223-402b-96d0-589e6b5ae335"
//***************will probably need to make more characteristics
#define CHARACTERISTIC_UUID_BASIC "5198f135-81ca-4e28-a2c8-5afdc1ee7141"
enum mode{
  main = 0x0,
  BLE = 0x1,
  logMenu = 0x2,
  sleepMenu = 0x3,
  find = 0x4
};
////////////////////////////////////
//Global Variables
enum mode currentMode;
BLEServer *pServer = NULL;
BLEService *pService = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

////////////////////////////////////
//Functions
//This checks the touch screen and any BLE signals paired to see if action
//needs to be taken
//**************unwritten
void sleepCheck();
//Allows you to manage BLE connections.
//***********unwritten
void BLEManage();
//finds a device by beeping it
//************unwritten
void findDevice();
//allows you to see logs
//***********unwritten
void logFunct();
///////////////////////////////
//server callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {

      deviceConnected = true;
      Serial.println("device connected");
      M5.Lcd.fillScreen(GREEN);
      M5.Lcd.setCursor(10,10);
      M5.Lcd.printf("Pair successful!");
      M5.Lcd.fillRect(0,190,100,50,0xF800);
      M5.Lcd.setCursor(8,207);
      M5.Lcd.println("Go back");
      delay(1000);
      Serial.println(pServer->getConnId());

    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("device disconnected");
    }
};
//////////////////////////////////
void setup() {
//general
  M5.begin(true, true, true, true, mbus_mode_t::kMBusModeOutput,true);
//display
  M5.Lcd.fillScreen(WHITE);
  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10, 10);
  //for debugging
  Serial.begin(115200);
  //////////////////BLE
  BLEDevice::init("Byron's Server");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  pService = pServer->createService(SERVICE_UUID);

  //**************only one characteristic right now, might need one for each
  //client, maybe even more for more functions
  pTxCharacteristic = pService->createCharacteristic(
		CHARACTERISTIC_UUID_BASIC,
		BLECharacteristic::PROPERTY_NOTIFY
		);
  //I think this allows a client to register for notifications
  pTxCharacteristic->addDescriptor(new BLE2902());
  pService->start();
  ////////////////////////////
  //complete
  M5.Lcd.println("Setup successful.");
  M5.Lcd.setCursor(10,26);
  M5.Lcd.println("Press to continue.");
  while (M5.Touch.ispressed() == false){
    M5.Touch.update();
    delay(100);
  }

  currentMode = main;
}


void loop() {
  TouchPoint_t pos;
  //wait for button to stop being pressed
  while (M5.Touch.ispressed() == true){
    M5.Touch.update();
    delay(100);
  }
  switch(currentMode){
    case main:
    //draw main menu
    M5.Lcd.fillRect(0,0,161,120,0x24BD);
    M5.Lcd.fillRect(161,0,160,120,0xF1AA);
    M5.Lcd.fillRect(0,121,160,120,0xFFE8);
    M5.Lcd.fillRect(161,120,160,120,0x56E8);
    M5.Lcd.setCursor(10,10);
    M5.Lcd.printf("Manage");
    M5.Lcd.setCursor(10,26);
    M5.Lcd.printf("bluetooth");
    M5.Lcd.setCursor(170,10);
    M5.Lcd.printf("Find a");
    M5.Lcd.setCursor(170, 26);
    M5.Lcd.printf("device");
    M5.Lcd.setCursor(10,130);
    M5.Lcd.printf("View device");
    M5.Lcd.setCursor(10,146);
    M5.Lcd.printf("logs");
    M5.Lcd.setCursor(170,130);
    M5.Lcd.printf("Sleep Mode");
    //wait for press
    while (M5.Touch.ispressed() == false){
      M5.Touch.update();
      delay(100);
    }
    pos = M5.Touch.getPressPoint();
    if ((pos.x<=160) and (pos.y<=120)){
      currentMode = BLE;
    }
    else if ((pos.x>160) and (pos.y<=120)){
      currentMode = find;
    }
    else if ((pos.x<=160) and (pos.y>120)){
      currentMode = logMenu;
    }
    else {
      currentMode = sleepMenu;
    }
    break;
    case BLE:
      while (M5.Touch.ispressed() == true){
        M5.Touch.update();
        delay(100);
      }
      BLEManage();
    break;
    case find:
      while (M5.Touch.ispressed() == true){
        M5.Touch.update();
        delay(100);
      }
      findDevice();
    break;
    case logMenu:
      while (M5.Touch.ispressed() == true){
        M5.Touch.update();
        delay(100);
      }
      logFunct();
    break;
    case sleepMenu:
    //**************I'm not really sure what light sleep does
    M5.Axp.LightSleep(SLEEP_SEC(1));
    sleepCheck();
    break;
    default:
    M5.Lcd.fillScreen(WHITE);
    M5.Lcd.setTextSize(3);
    M5.Lcd.println("Error in main menu handling");
    break;
  }
}
//////////////////////////////////////////
void sleepCheck(){
  currentMode = main;
}

void BLEManage(){
  TouchPoint_t screenPos;

  M5.Lcd.fillScreen(0x9e1e);
  M5.Lcd.setCursor(10,10);
  M5.Lcd.println("Advertising...");
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
  M5.Lcd.fillRect(0,190,100,50,0xF800);
  M5.Lcd.setCursor(8,207);
  M5.Lcd.println("Go back");
  while (M5.Touch.ispressed() == true){
    M5.Touch.update();
    delay(100);
  }
  while(true){
    delay(100);
    screenPos = M5.Touch.getPressPoint();
    if ((screenPos.x<100) and (screenPos.y>190)){
        break;
    }
  }
  currentMode = main;
}
void logFunct(){
  M5.Lcd.fillScreen(0xf7f7);
  M5.Lcd.setCursor(10,10);
}
//////////////////////////////////////////

///////////////////////////////////////
//findDevice
void findDevice(){
  TouchPoint_t screenPos;
  bool newPress = true;
  bool beeping = false;
  bool exit = false;

  M5.Lcd.fillScreen(0xFDDA);
  M5.Lcd.fillRect(0,190,100,50,0xF800);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(10,10);
  M5.Lcd.println("Select a device to find");
  M5.Lcd.setCursor(10,26);
  M5.Lcd.println("Only one device right now so just click anywhere");
  M5.Lcd.setCursor(8,207);
  M5.Lcd.println("Go back");
  while (M5.Touch.ispressed() == false){
    M5.Touch.update();
    delay(100);
  }
  while (M5.Touch.ispressed() == true){
    M5.Touch.update();
    delay(100);
  }

  while (true){
    delay(100);
    screenPos = M5.Touch.getPressPoint();
    //go back to main menu
    if ((screenPos.x<100) and (screenPos.y>190)){
      break;
    }

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
      //back button
      M5.Lcd.fillRect(0,190,100,50,0xF800);
      M5.Lcd.setCursor(8,207);
      M5.Lcd.println("Go back");
      //send beep
      pTxCharacteristic->setValue(&txValue, 1);
      pTxCharacteristic->notify();
      txValue++;
      delay(2000);
    }
    else {
      M5.Lcd.fillScreen(WHITE);
      M5.Lcd.printf("Press to beep other");
      M5.Lcd.setCursor(10,26);
      M5.Lcd.printf("device");
      //back button
      M5.Lcd.fillRect(0,190,100,50,0xF800);
      M5.Lcd.setCursor(8,207);
      M5.Lcd.println("Go back");
      //wait for press
      while (M5.Touch.ispressed() == false){
        M5.Touch.update();
        delay(100);
      }
    }
  }
  currentMode = main;
}

