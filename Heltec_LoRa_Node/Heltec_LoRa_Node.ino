#include "SSD1306.h"
#include <SPI.h>
#include <LoRa.h>
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

// SDcard ports
    // GPIO12 -- MISO SDCARD
    // GPIO13 -- SCK SD CARD
    // GPIO21 -- MOSI SD CARD
    // GPIO23 -- CS SD CARD

SSD1306 display(0x3c, 4, 15);         // OLED Data, Clock

// LoRa Settings 
    #define SCK 5
    #define MISO 19
    #define MOSI 27
    #define SS 18
    #define RST 14
    #define DI0 26
    #define BAND 433E6
    #define spreadingFactor 9
    #define SignalBandwidth 62.5E3    // #define SignalBandwidth 31.25E3
    #define codingRateDenominator 8

// OLED 0.96"
    #define OLED_RST 16

// SD card
    SPIClass SPI2(HSPI);
 
// Initiation variable
    const int LED_indicator = 2;
    String datapakages = "";
    String rssipakages = "";
    String snrpakages = "";


void setup() {
  // Recieve pakages success, LED will bright
      pinMode(LED_indicator, OUTPUT);   
      pinMode(OLED_RST,OUTPUT);
      
  // set GPIO16 low to reset OLED
      digitalWrite(OLED_RST, LOW);      
      delay(50);
      digitalWrite(OLED_RST, HIGH);

  // Serial communications
      Serial.begin(115200);
      while (!Serial); 

  // Initialising the UI OLED Screen.
      display.init();
      display.flipScreenVertically();
      display.setFont(ArialMT_Plain_10);
      display.setTextAlignment(TEXT_ALIGN_LEFT);
      display.drawString(5,5,"LoRa Reciever");
      display.display();

  
  // Initialising LoRa SX1278
      SPI.begin(SCK,MISO,MOSI,SS);                // LoRA SPI comm 5, 19, 27, 18
      LoRa.setPins(SS,RST,DI0);                   // 18, 14, 26
      Serial.println("LoRa Reciever");              // Type of communicator

      if (!LoRa.begin(BAND)) {                    // Stating LoRa
        Serial.println("Starting LoRa failed!");  // if LoRa cannot start
        while (1);                                // do nothing
      }

  // Frequency LoRa SX1278
      Serial.print("LoRa Frequency: ");
      Serial.println(BAND);

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

  // if LoRa SX1278 --> Sukses 
      Serial.println("LoRa Initial OK!");
      display.drawString(5,25,"LoRa Initializing OK!");
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
      delay(2000);
      display.clear();
      
}

void loop() {
  // try to parse packet
      int packetSize = LoRa.parsePacket();
      if (packetSize){
        // received a packets
            Serial.print("Received packet. ");
            display.clear();
            display.setFont(ArialMT_Plain_10);
            display.drawString(5, 5, "Received packet ");
            display.display();
    
        // read packet
            while (LoRa.available()) {
              datapakages = LoRa.readString();
              Serial.print(datapakages);
              display.drawString(5,15, datapakages);
              display.display();
        }

  // print RSSI of packet
      rssipakages = LoRa.packetRssi();
      Serial.print(" with RSSI ");
      Serial.println(rssipakages);

      snrpakages = LoRa.packetSnr();
      Serial.print(" with SNR ");
      Serial.println(snrpakages);
      
      display.drawString(5, 30, rssipakages + "dB (" + snrpakages +"dB)");
      display.display();

      char buff[128];
      datapakages.toCharArray(buff, 128);
      appendFile(SD, "/bogor.txt", buff);
  }
  
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
