#include "SSD1306.h"
#include <SPI.h>
#include <LoRa.h>
#include <TinyGPS++.h>  
#include "FS.h"
#include <SD.h>
#include <pgmspace.h>


// OLED pins to ESP32 GPIOs
    // OLED_SDA -- GPIO4
    // OLED_SCL -- GPIO15
    // OLED_RST -- GPIO16

// LoRa_32 ports
    // GPIO05 -- SX1278s SCK
    // GPIO19 -- SX1278s MISO
    // GPIO27 -- SX1278s MOSI
    // GPIO18 -- SX1278s CS/SS
    // GPIO14 -- SX1278s RESET
    // GPIO26 -- SX1278s IRQ(Interrupt Request)

// GPS NEO 6MV2
    // GPIO16 (RX as uart 1) -- TX GPS
    // GPIO17 (TX as uart 1) -- RX GPS

// SDcard ports
    // GPIO12 -- MISO SDCARD
    // GPIO13 -- SCK SD CARD
    // GPIO21 -- MOSI SD CARD
    // GPIO23 -- CS SD CARD

SSD1306 display(0x3c, 4, 15);

// LoRa Settings 
    #define SS 18
    #define RST 14
    #define DI0 26
    #define BAND 433E6
    #define spreadingFactor 9
    #define SignalBandwidth 62.5E3     // #define SignalBandwidth 31.25E3
    #define preambleLength 8
    #define codingRateDenominator 8

// OLED 0.96"
    #define OLED_RST 16

// SD card
    SPIClass SPI2(HSPI);

// GPS NEO
    TinyGPSPlus gps;                            
    HardwareSerial MySerial1(0);      // pada HW-Thinker uart 0 --> u2RX u2TX (3,1)

// Initiation variable
    int counter = 0;
    const int LED_indicator = 2;
    String datapakages;
    String dataGPS;



void setup() {
  // Send pakages success, LED will bright
      pinMode(LED_indicator, OUTPUT);   
      pinMode(OLED_RST,OUTPUT);
      
  // set GPIO16 low to reset OLED
      digitalWrite(OLED_RST, LOW);      
      delay(50);
      digitalWrite(OLED_RST, HIGH);

  // Serial communications
      //Serial.begin(115200);
      //while (!Serial); 

  // Serial Uart 1 (myserial1)
     MySerial1.begin(9600, SERIAL_8N1, 1, 3);

  // Initialising the UI OLED Screen.
      display.init();
      display.flipScreenVertically();
      display.setFont(ArialMT_Plain_10);
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.drawString(5,5,"LoRa Sender");
      display.display();

  // Initialising LoRa SX1278
      SPI.begin(5,19,27,21);                      // LoRA SPI comm sck, miso, mosi, cs
      LoRa.setPins(SS,RST,DI0);                   // 18, 14, 26
      Serial.println("LoRa Sender");              // Type of communicator

      if (!LoRa.begin(BAND)) {                    // Stating LoRa
        Serial.println("Starting LoRa failed!");  // if LoRa cannot start
        while (1);                                // do nothing
      }
      
  // Set Spreading Factor LoRa SX1278
      Serial.print("LoRa Spreading Factor: ");
      Serial.println(spreadingFactor);
      LoRa.setSpreadingFactor(spreadingFactor);

  // Set Signal Bandwidth LoRa SX1278
      Serial.print("LoRa Signal Bandwidth: ");
      Serial.println(SignalBandwidth);
      LoRa.setSignalBandwidth(SignalBandwidth);

  // Set codingRateDenominator LoRa SX1278
      LoRa.setCodingRate4(codingRateDenominator);

  // Set preambleLength LoRa SX1278
      LoRa.setPreambleLength(preambleLength);

  // if LoRa SX1278 --> Sukses
      Serial.println("LoRa Initial OK!");
      display.drawString(5,20,"LoRa Initializing OK!");
      display.display();

  // SDcard iniialize
      SPI2.begin(13, 12, 21);    // sck, miso, mosi
      if (! SD.begin(23, SPI2, 1000000)) {
        Serial.println(F("Card Mount Failed"));
        display.drawString(5,35,"SD Initializing fail!");
        display.display();
        while(1);
      }
      Serial.println(F("Card Mount Sukses"));
      display.drawString(5,35,"SD Initializing OK!");
      display.display();

      appendFile(SD, "/cobacoba.txt", "PACK|LAT|LON|ALT|SAT|HDOP ");

      display.clear();
      delay(2000);
}

void loop() {
      display.clear();
      getGPS();
      sendpacketLORA(100);

      char buff[128];
      datapakages.toCharArray(buff, 128);
      appendFile(SD, "/cobacoba.txt", buff);
      delay(10); // debounce
}


void getGPS(){

    // get the byte data from the GPS
      while (MySerial1.available() > 0){
          gps.encode(MySerial1.read());     
      }
      if (gps.location.isUpdated()){}
        display.setFont(ArialMT_Plain_10);
        display.drawString(5, 20, "Lat : " + String(gps.location.lat(), 8));
        display.drawString(5, 35, "Lon : " + String(gps.location.lng(), 8));
        display.drawString(5, 50, "Alt : " + String(gps.altitude.meters(), 2));
        display.drawString(60, 50, "/ " + String(gps.satellites.value()));
        display.drawString(78, 50, "/ " + String(gps.hdop.value()/100, 6));

        dataGPS = String(gps.location.lat(), 8) + "|" + String(gps.location.lng(), 8) + "|" + String(gps.altitude.meters(), 2) + "|" + String(gps.satellites.value()) + "|" + String(gps.hdop.value()/100.0, 2);
}


int sendpacketLORA(int interval){
  
    // LoRa SX1278 sending pakages
        // Serial.print("Sending packet: ");
        // Serial.println(counter);
        display.setFont(ArialMT_Plain_10);
        display.drawString(3, 5, "Sending packet ");
        display.drawString(90, 5, String(counter));
        display.display();

      datapakages = "Au0o.." + String(counter) + "|" + dataGPS;
      
  // Send packet
      LoRa.beginPacket();
      LoRa.print(datapakages);
      LoRa.endPacket();
      
      counter++;
      digitalWrite(LED_indicator, HIGH);  
      delay(interval / 2); 
      digitalWrite(LED_indicator, LOW);  
      delay(interval / 2);
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        display.drawString(5,45,"Failed to open file!");
        display.display();
        while(1);
    }
    file.println(message);

    file.close();
}
