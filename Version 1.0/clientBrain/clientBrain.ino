#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <M5Core2.h>
#include <vector>
#include <Ticker.h>
////////////////////////////////////
/////*******Note: No scroll bars in menus of arbitrary length yet
//*************And: Find device's front end is not fully developed yet
//Definitions

//old single tag services
//static BLEUUID serviceUUID("9f88320c-0223-402b-96d0-589e6b5ae335");

//static BLEUUID    charUUID("5198f135-81ca-4e28-a2c8-5afdc1ee7141");

//static BLEUUID INFO_CHARACTERISTIC_UUID("9d32b8fb-9184-4cae-9d1b-d385abce4c58");

enum mode{
  main = 0x0,
  BLE = 0x1,
  logMenu = 0x2,
  timeSet = 0x3,
  find = 0x4
};

struct info{
  std::string name;
  std::string address;
  std::string timestamp;
  std::string actionType;
};

class serverInfo{
  public:
  serverInfo(){
    service = nullptr;
    beepChar = nullptr;
    infoChar = nullptr;
    address = nullptr;
    client = nullptr;
    name = {};
  }
  ~serverInfo(){
    //I have no idea if this empty destructor causes a memory leak. I tried
    //using the below code and it gave me a bug I couldn't figure out.
    /*
    if (service != nullptr){
    delete[] service;
    }
    if (beepChar != nullptr){
    delete[] beepChar;
    }
    if (infoChar != nullptr){
    delete[] infoChar;
    }
    if (address != nullptr){
    delete[] address;
    }
    address = nullptr;
    service = nullptr;
    beepChar = nullptr;
    infoChar = nullptr;
    client = nullptr;
    */
  }
  BLEClient *client;
  std::string name;
  BLEAddress *address;
  BLERemoteService *service;
  BLERemoteCharacteristic *beepChar;
  BLERemoteCharacteristic *infoChar;
};
////////////////////////////////////
//Global Variables
enum mode currentMode;
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
//to be deleted
//static BLERemoteCharacteristic* pRemoteCharacteristic = NULL;
//static BLERemoteCharacteristic* infoCharacteristic = NULL;
static BLEAdvertisedDevice* myDevice = NULL;
BLEScan* pBLEScan;
std::vector<info> infoVector = {};
//keep track of multiple tags
std::vector<serverInfo> myServers = {};
Ticker periodicTicker;
int currentHourTen = 0;
int currentHourOne = 0;
int currentMinuteTen = 0;
int currentMinuteOne = 0;
int currentSecondTen = 0;
int currentSecondOne = 0;
serverInfo *newServer = nullptr;
////////////////////////////////////
//Functions
//sets time
void setTime();
//Scans for and automatically adds devices by name
void BLEManage();
//finds a device by beeping it
void findDevice();
//allows you to see logs
void logFunct();
//increments time
void updateTime();
//convert int to char*
char intToChar(int);
///////////////////////////////
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    //connection handled in another function
  }

  void onDisconnect(BLEClient* pclient) {
    Serial.println("onDisconnect");
    struct info newInfo;
    newInfo.name = "Unknown";
    for (int i = 0; i<infoVector.size(); i++){
      if (infoVector[i].address == pclient->getPeerAddress().toString()){
        newInfo.name = infoVector[i].name;
        break;
      }
    }
    
    newInfo.actionType = "disconnect";
    std::string temp;
    temp = intToChar(currentHourTen);
    temp = temp + intToChar(currentHourOne);
    temp = temp + ":";
    temp = temp + intToChar(currentMinuteTen);
    temp = temp + intToChar(currentMinuteOne);
    temp = temp + ":";
    temp = temp + intToChar(currentSecondTen);
    temp = temp + intToChar(currentSecondOne);
    newInfo.timestamp = (temp);
    newInfo.address = pclient->getPeerAddress().toString();
    infoVector.push_back(newInfo);

    //remove server from list
    for(int i = 0; i<myServers.size(); i++){
      if (myServers[i].address->toString() == newInfo.address){
        myServers[i] = myServers.back();
        myServers.pop_back();
      }
    }
    connected = false;
  }
};
/////////////////////////////////////
//server connection
bool connectToServer() {
  newServer = new serverInfo;
  Serial.print("Forming a connection to ");
  Serial.println(myDevice->getAddress().toString().c_str());
    
  newServer->client = BLEDevice::createClient();
  Serial.println(" - Created client");

  newServer->client->setClientCallbacks(new MyClientCallback());
      
    
    

    // Connect to the remove BLE Server.
  newServer->client->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
  Serial.println(" - Connected to server");

    //update vector for multi tag support
    
  std::string tempUUID = myDevice->getServiceUUID().toString();
  newServer->service = newServer->client->getService(tempUUID.c_str());
  newServer->name = myDevice->getName();
  newServer->address = new BLEAddress(newServer->client->getPeerAddress().toString());
    //add characteristics
  std::map<std::string, BLERemoteCharacteristic*> *serverMap = nullptr;
  serverMap = newServer->service->getCharacteristics();
    
  for (std::map<std::string, BLERemoteCharacteristic*>::iterator it=serverMap->begin(); it!=serverMap->end();++it){
    bool foundInfo = true;
    std::string infoComp = "Device";
    if (it->second->readValue() == "Beep Characteristic"){
      newServer->beepChar = it->second;
    }
    for (int i = 0; i<infoComp.length(); i++){
      if (it->second->readValue()[i] != infoComp[i]){
        foundInfo = false;
      }
    }
    if (foundInfo = true){
      newServer->infoChar = it->second;
    }
  }
  myServers.push_back(*newServer);
  //for logging
  struct info newInfo;
  //note to future me: Output part of this line to the console for diagnostics
  Serial.println(myServers.back().name.c_str());
  newInfo.name = myServers.back().name;
  newInfo.actionType = "connect";
  std::string temp;
  temp = intToChar(currentHourTen);
  temp = temp + intToChar(currentHourOne);
  temp = temp + ":";
  temp = temp + intToChar(currentMinuteTen);
  temp = temp + intToChar(currentMinuteOne);
  temp = temp + ":";
  temp = temp + intToChar(currentSecondTen);
  temp = temp + intToChar(currentSecondOne);
  newInfo.timestamp = temp;
  newInfo.address = (myServers.back().address->toString());
  infoVector.push_back(newInfo);

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
    delay(25);
  }

  currentMode = main;
}


