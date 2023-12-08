#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <M5Core2.h>
#include <vector>
#include <Ticker.h>
#include <cmath>
#include <WiFi.h>
#include "soc/rtc_wdt.h"
//#include <ESP32Servo.h>
#include "hugefile.h"

//used for main menu navigation
enum mode{
  main = 0x0,
  BLE = 0x1,
  logMenu = 0x2,
  timeSet = 0x3,
  find = 0x4
};

//used to store actions to output in the log
struct info{
  std::string name;
  std::string address;
  std::string timestamp;
  std::string actionType;
};

//information about the servers. Set during connection, pulled for various functions.
//probably causes a small memory leak
class serverInfo{
  public:
  serverInfo(){
    service = nullptr;
    beepChar = nullptr;
    infoChar = nullptr;
    accChar = nullptr;
    address = nullptr;
    client = nullptr;
    name = {};
    connected = false;
  }
  ~serverInfo(){
    //I tried to make a destructor, but it kept crashing, and it would be shocking if a memory leak crashed the MCU before it ran out of battery
  }
  bool connected;
  BLEClient *client;
  std::string name;
  BLEAddress *address;
  BLERemoteService *service;
  BLERemoteCharacteristic *beepChar;
  //infoChar isn't really used, but I like it, so it gets to stay
  BLERemoteCharacteristic *infoChar;
  BLERemoteCharacteristic *accChar;
};
////////////////////////////////////
//Global Variables
enum mode currentMode;
//scanning and connection
static boolean doConnect = false;
static boolean doScan = false;
static BLEAdvertisedDevice* myDevice = NULL;
BLEScan* pBLEScan;
//for logging
std::vector<info> infoVector = {};
//server information
std::vector<serverInfo> myServers = {};
//used to update time. This MCU has an RTC, which I didn't know until I made the function. Oh well.
Ticker periodicTicker;
int currentHourTen = 0;
int currentHourOne = 0;
int currentMinuteTen = 0;
int currentMinuteOne = 0;
int currentSecondTen = 0;
int currentSecondOne = 0;
serverInfo *newServer = nullptr;
bool reconnectAttempt = false;

//Accelerometer data
float accX = 0.0F;
float accY = 0.0F;
float accZ = 0.0F;  

