#include <config.h>
#include<ADC_config.h>
#include <Functions.h>
#include<RTClib.h>
//#include<Ch376msc.h>
#include <SPI.h>
#include <SD.h>
RTC_DS3231 rtc;

#define SD_CS 5

File myfile;

ADC C1(2, 12);
unsigned long CurrentTime = 0;
unsigned long PreviousTime = 0;
unsigned long interval = 1000;

uint16_t Year;
uint8_t Month;
uint8_t Day;
uint8_t Hour;
uint8_t Minute;
uint8_t Second;
uint16_t C1Data;

void setup() {
  Serial.begin(9600);
  pinMode(SWITCH, INPUT_PULLUP);
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);



 if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, let's set the time!");
    
   //rtc.adjust(DateTime(F(_DATE), F(TIME_)));
    
  }

 if (!SD.begin(SD_CS, SPI)) {
    Serial.println("Card Mount Failed!");
    while (1);
  }
  Serial.println("SD Card initialized.");
}

void loop() {
  CurrentTime = millis();
  if(digitalRead(SWITCH) == LOW){
    SwitchOn(LEDR, LEDG);
  if(CurrentTime>=(PreviousTime + interval)){
    DateTime now = rtc.now();
   
    Day = now.day();
    Month = now.month();
    Year = now.year();
    Hour = now.hour();
    Minute = now.minute();
    Second = now.second();
    C1Data = C1.Read(); 
    Serial.println(C1Data);
    writeCSV(Year, Month, Day, Hour, Minute, Second, myfile);
    PreviousTime = CurrentTime;
  }
  else{
    SwitchOff(LEDR, LEDG);
  }
}

}