void loop() {
  TouchPoint_t pos;
  //wait for button to stop being pressed
  while (M5.Touch.ispressed() == true){
    M5.Touch.update();
    delay(25);
  }
  M5.Lcd.setTextSize(2);
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
    M5.Lcd.printf("Set time");
    //wait for press
    while (M5.Touch.ispressed() == false){
      M5.Touch.update();
      delay(25);
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
      currentMode = timeSet;
    }
    break;
    case BLE:
      while (M5.Touch.ispressed() == true){
        M5.Touch.update();
        delay(25);
      }
      BLEManage();
    break;
    case find:
      while (M5.Touch.ispressed() == true){
        M5.Touch.update();
        delay(25);
      }
      findDevice();
    break;
    case logMenu:
      while (M5.Touch.ispressed() == true){
        M5.Touch.update();
        delay(25);
      }
      logFunct();
    break;
    case timeSet:
    //**************I'm not really sure what light sleep does
    setTime();
  }
}
//////////////////////////////////////////
void setTime(){
  TouchPoint_t pos;
  periodicTicker.detach();
  
  int currentNumTen = 0;
  int currentNumOne = 0;

  enum timeMode{
    hour,
    minute,
    second
  };

  enum timeMode timeMode = hour;
  while(true){
    M5.Lcd.fillScreen(0xA6df);
    M5.Lcd.fillRect(15, 15, 145, 210, 0x4cda);
    M5.Lcd.fillRect(175, 15, 130, 98, 0x4cda);
    M5.Lcd.fillRect(175, 128, 130, 97, 0x4cda);
    M5.Lcd.fillTriangle(87,67,77,87,97,87,0x29b0);
    M5.Lcd.fillTriangle(77,166,87,186,97,166,0x29b0);
    M5.Lcd.setCursor(25,25);
    switch(timeMode){
      case hour:
      default:
      M5.Lcd.printf("Hour");
      M5.Lcd.setCursor(25,120);
      M5.Lcd.print(currentHourTen);
      M5.Lcd.print(currentHourOne);
      break;
      case minute:
      M5.Lcd.printf("Minute");
      M5.Lcd.setCursor(25,120);
      M5.Lcd.print(currentMinuteTen);
      M5.Lcd.print(currentMinuteOne);
      break;
      case second:
      M5.Lcd.printf("Second");
      M5.Lcd.setCursor(25,120);
      M5.Lcd.print(currentSecondTen);
      M5.Lcd.print(currentSecondOne);
      break;
    }
    M5.Lcd.setCursor(185,64);
    M5.Lcd.println("Confirm");
    M5.Lcd.setCursor(185, 177);
    M5.Lcd.println("Change");
    M5.Lcd.setCursor(185, 193);
    M5.Lcd.println("Unit");
    
    while (M5.Touch.ispressed() == false){
      M5.Touch.update();
      delay(25);
    }

    pos = M5.Touch.getPressPoint();
    //up and down arrows
    if (pos.x <= 168){ 
      if (pos.y <= 120){
        if ((currentNumTen == 1) and (currentNumOne == 2) and (timeMode == hour)){
          currentNumTen = 0;
          currentNumOne = 0;
        }
        else if ((currentNumTen == 5) and (currentNumOne == 9)){
          currentNumTen = 0;
          currentNumOne = 0;
        }
        else if (currentNumOne == 9){
          currentNumTen++;
          currentNumOne = 0;
        }
        else {
          currentNumOne++;
        }
      }
      else {
        if ((currentNumTen == 0) and (currentNumOne == 0) and (timeMode == hour)){
          currentNumTen = 1;
          currentNumOne = 2;
        }
        else if ((currentNumTen == 0) and (currentNumOne == 0)){
          currentNumTen = 5;
          currentNumOne = 9;
        }
        else if (currentNumOne == 0){
          currentNumTen--;
          currentNumOne = 9;
        }
        else {
          currentNumOne--;
        }
      }
      switch(timeMode){
        default:
        case hour:
        currentHourTen = currentNumTen;
        currentHourOne = currentNumOne;
        break;
        case minute:
        currentMinuteTen = currentNumTen;
        currentMinuteOne = currentNumOne;
        break;
        case second:
        currentSecondTen = currentNumTen;
        currentSecondOne = currentNumOne;
        break;
      }
    }
    if (pos.x > 168){
      if (pos.y <= 120){
        periodicTicker.attach_ms(1000, updateTime);
        currentMode = main;
        break;
      }
      else {
        switch(timeMode){
          default:
          case hour:
          currentHourTen = currentNumTen;
          currentHourOne = currentNumOne;
          timeMode = minute;
          break;
          case minute:
          currentMinuteTen = currentNumTen;
          currentMinuteOne = currentNumOne;
          timeMode = second;
          break;
          case second:
          currentSecondTen = currentNumTen;
          currentSecondOne = currentNumOne;
          timeMode = hour;
          break;
        }
        currentNumTen = 0;
        currentNumOne = 0;
      }
    }
    //wait to let go of button
    M5.Touch.update();
    while (M5.Touch.ispressed() == true){
      M5.Touch.update();
      delay(25);
    }
  }
}


