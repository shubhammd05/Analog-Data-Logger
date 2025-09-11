#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
  void SwitchOn(int red, int green){
        digitalWrite(green, HIGH);
        digitalWrite(red, LOW);
  }

  void SwitchOff(int red, int green){
        digitalWrite(red, HIGH);
        digitalWrite(green, LOW);
  }

  void writeCSV(uint16_t Y,uint8_t M, uint8_t D, uint8_t H, uint8_t Mi, uint8_t S, uint16_t D1, File myFile) {
  myFile = SD.open("/data.csv", FILE_APPEND);
  if (myFile) {
    
    myFile.print(D);
    myFile.print("-");
    myFile.print(M);
    myFile.print(",");
    myFile.print(Y);
    myFile.print(",");
    myFile.print(H);
    myFile.print(":");
    myFile.print(Mi);
    myFile.print(":");
    myFile.print(S);
    myFile.print(",");
    myFile.print(D1);
    myFile.print("\n");

    myFile.close();
    Serial.println("Entry added to CSV.");
  } else {
    Serial.println("Error opening file for writing.");
  }
}