#include <ArduinoWebsockets.h>
#include <WiFi.h>
#include <Arduino_JSON.h>
#include "Stepper_28BYJ_48.h"
#include "time.h"

const char* uuid = "1234";

const int stepsPerRevolution = 2048;

Stepper_28BYJ_48 stepper(16, 17, 18, 19);

const int rotateAngle = stepsPerRevolution / 4;
bool isLocked;
const int openBtnOutPin = 25;
const int openBtnInPin = 26;

const char* ssid = ""; //WiFi SSID HERE
const char* password = ""; //WiFi password here
const char* websockets_server = "ws://socket.thezoot3.com";

const char* ntpServer = "203.248.240.140";
const long gmtOffset_sec = 3600 * 9; // 대한민국의 GMT/UTC 오프셋 (KST: UTC+9)
const int daylightOffset_sec = 0;     // Daylight Saving Time이 적용되지 않음
unsigned long epochTime; 

using namespace websockets;
WebsocketsClient client;

const int trigPin = 33;
const int echoPin = 32;

float closeableDistance = 3;
bool isClosing = false;
bool closedByExplict = false;

JSONVar classPeriod;
JSONVar mobileClass;

JSONVar wsType;

bool onScheduled = true;

bool isInit = false;

void setup() {
    pinMode(trigPin, OUTPUT);
	  pinMode(echoPin, INPUT);
    pinMode(openBtnInPin, INPUT);
    pinMode(openBtnOutPin, OUTPUT);
    Serial.begin(115200);
    WiFi.setSleep(true);
    WiFi.begin(ssid, password);
    for(int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println("Connected");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    //stepper.setSpeed(5);
    wsType["LOCKER_OPEN"] = "LOCKER_OPEN";
    wsType["LOCKER_CLOSE"] = "LOCKER_CLOSE";
    wsType["RES_MOBILE_CLASS"] = "RES_MOBILE_CLASS";
    wsType["RES_TIMEPERIOD"] = "RES_TIMEPERIOD";
    wsType["REQ_SYNC"] = "REQ_SYNC";
    wsType["LOCKER_ON_SCHEDULE"] = "LOCKER_ON_SCHEDULE";
    wsType["LOCKER_OFF_SCHEDULE"] = "LOCKER_OFF_SCHEDULE";
    /*while(digitalRead(openBtnInPin) != HIGH) {
      stepper.step(stepsPerRevolution / 120);
      delay(10);
    }*/
    stepper.step(512);
    isLocked = false;
    stepper.step(-stepsPerRevolution / 60);
    client.onMessage(onMessageCallback);
    client.onEvent(onEventsCallback);
    client.connect(websockets_server);
}

void getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  }
  time(&now);
  epochTime = now;
  Serial.println(epochTime);
}

void sendWsMessage(const char* type, JSONVar data = JSON.parse("{}")) {
  JSONVar message;
  message["type"] = type;
  message["data"]["uuid"] = uuid;
  Serial.println(JSON.stringify(data));
  if(data["index"] != null) {
    message["data"][data["index"]] = data["data"];
  }
  Serial.println(JSON.stringify(message));
  client.send(JSON.stringify(message));
}

void locker_init() {
  classPeriod = JSON.parse("[]");
  mobileClass = JSON.parse("[]");
  sendWsMessage("REQ_MOBILE_CLASS");
  sendWsMessage("RES_SYNC");
}

void loop() {	
  client.poll();
  /*static unsigned long previousMillis = 0;
  const unsigned long interval = 50000;
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    if(mobileClass.length() > 0 && classPeriod.length() > 0) {
      if(onScheduled) {
        getTime();
        int nonInSchedule = 0;
        for(int i = 0; i < classPeriod.length(); i++) {
          JSONVar period = classPeriod[i];
          if(epochTime > (long) period["start"]) {
            if(epochTime < (long) period["end"]) {
              if(!isLocked) {
                close();  
                closedByExplict = false;
                return;
              }
            } else {
              nonInSchedule++;
              continue;
            }
          } else {
            nonInSchedule++;
            continue;
          }
        }
        if(nonInSchedule == classPeriod.length() && !isLocked) {
            open();
            return;
        }
      } else if(isLocked && !closedByExplict){
        open();
        return;
      }
    }
  }*/
}