void updateTime(){
  //test for connection
  //don't need?
  /*
  for (int i = 0; i< myServers.size(); i++){
    if (myServers[i].infoChar != NULL){
      if (myServers[i].service->getClient()->isConnected() == false){
        Serial.println("A");
        myServers[i].service->getClient()->disconnect();
        Serial.println("B");
      }
    }
  }
  */
  if ((currentHourTen == 1) and (currentHourOne == 2) and (currentMinuteTen == 5) and (currentMinuteOne == 9) and (currentSecondTen == 5) and (currentSecondOne == 9)){
    currentHourTen = 0;
    currentHourOne = 0;
    currentMinuteTen = 0;
    currentMinuteOne = 0;
    currentSecondTen = 0;
    currentSecondOne = 0;
  }
  else if ((currentHourOne == 9) and (currentMinuteTen == 5) and (currentMinuteOne == 9) and (currentSecondTen == 5) and (currentSecondOne == 9)){
    currentHourTen = 1;
    currentHourOne = 0;
    currentMinuteTen = 0;
    currentMinuteOne = 0;
    currentSecondTen = 0;
    currentSecondOne = 0;
  }
  else if ((currentMinuteTen == 5) and (currentMinuteOne == 9) and (currentSecondTen == 5) and (currentSecondOne == 9)){
    currentHourOne++;
    currentMinuteTen = 0;
    currentMinuteOne = 0;
    currentSecondTen = 0;
    currentSecondOne = 0;
  }
  else if ((currentMinuteOne == 9) and (currentSecondTen == 5) and (currentSecondOne == 9)){
    currentMinuteTen++;
    currentMinuteOne = 0;
    currentSecondTen = 0;
    currentSecondOne = 0;
  }
  else if ((currentSecondTen == 5) and (currentSecondOne == 9)){
    currentMinuteOne++;
    currentSecondTen = 0;
    currentSecondOne = 0;
  }
  else if (currentSecondOne == 9){
    currentSecondTen++;
    currentSecondOne = 0;
  }
  else {
    currentSecondOne++;
  }
  /*
  Serial.print(currentHourTen);
  Serial.print(currentHourOne);
  Serial.print(":");
  Serial.print(currentMinuteTen);
  Serial.print(currentMinuteOne);
  Serial.print(":");
  Serial.print(currentSecondTen);
  Serial.println(currentSecondOne);
  */
}


