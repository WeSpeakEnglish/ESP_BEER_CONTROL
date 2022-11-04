/*
    This sketch demonstrates how to scan WiFi networks.
    The API is almost the same as with the WiFi Shield library,
    the most obvious difference being the different file you need to include:
*/
#include "EEPROM.h"
#include "WiFi.h"
#include "time.h"
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

OneWire ds(25);  //data wire connected to GPIO15
DeviceAddress sensor1 = { 0x28, 0xFF, 0x45, 0x4C, 0x74, 0x15, 0x3, 0x1 }; //hot water
DeviceAddress sensor2 = { 0x28, 0xFF, 0xA7, 0xF2, 0x81, 0x15, 0x1, 0xD8}; //cold water
DallasTemperature sensors(&ds);
float temp[8]; //temperature array

const char* ssid       = "Redmi";
const char* password   = "micromax";

const char* ntpServer = "pool.ntp.org";
const char* httpSettings = "http://test.pollutants.eu/setup.php?set_flag=1";

const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

int RTC_DATA_ATTR LoopCount = 0;

//static const uint8_t LED_BUILTIN = 32;
#define HEAT_RELAY  27 // backward compatibility
#define PUMP1  26 // backward compatibility
#define PUMP2  32 // backward compatibility
#define COOLER 33 // backward compatibility

#define EEPROM_SIZE 16
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5        /* Time ESP32 will go to sleep (in seconds) */

signed char TemperatureArray[EEPROM_SIZE] = {0, 1, 2, 3, 4, 5, 6, 7}; // temporal temperature array
signed char TimeArray[EEPROM_SIZE] = {0, 1, 2, 3, 4, 5, 6, 7}; // temporal temperature array

time_t unixTime[2]={0,0};
int Temperature[2];

//signed char RTC_DATA_ATTR TemperatureArrayRTC[EEPROM_SIZE];
//signed char RTC_DATA_ATTR TimeArrayRTC[EEPROM_SIZE];

signed char RTC_DATA_ATTR TimeArrayRTC[EEPROM_SIZE];
struct RTC_DATA_ATTR tm timeinfo;

int RTC_DATA_ATTR loopCounter = 0; // main loop counter

int addr = 0;
#define ARRAY_SIZE_PARMS 8
int arrayParams[ARRAY_SIZE_PARMS];

void parseParms(String strToParse) {
  bool flag = false;
  int arrayIndex = 0;
  int numbIndex = 0;
  int numb[4];

  for (int i = 0; i < 35; i++) {
    if (strToParse[i] == ':') {
      if (flag) {
        switch (numbIndex) {
          case 1:
            arrayParams[arrayIndex++] = numb[0];
            break;
          case 2:
            arrayParams[arrayIndex++] = (numb[0]) * 10 + numb[1];
            break;
          case 3:
            arrayParams[arrayIndex++] = (numb[0]) * 100 + (numb[1]) * 10 + numb[2];
            break;
          case 4:
            arrayParams[arrayIndex++] = (numb[0]) * 1000 + (numb[1]) * 100 + (numb[2]) * 10 + numb[3];
            break;
        }

        if (arrayIndex == ARRAY_SIZE_PARMS) return;
        for (int j = 0; j < 4 ; j++) {
          numb[j] = 0;
          numbIndex = 0;
        }
      }

      flag = true;
    }
    else {
      if (flag && ((int)strToParse[i] > 47)) {
        numb[numbIndex] = (int)strToParse[i] - 48;
        numbIndex++;
      }
    }
  }
}

void writeParmsEEPROM(int* arrayParams){
  int Idx = 0;
  if (!EEPROM.begin(EEPROM_SIZE))
  {
    Serial.println("failed to initialise EEPROM"); delay(1000000);
  }

  for (int i = 0; i < 8; i++)
  {
    EEPROM.write(Idx,arrayParams[i]&0xFF);
    EEPROM.write(Idx+1,arrayParams[i]>>8);
    EEPROM.commit();
    Serial.print(EEPROM.read(Idx));Serial.print(",");Serial.print(EEPROM.read(Idx+1));Serial.println(";");
    Idx+=2;
  }
  
  }
  
void readParmsEEPROM(int* arrayParams){
  int Idx = 0;
  if (!EEPROM.begin(EEPROM_SIZE))
  {
    Serial.println("failed to initialise EEPROM"); delay(1000000);
  }

  for (int i = 0; i < 8; i++)
  {
   arrayParams[i] = EEPROM.read(Idx++);
   arrayParams[i] += EEPROM.read(Idx++)<<8;

  }
  
  }