//Servo myservo;
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
//it's a tiny function but oh well
char intToChar(int);
//this is where the actual pinging happens
//also the display and backlight are turned off to conserve power
void customSleep();
//uh oh you tried to leave
void warning();
bool samesign(float, float);
///////////////////////////////
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    //connection handled in another function
  }

  void onDisconnect(BLEClient* pclient) {
    //update logs
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

    for(int i = 0; i<myServers.size(); i++){
      if (myServers[i].address->toString() == newInfo.address){
        myServers[i].connected = false;
      }
    }
  }
};
/////////////////////////////////////
//server connection
bool connectToServer() {
  if(reconnectAttempt == true){
    for (int i=0; i<myServers.size();i++){
      if (myServers[i].name == myDevice->getName()){
        myServers[i].client->connect(myDevice);
        myServers[i].connected = true;
        reconnectAttempt = false;

        struct info newInfo;
        newInfo.name = myServers[i].name;
        newInfo.actionType = "reconnect";
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
        newInfo.address = (myServers[i].address->toString());
        infoVector.push_back(newInfo);

        return true;
      }
    }
    printf("Error\n"); 
  }
  else{
  newServer = new serverInfo;  
  newServer->client = BLEDevice::createClient();
  newServer->client->setClientCallbacks(new MyClientCallback());

//Connect to the remove BLE Server.
  newServer->client->connect(myDevice);

  //pulls server info for the server vector
  std::string tempUUID = myDevice->getServiceUUID().toString();
  newServer->service = newServer->client->getService(tempUUID.c_str());
  newServer->name = myDevice->getName();
  newServer->address = new BLEAddress(newServer->client->getPeerAddress().toString());
  std::map<std::string, BLERemoteCharacteristic*> *serverMap = nullptr;
  serverMap = newServer->service->getCharacteristics();
  
  //identifying characteristics by initialized content
  for (std::map<std::string, BLERemoteCharacteristic*>::iterator it=serverMap->begin(); it!=serverMap->end();++it){
    bool foundInfo = true;
    std::string infoComp = "Device";
    if (it->second->readValue() == "Beep Characteristic"){
      newServer->beepChar = it->second;
    }
    else if (it->second->readValue() == "Not moving" or it->second->readValue() == "Moving"){
      newServer->accChar = it->second;
    }
    for (int i = 0; i<infoComp.length(); i++){
      if (it->second->readValue()[i] != infoComp[i]){
        foundInfo = false;
      }
    }
    if (foundInfo == true){
      newServer->infoChar = it->second;
    }
  }
  newServer->connected = true;
  myServers.push_back(*newServer);
  //for logging
  struct info newInfo;
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

  return true;
  }
}
//Scan for BLE servers and find the first one that advertises the name
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    bool found = true;
    if (reconnectAttempt == true){
      std::vector<std::string> names;
      for (int i=0; i<myServers.size();i++){
        if (myServers[i].connected == false){
          names.push_back(myServers[i].name);
        } 
      }
      for (int i=0; i<names.size();i++){
        found = true;
        for (int j = 0; j<names[i].length();j++){
          if (advertisedDevice.toString().c_str()[j+6] != names[i][j]){
            found = false;
            break;
          }
        }
        if (found == true){
          BLEDevice::getScan()->stop();
          myDevice = new BLEAdvertisedDevice(advertisedDevice);
          doConnect = true;
          doScan = true;
        }
      }
    }
    else {
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
      }
    }
  }
};
//////////////////////////////////////////////////
void setup() {
//general
  M5.begin(true, true, true, true, mbus_mode_t::kMBusModeOutput,true);
  //IMU
  M5.IMU.Init();
  //prevent wdt from crashing MPU when BLE takes too long
  rtc_wdt_set_time(RTC_WDT_STAGE0, 5000);
//display
  M5.Lcd.fillScreen(WHITE);
  M5.Lcd.setTextColor(BLACK);
  M5.Lcd.setTextSize(3);
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
  M5.Lcd.println("Setup successful");
  M5.Lcd.setCursor(10,34);
  M5.Lcd.println("Press to continue");
  while (M5.Touch.ispressed() == false){
    M5.Touch.update();
    delay(25);
  }

  currentMode = main;
}

int proxySecond = 0;
int timer = 0;