void BLEManage(){
  TouchPoint_t screenPos;
  while (true){
    //wait for scan to end, then kick back to submenu
    doScan = false;
    M5.Lcd.setTextSize(2);
    M5.Lcd.fillScreen(0x9e1e);
    M5.Lcd.fillRect(15, 15, 138, 98, 0x4cda);
    M5.Lcd.fillRect(167, 15, 138, 98, 0x4cda);
    M5.Lcd.fillRect(0,190,100,50,0xF800);
    M5.Lcd.setCursor(8,207);
    M5.Lcd.println("Go back");
    M5.Lcd.setCursor(25,25);
    M5.Lcd.print("Pair with");
    M5.Lcd.setCursor(25,41);
    M5.Lcd.print("new tag");
    M5.Lcd.setCursor(177, 25);
    M5.Lcd.print("View");
    M5.Lcd.setCursor(177,41);
    M5.Lcd.print("connected");
    M5.Lcd.setCursor(177,57);
    M5.Lcd.print("tags");
    //wait to let go of button
    while (M5.Touch.ispressed() == true){
      M5.Touch.update();
      delay(25);
    }
  //wait to press button
    while (M5.Touch.ispressed() == false){
      M5.Touch.update();
      delay(25);
    }
    screenPos = M5.Touch.getPressPoint();
    //go back to main menu
    if ((screenPos.x<100) and (screenPos.y>190)){
      break;
    }
    if (screenPos.x<=160){
      M5.Lcd.fillScreen(0x9e1e);
      M5.Lcd.setCursor(10,10);
      M5.Lcd.print("Looking for tag...");
      pBLEScan->start(5, false);
      if (doScan == true){
        break;
      }
    }
    //nested menu option
    else {
      while (true) {
        M5.Lcd.fillScreen(0x9e1e);
        M5.Lcd.fillRect(0,190,100,50,0xF800);
        M5.Lcd.setCursor(8,207);
        M5.Lcd.println("Go back");
        M5.Lcd.setCursor(10,10);
        M5.Lcd.setTextSize(1);
        M5.Lcd.println("Name, Address, RSSI");
        for (int i = 0; i < myServers.size(); i++){
          M5.Lcd.setCursor(10,26+16*i);
          M5.Lcd.print(myServers[i].name.c_str());
          M5.Lcd.print(", ");
          M5.Lcd.print(myServers[i].address->toString().c_str());
          M5.Lcd.print(", ");
          M5.Lcd.print(myServers[i].service->getClient()->getRssi());
          M5.Lcd.println(" ");
        }
        while (M5.Touch.ispressed() == true){
          M5.Touch.update();
          delay(25);
        }
        //wait to press button
        while (M5.Touch.ispressed() == false){
          M5.Touch.update();
          delay(25);
        }
        screenPos = M5.Touch.getPressPoint();
        if ((screenPos.x<100) and (screenPos.y>190)){
          break;
        }
      }
    }
  }
  currentMode = main;
}