float doorDistance() {
  float duration, distance;
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = (duration / 2) * 0.0343;
  return distance;
  
}

void wsHandleLockerOpen() {
  Serial.println("open");
  open();
  JSONVar res;
  res["type"] = "LOCKER_OPEN_SUCCESS";
  res["data"]["uuid"] = uuid;
  client.send(JSON.stringify(res));
}

void wsHandleLockerClose(int attempt) {
  Serial.println("close");
  JSONVar res;
  if(close()) {
    res["type"] = "LOCKER_CLOSE_SUCCESS";
    closedByExplict = true;
  } else {
    res["type"] = "LOCKER_CLOSE_FAILED";
    res["data"]["attempt"] = attempt;
  }
  res["data"]["uuid"] = uuid;
  client.send(JSON.stringify(res));
}

void wsHandleResPeriod(JSONVar data) {
  Serial.println("period");
  classPeriod[data["period"]]["start"] = data["start"];
  classPeriod[data["period"]]["end"] = data["end"];
  Serial.println(JSON.stringify(classPeriod));
}

void wsHandleResMobileClass(JSONVar data) {
  Serial.println("mobile");
  Serial.println(data.length());
  Serial.println(data);
  if(data.length() > 0) {
    for(int i = 0; i < data.length(); i++) {
      int mcLength = mobileClass.length();
      Serial.println(data[i]["classTime"]);
      mobileClass[mcLength] = data[i]["classTime"];
      JSONVar d;
      d["index"] = "period";
      d["data"] = data[i]["classTime"];
      sendWsMessage("REQ_TIMEPERIOD", d);
    }
  } else {
    mobileClass[0] = null;
    classPeriod[0] = null;
  }
  
}

void wsHandleReqSync() {
  Serial.println("sync");
  locker_init();
}

void wsHandleOnSchedule() {
  onScheduled = true;
}

void wsHandleOffSchedule() {
  onScheduled = false;
}

void open() {
  Serial.println("open1");
  if(isLocked == true) {
    stepper.step(512);
    isLocked = false;
  }
}

bool close() {
  Serial.println("close1");
  stepper.step(-256);
  isLocked = true;
  return true;
}

void onMessageCallback(WebsocketsMessage message) {
  String input = message.data();
  JSONVar myObject = JSON.parse(input);
  Serial.println(JSON.stringify(myObject));
  JSONVar data;
  if(wsType["LOCKER_OPEN"] == myObject["type"]) {
    wsHandleLockerOpen();
  } else if(wsType["LOCKER_CLOSE"] == myObject["type"]) {
    data = (int) myObject["data"]["attempt"];
    wsHandleLockerClose(data);
  } else if (wsType["RES_TIMEPERIOD"] == myObject["type"]) {
    data = myObject["data"]["period"];
    wsHandleResPeriod(data);
  } else if(wsType["RES_MOBILE_CLASS"] == myObject["type"]) {
    data = myObject["data"]["mobileClass"];
    wsHandleResMobileClass(data);
  } else if(wsType["REQ_SYNC"] == myObject["type"]) {
    wsHandleReqSync();
  } else if(wsType["LOCKER_ON_SCHEDULE"] == myObject["type"]) {
    wsHandleOnSchedule();
  } else if(wsType["LOCKER_OFF_SCHEDULE"] == myObject["type"]) {
    wsHandleOffSchedule();
  } else {
    Serial.println(myObject["type"]);
  }
}

void onEventsCallback(WebsocketsEvent event, String data) {
    if(event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("Connnection Opened");
        sendWsMessage("CONNECTION_INIT");
    } else if(event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("Connnection Closed");
    }
}