void loop() {
  
  TouchPoint_t pos;
  //wait for button to stop being pressed
  while (M5.Touch.ispressed() == true){
    M5.Touch.update();
    delay(25);
  }
  M5.Lcd.setTextSize(3);
  ///////////////////BLE connection
  if (doConnect == true) {
    if (connectToServer()) {
      M5.Lcd.fillScreen(GREEN);
      M5.Lcd.setCursor(10,10);
      M5.Lcd.println("Connected!");
      delay(1500);
    } 
    else {
      M5.Lcd.fillScreen(RED);
      M5.Lcd.setCursor(10,10);
      M5.Lcd.println("Connection Failed");
      M5.Lcd.setCursor(10,34);
      M5.Lcd.println("Error during connection");
      delay(1500);
    }
    doConnect = false;
  }
  switch(currentMode){
    case main:
    //draw main menu
    M5.Lcd.setTextSize(3);
    M5.Lcd.fillRect(0,0,161,120,0x24BD);
    M5.Lcd.fillRect(161,0,160,120,0xF1AA);
    M5.Lcd.fillRect(0,121,160,120,0xFFE8);
    M5.Lcd.fillRect(161,120,160,120,0x56E8);
    M5.Lcd.setCursor(10,10);
    M5.Lcd.printf("Manage");
    M5.Lcd.setCursor(10,34);
    M5.Lcd.printf("BLE");
    M5.Lcd.setCursor(170,10);
    M5.Lcd.printf("Find a");
    M5.Lcd.setCursor(170, 34);
    M5.Lcd.printf("device");
    M5.Lcd.setCursor(10,130);
    M5.Lcd.printf("View");
    M5.Lcd.setCursor(10,154);
    M5.Lcd.printf("device");
    M5.Lcd.setCursor(10,178);
    M5.Lcd.printf("logs");
    M5.Lcd.setCursor(170,130);
    M5.Lcd.printf("Set time");
    timer = 0;
    proxySecond = currentSecondTen*10+currentSecondOne;
    //wait for press
    //if the wait gets long enough, the MCU's display goes to sleep
    while (M5.Touch.ispressed() == false){
      
      if (proxySecond != currentSecondTen*10+currentSecondOne){
        proxySecond++;
        timer++;
      }
      if (timer == 10){
        customSleep();
        while (M5.Touch.ispressed()){
          delay(25);
          M5.Touch.update();
        }
        break;
      }  
      delay(25);
      M5.Touch.update();
    }
    
    pos = M5.Touch.getPressPoint();
    if (M5.Touch.ispressed() == false){
      currentMode = main;
    }
    else if ((pos.x<=160) and (pos.y<=120)){
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

  enum timeMode mode = hour;
  while(true){
    M5.Lcd.fillScreen(0xA6df);
    M5.Lcd.fillRect(15, 15, 145, 210, 0x4cda);
    M5.Lcd.fillRect(175, 15, 130, 98, 0x4cda);
    M5.Lcd.fillRect(175, 128, 130, 97, 0x4cda);
    M5.Lcd.fillTriangle(87,67,77,87,97,87,0x29b0);
    M5.Lcd.fillTriangle(77,166,87,186,97,166,0x29b0);
    M5.Lcd.setCursor(25,25);
    switch(mode){
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
    M5.Lcd.setCursor(179,52);
    M5.Lcd.println("Confirm");
    M5.Lcd.setCursor(185, 153);
    M5.Lcd.println("Change");
    M5.Lcd.setCursor(185, 177);
    M5.Lcd.println("Unit");
    
    while (M5.Touch.ispressed() == false){
      M5.Touch.update();
      delay(25);
    }

    pos = M5.Touch.getPressPoint();
    //up and down arrows
    if (pos.x <= 168){ 
      if (pos.y <= 120){
        if ((currentNumTen == 1) and (currentNumOne == 2) and (mode == hour)){
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
        if ((currentNumTen == 0) and (currentNumOne == 0) and (mode == hour)){
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
      switch(mode){
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
        switch(mode){
          default:
          case hour:
          currentHourTen = currentNumTen;
          currentHourOne = currentNumOne;
          mode = minute;
          break;
          case minute:
          currentMinuteTen = currentNumTen;
          currentMinuteOne = currentNumOne;
          mode = second;
          break;
          case second:
          currentSecondTen = currentNumTen;
          currentSecondOne = currentNumOne;
          mode = hour;
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
}


void BLEManage(){
  TouchPoint_t screenPos;
  while (true){
    //wait for scan to end, then kick back to submenu
    doScan = false;
    M5.Lcd.setTextSize(2);
    M5.Lcd.fillScreen(0x9e1e);
    M5.Lcd.fillRect(10, 10, 300, 80, 0x4cda);
    M5.Lcd.fillRect(10, 100, 300, 80, 0x4cda);
    M5.Lcd.fillRect(0,190,100,50,0xF800);
    M5.Lcd.setCursor(8,207);
    M5.Lcd.println("Go back");
    M5.Lcd.setTextSize(3);
    M5.Lcd.setCursor(20,20);
    M5.Lcd.print("Pair with new");
    M5.Lcd.setCursor(20,44);
    M5.Lcd.print("tag");
    M5.Lcd.setCursor(20, 110);
    M5.Lcd.print("View connected");
    M5.Lcd.setCursor(20,134);
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
    if (screenPos.y<=95){
      M5.Lcd.fillScreen(0x9e1e);
      M5.Lcd.setCursor(10,10);
      M5.Lcd.print("Looking for tag..");
      reconnectAttempt = false;
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
        M5.Lcd.fillTriangle(280,52,270,72,290,72,0x500d);
        M5.Lcd.fillTriangle(270,152,290,152,280,172,0x500d);
        M5.Lcd.setCursor(8,207);
        M5.Lcd.setTextSize(2);
        M5.Lcd.println("Go back");
        M5.Lcd.setCursor(10,10);
        M5.Lcd.println("Name, Address");
        int temp;
        temp = myServers.size();
        if (temp>3){
          temp=3;
        }
        for (int i = 0; i < myServers.size(); i++){
          M5.Lcd.setCursor(10,42+48*i);
          M5.Lcd.print(myServers[i].name.c_str());
          M5.Lcd.setCursor(10,58+48*i);
          M5.Lcd.print(myServers[i].address->toString().c_str());
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
  TouchPoint_t screenPos;
  int scrollFactor = 0;
  if (infoVector.size() == 0){
    currentMode = main;
    return;
  }

  while (true){
    M5.Lcd.fillScreen(0xf7f7);
    M5.Lcd.fillTriangle(280,70,270,90,290,90,0x500d);
    M5.Lcd.fillTriangle(270,170,290,170,280,190,0x500d);
    M5.Lcd.fillRect(0,210,100,30,0xF800);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(8,217);
    M5.Lcd.println("Go back");
    M5.Lcd.setCursor(10,10);
    M5.Lcd.print("Name, address, action,");
    M5.Lcd.setCursor(10,26);
    M5.Lcd.print("timestamp");
    if (infoVector.size() == 1){
      M5.Lcd.setCursor(10,58);
      M5.Lcd.print(infoVector[0].name.c_str());
      M5.Lcd.setCursor(10,74);
      M5.Lcd.print(infoVector[0].address.c_str());
      M5.Lcd.setCursor(10,90);
      M5.Lcd.print(infoVector[0].actionType.c_str());
      M5.Lcd.setCursor(10,106);
      M5.Lcd.println(infoVector[0].timestamp.c_str());
    }
    else {
      for(int i = scrollFactor; i<scrollFactor+2; i++){
          M5.Lcd.setCursor(10,58+80*(i-scrollFactor));
          M5.Lcd.print(infoVector[i].name.c_str());
          M5.Lcd.setCursor(10,74+80*(i-scrollFactor));
          M5.Lcd.print(infoVector[i].address.c_str());
          M5.Lcd.setCursor(10,90+80*(i-scrollFactor));
          M5.Lcd.print(infoVector[i].actionType.c_str());
          M5.Lcd.setCursor(10,106+80*(i-scrollFactor));
          M5.Lcd.println(infoVector[i].timestamp.c_str());
      }
    }
    while (M5.Touch.ispressed() == true){
      M5.Touch.update();
      delay(25);
    }
    while (M5.Touch.ispressed() == false){
      M5.Touch.update();
      delay(25);
    }
    screenPos = M5.Touch.getPressPoint();
    if ((screenPos.x<100) and (screenPos.y>210)){
      break;
    }
    else if (screenPos.y <= 130){
      if (scrollFactor != 0){
        scrollFactor--;
      }
    }
    else if(screenPos.y <= 190) {
      if (scrollFactor != infoVector.size()-2){
        scrollFactor++;
      }
    }
  }
  currentMode = main;
}
//////////////////////////////////////////

///////////////////////////////////////
//findDevice
void findDevice(){
  int scrollFactor = 0;
  if (myServers.begin() == myServers.end()){
    currentMode = main;
    return;
  }
  TouchPoint_t screenPos;
  bool menu = true;
  bool beeping = false;
  int selected = 99;
  //the submenu here is pretty dirty. You can fix it if you REALLY want to
  while (true){
    //draw screen and fish input
    if (menu == true){
      M5.Lcd.fillScreen(0xe53f);
      M5.Lcd.fillRect(0,190,100,50,0xF800);
      M5.Lcd.fillTriangle(280,54,270,74,290,74,0x500d);
      M5.Lcd.fillTriangle(270,154,290,154,280,174,0x500d);
      M5.Lcd.setTextSize(2);
      M5.Lcd.setCursor(10,10);
      M5.Lcd.print("Select a device to find");
      M5.Lcd.setCursor(10,26);
      M5.Lcd.print("Format: Name, Address");
      M5.Lcd.setCursor(8,207);
      M5.Lcd.println("Go back");

      if (myServers.size() == 1){
        M5.Lcd.setCursor(10,58);
        M5.Lcd.print(myServers[0].name.c_str());
        M5.Lcd.setCursor(10,74);
        M5.Lcd.print(myServers[0].address->toString().c_str());
      }
      else {
        for(int i = scrollFactor; i<scrollFactor+2; i++){
          M5.Lcd.setCursor(10,58+48*(i-scrollFactor));
          M5.Lcd.print(myServers[i].name.c_str());
          M5.Lcd.setCursor(10,74+48*(i-scrollFactor));
          M5.Lcd.print(myServers[i].address->toString().c_str());
        }
      }
    }
    //fish for input
    while (M5.Touch.ispressed() == true){
      M5.Touch.update();
      delay(25);
    }
    while (M5.Touch.ispressed() == false){
      M5.Touch.update();
      delay(25);
    }
    screenPos = M5.Touch.getPressPoint();
    //back button behavior in nested menu
    if ((screenPos.x<=100) and (screenPos.y>190)){
      if (menu == false){
        menu = true;
        myServers[selected].beepChar->writeValue("noping");
        delay(200);
        selected = 99;
        scrollFactor = 0;
        beeping = false;
      }
      //back button behavior in menu
      else {
        break;
      }
    }
    //down arrow
    else if (screenPos.y <= 90 and screenPos.y > 58 and screenPos.x > 240 and menu == true){
      if (scrollFactor != 0){
        scrollFactor--;
      }
    }
    //up arrow
    else if(screenPos.y <= 190 and screenPos.y > 158 and screenPos.x > 240 and menu == true) {
      if (scrollFactor != myServers.size()-2){
        scrollFactor++;
      }
    }
    //first server
    else if (screenPos.y <= 95 and screenPos.y > 54 and menu == true){
      selected = scrollFactor;
    }
    //second server
    else if (screenPos.y <= 142 and screenPos.y > 101 and myServers.size() != 1 and menu == true){
      selected = scrollFactor+1;
    }
    else if (menu == false){
      beeping = !beeping;
    }
    //add log
    if (screenPos.x <= 240 and menu == true and selected != 99){
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
      menu = false;
    }
    M5.Lcd.setCursor(10, 10);
    if (beeping == true and menu == false){
      //send beep
      myServers[selected].beepChar->writeValue("beeping");
      M5.Lcd.fillScreen(GREEN);
      M5.Lcd.setTextSize(3);
      M5.Lcd.printf("Beeping!");
      //back button
      M5.Lcd.fillRect(0,190,100,50,0xF800);
      M5.Lcd.setCursor(8,207);
      M5.Lcd.setTextSize(2);
      M5.Lcd.println("Go back");
    }
    else if (menu == false){
      myServers[selected].beepChar->writeValue("noping");
      M5.Lcd.fillScreen(WHITE);
      M5.Lcd.setTextSize(3);
      M5.Lcd.printf("Press to beep");
      M5.Lcd.setCursor(10,34);
      M5.Lcd.printf("other device");
      //back button
      M5.Lcd.fillRect(0,190,100,50,0xF800);
      M5.Lcd.setCursor(8,207);
      M5.Lcd.setTextSize(2);
      M5.Lcd.println("Go back");
      
    }
    
  }
  currentMode = main;
}

void customSleep(){
  int connectionCheck = 0;
  int disconnected = 0;
  int movingServers = 0;
  float xCalib, yCalib, zCalib;
  float prevX, prevY, prevZ;
  delay(50);
  M5.IMU.getAccelData(&accX, &accY, &accZ);
  xCalib = accX;
  yCalib = accY;
  zCalib = accZ;

  //turn screen off
  M5.Lcd.sleep();
  M5.Axp.SetLcdVoltage(2500);
  while (true){
    
    delay(50);
    if (doConnect == true) {
      if (connectToServer()) {
        delay(300);
      } 
      else {
        M5.Lcd.wakeup();
        M5.Axp.SetLcdVoltage(2800);
        M5.Lcd.fillScreen(RED);
        M5.Lcd.setCursor(10,10);
        M5.Lcd.println("Connection Failed");
        M5.Lcd.setCursor(10,34);
        M5.Lcd.println("Error during connection");
        delay(1500);
        doConnect = false;
      } 
  }
    movingServers = 0;
    disconnected = 0;
    reconnectAttempt = false;
    M5.update();
    //check how many servers have moved in the past 10 seconds
    for (int i=0; i<myServers.size();i++){
      if (myServers[i].connected == false){
        disconnected++;
        connectionCheck++;
      }
      else if (myServers[i].accChar->readValue() == "Moving"){
        movingServers++;
        
      }
    }
    if (disconnected > 0){
      if(connectionCheck/disconnected >= 20){
        doScan = false;
        connectionCheck = 0;
        reconnectAttempt = true;
        pBLEScan->setActiveScan(true);
        pBLEScan->start(5, false);
      }
    }
    //check if the client is moving
    prevX = accX;
    prevY = accY;
    prevZ = accZ;
    M5.IMU.getAccelData(&accX, &accY, &accZ);
    /*
    Serial.println(accX-xCalib, 3);
    Serial.println(accY-yCalib, 3);
    Serial.println(accZ-zCalib, 3);
    Serial.println("");
    Serial.println(sqrt(pow(accX-xCalib,2.0)+pow(accZ-zCalib,2.0)));
    Serial.println("");
    Serial.println("");
    */
    if ((sqrt(pow(accX-xCalib, 2.0)+pow(accZ-zCalib,2.0))) >= 0.12){
      if ((abs(accX-xCalib)>=sqrt(0.0144-pow(accZ-zCalib,2.0)) and samesign(accX-xCalib, prevX-xCalib)) or (abs(accZ-zCalib)>=sqrt(0.0144-pow(accX-xCalib,2.0)) and samesign(accZ-zCalib, prevZ-zCalib))){
        //Serial.println("Moving!");
        if (movingServers != myServers.size()){
          warning();
          M5.IMU.getAccelData(&accX, &accY, &accZ);
          prevX = accX;
          prevY = accY;
          prevZ = accZ;
          xCalib = accX;
          yCalib = accY;
          zCalib = accZ;
          delay(50);
          M5.IMU.getAccelData(&accX, &accY, &accZ);
        }
      }
    }
    M5.update();
    delay(25);
    if (M5.Touch.ispressed()){
      break;
    }
  }
  M5.Lcd.wakeup();
  M5.Axp.SetLcdVoltage(2800);
}
void warning(){
  //logging
  struct info newInfo;
  newInfo.name = "This Device";
  newInfo.actionType = "warning";
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
  newInfo.address = WiFi.macAddress().c_str();
  infoVector.push_back(newInfo);
  //warning stuff
  M5.Lcd.wakeup();
  M5.Axp.SetLcdVoltage(2800);
  M5.Lcd.setTextSize(3);
  for (int i = 0;i<10;i++){
    M5.Lcd.fillScreen(WHITE);
    M5.Lcd.setCursor(10,10);
    M5.Lcd.print("Warning! Collect");
    M5.Lcd.setCursor(10,34); 
    M5.Lcd.print("all tracked items");
    M5.Lcd.setCursor(10,58);
    M5.Lcd.print("before departing!");
    M5.Lcd.setCursor(10,106);
    M5.Lcd.print("Touch to stop");
    M5.update();
    if (M5.Touch.ispressed()){
      break;
    }
    M5.Spk.PlaySound(previewR, sizeof(previewR));
    M5.Lcd.fillScreen(RED);
    M5.Lcd.setCursor(10,10);
    M5.Lcd.print("Warning! Collect");
    M5.Lcd.setCursor(10,34); 
    M5.Lcd.print("all tracked items");
    M5.Lcd.setCursor(10,58);
    M5.Lcd.print("before departing!");
    M5.Lcd.setCursor(10,106);
    M5.Lcd.print("Touch to stop");
    delay(1000);
    M5.update();
    if (M5.Touch.ispressed()){
      break;
    }
  }
  //waits to stop moving
  delay(5000);
  //turns display back off
  M5.Lcd.sleep();
  M5.Axp.SetLcdVoltage(2500);
}

char intToChar(int n){
  return (n + '0');
}

bool samesign(float a, float b){
  return (a>=0 and b>= 0) or (a<0 and b<0);
}