void printLocalTime()
{
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

bool takeDataWeb(bool reconnect) {
  WiFiClient cli;
  bool flagSuccess = false;
  int i = 0;
  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  //connect to WiFi
  Serial.printf("Connecting to %s ", ssid);
  
  WiFi.mode(WIFI_AP_STA);

  if(!reconnect)WiFi.begin(ssid, password);
  else WiFi.reconnect();
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    i++;
    if (i > 20) break;
  }
  Serial.println(" CONNECTED");

  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
  ///HTTP
  HTTPClient http;
  

  Serial.print("[HTTP] begin...\n");
  http.begin(httpSettings); //HTTP
  Serial.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();

  // httpCode will be negative on error
  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println("---------------------------------------");
      Serial.println(payload);
      parseParms(payload);
      writeParmsEEPROM(arrayParams);
      Serial.println("---------------------------------------");
      for (int i = 0; i < 8; i++) {
        Serial.print(arrayParams[i]);
        Serial.print(" : ");
      }
      flagSuccess = true;
      Serial.println("---------------------------------------");
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();

  ////////////////
  //disconnect WiFi as it's no longer needed
    WiFi.disconnect(true);
//  WiFi.mode(WIFI_OFF);
  return flagSuccess;
}
//----------------------------------
void setup()
{
  Serial.begin(115200);

  pinMode (HEAT_RELAY, OUTPUT);
  digitalWrite(HEAT_RELAY, LOW);
  pinMode (PUMP1, OUTPUT);
  digitalWrite(PUMP1, HIGH);
  pinMode (PUMP2, OUTPUT);
  digitalWrite(PUMP2, HIGH);
  pinMode (COOLER, OUTPUT);
  digitalWrite(COOLER, HIGH);

  Serial.println(loopCounter);
  //if (LoopCount++ % 60 == 0) {
  //  takeDataWeb();
  // takeDataWeb();
readParmsEEPROM(arrayParams);
  for (int i = 0; i < 8; i++)
  {
    Serial.print(arrayParams[i]); Serial.print(" ");
  }
  Serial.println("Setup done");
  // }
  sensors.begin();
}

void stateMachine(void);

void loop()
{
  byte i;
  sensors.begin();

  Serial.println(" ");
  printLocalTime();
  unixTime[1] = mktime(&timeinfo);
  Serial.print(unixTime[1]);Serial.print(" ");
  
  Serial.println(" ");
  if (loopCounter == 1){
   takeDataWeb(0);
  }
 loopCounter++;
  sensors.requestTemperatures(); // Send the command to get temperatures
  
  Temperature[0] = sensors.getTempC(sensor1);
  Temperature[1] = sensors.getTempC(sensor2);
  
  Serial.print("Sensor 1(*C): ");
  Serial.print(Temperature[0]);
  Serial.print(" Sensor 2(*C): ");
  Serial.print(Temperature[1]);
  // esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);


 // Serial.flush();

  delay(2000);
if((loopCounter%20) == 19)takeDataWeb(1);
stateMachine();
  // esp_light_sleep_start();
  // esp_deep_sleep_start();
 
}

void stateMachine(void){
static int Stage = 0;

 switch (Stage){
  case 0:
   Serial.print(" Stage 0");
  ///////////
  if(timeinfo.tm_min == (arrayParams[6]%100) &&  
      timeinfo.tm_hour == (arrayParams[6]/100) &&  
      timeinfo.tm_mday == (arrayParams[7]%100) &&  
      timeinfo.tm_mon == ((arrayParams[7]/100) -1))
  {
    Stage ++;
     unixTime[0] = mktime(&timeinfo);
    Serial.print(" Entered stage 1");
    }
          break;
  case 1:
 
     if(difftime(mktime(&timeinfo), unixTime[0]) < arrayParams[0]){
          digitalWrite(PUMP1, LOW);
          Serial.print(" Stage 1 time:  ");Serial.print(difftime(mktime(&timeinfo), unixTime[0]));
    }
    else{
          digitalWrite(PUMP1, HIGH); 
          Serial.print(" Entered stage 2");  
          Stage ++;  
          unixTime[0] =  mktime(&timeinfo);
      }
          break;
  case 2:
     if(difftime(mktime(&timeinfo), unixTime[0]) < arrayParams[1]){
          if (Temperature[0] < arrayParams[2])digitalWrite(HEAT_RELAY, HIGH);
          else digitalWrite(HEAT_RELAY, LOW);
          Serial.print(" Stage 2 time:  "); Serial.print(difftime(mktime(&timeinfo), unixTime[0]));
    }
    else{
          digitalWrite(HEAT_RELAY, LOW); 
          Serial.print("Entered stage 3");  
          Stage ++;  
          unixTime[0] =  mktime(&timeinfo);
      }
          break;
  case 3: 
       if(difftime(mktime(&timeinfo), unixTime[0]) < arrayParams[3]){
          digitalWrite(PUMP2, LOW);
          Serial.print(" Stage1 time:  ");Serial.print(difftime(mktime(&timeinfo), unixTime[0]));
    }
    else{
          digitalWrite(PUMP2, HIGH); 
          Serial.print("Entered stage 3");  
          Stage ++;  
          unixTime[0] =  mktime(&timeinfo);
      }               
          break;
  case 4:
       if(difftime(mktime(&timeinfo), unixTime[0]) < arrayParams[4]){
          if (Temperature[1] > arrayParams[5])digitalWrite(COOLER, LOW);
          else digitalWrite(COOLER, HIGH);
          Serial.print(" Stage4 time:  "); Serial.print(difftime(mktime(&timeinfo), unixTime[0]));
    }
    else{
          digitalWrite(COOLER, HIGH); 
          Serial.print("Entered stage 0");  
          Stage = 0;  
      }
          break;
   
  }
  
  return;
  }