void logFunct(){
  M5.Lcd.fillScreen(0xf7f7);
  M5.Lcd.setCursor(10,10);
  M5.Lcd.setTextSize(1);
  M5.Lcd.println("Name, address, action, timestamp");
  for(int i = 0; i<infoVector.size(); i++){
    M5.Lcd.setCursor(10,(26+16*i));
    M5.Lcd.print(infoVector[i].name.c_str());
    M5.Lcd.print(", ");
    M5.Lcd.print(infoVector[i].address.c_str());
    M5.Lcd.print(", ");
    M5.Lcd.print(infoVector[i].actionType.c_str());
    M5.Lcd.print(", ");
    M5.Lcd.println(infoVector[i].timestamp.c_str());
    M5.Lcd.println(" ");
  }
  while (M5.Touch.ispressed() == true){
    M5.Touch.update();
    delay(25);
  }
  while (M5.Touch.ispressed() == false){
    M5.Touch.update();
    delay(25);
  }
  currentMode = main;
}
//////////////////////////////////////////

///////////////////////////////////////
//findDevice
void findDevice(){
  if (myServers.begin() == myServers.end()){
    currentMode = main;
    return;
  }
  TouchPoint_t screenPos;
  bool newPress = true;
  bool beeping = false;
  bool exit = false;
  int selected = 0;
  //expand upon this frontend
  //also add back button here
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
    delay(25);
  }
  screenPos = M5.Touch.getPressPoint();
  //go back to main menu
  if ((screenPos.x<100) and (screenPos.y>190)){
    currentMode = main;
    return;
  }

  struct info newInfo;
  newInfo.name = "Unknown";
  newInfo.address = "Unknown address";
  if (myServers[selected].infoChar != NULL){
    newInfo.address = myServers[selected].address->toString();
    for (int i = 0; i<infoVector.size(); i++){
      if (infoVector[i].address == newInfo.address){
        newInfo.name = infoVector[i].name;
        }
      }
    }
  
  newInfo.actionType = "beep";
  std::string temp;
  temp = intToChar(currentHourTen);
  temp = temp + intToChar(currentHourOne);
  temp = temp + ":";
  temp = temp + intToChar(currentMinuteTen);
  temp = temp + intToChar(currentMinuteOne);
  temp = temp + ":";
  temp = temp + intToChar(currentSecondTen);
  temp = temp + intToChar(currentSecondOne);
  newInfo.timestamp = (temp);
  infoVector.push_back(newInfo);

  while (true){
    screenPos = M5.Touch.getPressPoint();
    //go back to main menu
    if ((screenPos.x<100) and (screenPos.y>190)){
      break;
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
      myServers[selected].beepChar->writeValue("beeping");
    }
    else {
      myServers[selected].beepChar->writeValue("noping");
      M5.Lcd.fillScreen(WHITE);
      M5.Lcd.printf("Press to beep other");
      M5.Lcd.setCursor(10,26);
      M5.Lcd.printf("device");
      //back button
      M5.Lcd.fillRect(0,190,100,50,0xF800);
      M5.Lcd.setCursor(8,207);
      M5.Lcd.println("Go back");
      
    }
    beeping = !beeping;
    //wait to let go of button
    while (M5.Touch.ispressed() == true){
      M5.Touch.update();
      delay(25);
    }
    //wait to press button
    while (M5.Touch.ispressed() == false){
      M5.Touch.update();
      delay(25);
    }
  }
  myServers[selected].beepChar->writeValue("noping");
  currentMode = main;
}

char intToChar(int n){
  return (n + '0');
}