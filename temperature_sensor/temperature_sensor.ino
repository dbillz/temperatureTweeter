/*
 * Temperature Tweeter by Dan Billings
 * dgb@udel.edu
 * 
 * This reads five ds18b20 1wire temperature sensors and posts their readings to twitter at fixed intervals.
 * Designed for Arduino Yun running on wifi. The board should have its wifi set up before running this.
 * 
 * You can see it's output at https://twitter.com/ZadrogaHausTemp
 * 
 * Requires Temboo and OneWire arduino libraries.
 * 
 */


#include <Bridge.h>
#include <Temboo.h>
#include "TembooAccount.h"
#include "TwitterAccount.h"
#include <OneWire.h>


OneWire  ds(10);  // data connected to digital pin 10 of the arduino

//The sensor ROMs are the last byte of the ROM id for each sensor.
//you can obtain these by running this program with the arduino serial monitor open, it will be printed.
//the program will not save the temperatures unless these are correct.
int nSensors = 5;
double temperatures[] = {0,0,0,0,0};
String sensorNames[] = {"North","South","East","West","Interior"};
int sensorROMs[] = {0x3C,0x33,0x2B,0x48,0xB4};  

//This value controls the delay between tweets and the delay between heartbeats
//I have it set to 4 hours to stay within Temboo's free tier.
long delayMs = 14400000;


void setup(){
  Serial.begin(9600);
  delay(4000);
  Bridge.begin();
  
}

void loop(){

  //read all connected sensors, saving their values to the temperatures array.
  int haveStopSignal = 0;
  while(haveStopSignal == 0){
    haveStopSignal = readSensors();
  }

  //post the saved temperatures to twitter
  tweet();
  
  //wait 4 hours
  delay(delayMs);
  
  
}

int readSensors(){
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  float celsius, fahrenheit;

  
  if ( !ds.search(addr)) {
    Serial.println(F("No more addresses."));
    Serial.println();
    ds.reset_search();
    delay(250);
    return 1;
  }
  
  Serial.print("ROM =");
  for( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println(F("CRC is not valid!"));
      return 3.0;
  }
  Serial.println();
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      Serial.println(F("  Chip = DS18S20"));  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      Serial.println(F("  Chip = DS18B20"));
      type_s = 0;
      break;
    case 0x22:
      Serial.println(F("  Chip = DS1822"));
      type_s = 0;
      break;
    default:
      Serial.println(F("Device is not a DS18x20 family device."));
      return 2;
  } 

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad

  Serial.print(F("  Data = "));
  Serial.print(present, HEX);
  Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.print(F(" CRC="));
  Serial.print(OneWire::crc8(data, 8), HEX);
  Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  fahrenheit = celsius * 1.8 + 32.0;
  Serial.print(F("  Temperature = "));
  Serial.print(celsius);
  Serial.print(F(" Celsius, "));
  Serial.print(fahrenheit);
  Serial.println(F(" Fahrenheit"));
  saveReading(addr[7],fahrenheit);
  return 0;
}

void saveReading(int sensorROM,double reading){
  Serial.print(F("Saving reading for sensor: "));
  Serial.print(sensorROM);
  Serial.print(" ");
  Serial.println(reading);
  for(int i = 0; i < nSensors; i++){
    if(sensorROM == sensorROMs[i]){
      temperatures[i] = reading;
    }
  }
}


void tweet(){

      Serial.println(F("Running SendATweet"));
    
      //Set the text to be tweeted.
     String tweetText(sensorNames[0] + " " + String(temperatures[0]) + " " + sensorNames[1] + " " + String(temperatures[1]) 
               + " " +sensorNames[2] + " " + String(temperatures[2]) + " " + sensorNames[3] + " " + String(temperatures[3]) 
               + " " +sensorNames[4] + " " + String(temperatures[4]) );
     
      TembooChoreo StatusesUpdateChoreo;
  
      StatusesUpdateChoreo.begin();
      
      // set Temboo account credentials
      StatusesUpdateChoreo.setAccountName(TEMBOO_ACCOUNT);
      StatusesUpdateChoreo.setAppKeyName(TEMBOO_APP_KEY_NAME);
      StatusesUpdateChoreo.setAppKey(TEMBOO_APP_KEY);
  
      // identify the Temboo Library choreo to run (Twitter > Tweets > StatusesUpdate)
      StatusesUpdateChoreo.setChoreo(F("/Library/Twitter/Tweets/StatusesUpdate"));
   
      // add the Twitter account information
      Serial.println(tweetText);
      StatusesUpdateChoreo.addInput(F("AccessToken"), TWITTER_ACCESS_TOKEN);
      StatusesUpdateChoreo.addInput(F("AccessTokenSecret"), TWITTER_ACCESS_TOKEN_SECRET);
      StatusesUpdateChoreo.addInput(F("ConsumerKey"), TWITTER_API_KEY);    
      StatusesUpdateChoreo.addInput(F("ConsumerSecret"), TWITTER_API_SECRET);

      // and the tweet we want to send
      StatusesUpdateChoreo.addInput(F("StatusUpdate"), String(tweetText));
  
      // tell the Process to run and wait for the results. The 
      // return code (returnCode) will tell us whether the Temboo client 
      // was able to send our request to the Temboo servers
      unsigned int returnCode = StatusesUpdateChoreo.run();
  
      // a return code of zero (0) means everything worked
      if (returnCode == 0) {
          Serial.println(F("Success! Tweet sent!"));
      } else {
        // a non-zero return code means there was an error
        // read and print the error message
        while (StatusesUpdateChoreo.available()) {
          char c = StatusesUpdateChoreo.read();
          Serial.print(c);
        }
      } 
      StatusesUpdateChoreo.close();
}

