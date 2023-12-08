#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <M5Core2.h>
////////////////////////////////////
//Definitions
// The remote service we wish to connect to.
//our service
static BLEUUID serviceUUID("9f88320c-0223-402b-96d0-589e6b5ae335");
//our characteristic
static BLEUUID    charUUID("5198f135-81ca-4e28-a2c8-5afdc1ee7141");

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
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;
BLEScan* pBLEScan;
////////////////////////////////////
//Functions
//currently puts it to sleep for a couple seconds. Might not even be used in the
//final design
void sleepCheck();
//Scans for and automatically adds devices by name
void BLEManage();
//finds a device by beeping it
void findDevice();
//allows you to see logs
void logFunct();
///////////////////////////////
//server callbacks
/*to be delted
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    Serial.print("Notify callback for characteristic ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);
    Serial.print("data: ");
    Serial.println((char*)pData);
}
*/
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    //connection handled in another function
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("onDisconnect");
  }
};
/////////////////////////////////////
//server connection
bool connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());
    
    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to server");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our service");


    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our characteristic");
    if(pRemoteCharacteristic->canNotify()){
      pRemoteCharacteristic->registerForNotify(notifyCallback);
      Serial.println("notify register complete");
    }
    connected = true;
    return true;
}
//Scan for BLE servers and find the first one that advertises the name
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());
      //check device name
      bool found = true;
      std::string name = "Byron's Server";
      for (int i = 0; i<name.length(); i++){
        if (advertisedDevice.toString().c_str()[i+6] != name[i]){
          found = false;
        }
      }
    if (found == true) {
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;
    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks
//////////////////////////////////////////////////
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
  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("");

  //setup scan. Doesn't actually run scan yet.
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  
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
  ///////////////////BLE connection
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("We are now connected to the BLE Server.");
      M5.Lcd.fillScreen(GREEN);
      M5.Lcd.setCursor(10,10);
      M5.Lcd.println("Connected!");
      delay(1500);
    } 
    else {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
      M5.Lcd.fillScreen(RED);
      M5.Lcd.setCursor(10,10);
      M5.Lcd.println("Connection Failed");
      M5.Lcd.setCursor(10,26);
      M5.Lcd.println("Error during connection");
      delay(1500);
    }
    doConnect = false;
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
  M5.Lcd.println("Looking for tag...");
  pBLEScan->start(5, false);

  while (M5.Touch.ispressed() == true){
    M5.Touch.update();
    delay(100);
  }
  currentMode = main;
}

void logFunct(){
  M5.Lcd.fillScreen(0xf7f7);
  M5.Lcd.setCursor(10,10);
  delay(2000);
  currentMode = main;
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
      pRemoteCharacteristic->writeValue("beeping");
      delay(100);
    }
    else {
      pRemoteCharacteristic->writeValue("noping");
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
  pRemoteCharacteristic->writeValue("noping");
  currentMode = main;
}

