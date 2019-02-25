/*
  Simple transmitter, just sends 12-byte packet continously
  nRF24L01 library: https://github.com/gcopeland/RF24

  nRF24L01 connections
  1 - GND
  2 - VCC 3.3V !!! Ideally 3.0v, definitely not 5V
  3 - CE to Arduino pin 9
  4 - CSN to Arduino pin 10
  5 - SCK to Arduino pin 13
  6 - MOSI to Arduino pin 11
  7 - MISO to Arduino pin 12
  8 - UNUSED

*/
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <TinyGPS++.h>

#define ok 4
#define eror 6

const uint64_t pipeOut =  0xE8E8F0F0E1LL;
byte addresses[][6] = {"1serv", "2Node"};

// The sizeof this struct should not exceed 32 bytes
RF24 radio(48, 49);
char balas[4];
char data[32];
char tanggal[10];

TinyGPSPlus gps;
unsigned long start;
unsigned long waktu = 5000; //ms
float flat;
float flon;
float hdops;
String teks;
char g;

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);  // GPS U BLOX
  pinMode(ok, OUTPUT);
  pinMode(eror, OUTPUT);
  //IRQ PIN SET LOW
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);

  if (!radio.begin()) {
    Serial.println("begin error");
  }
  else Serial.println("oK");
  Serial.println("RF24 set channel 0 2400 MHz");
  radio.setChannel(0);
  Serial.println("Data Rate RF24_250KBPS");
  radio.setDataRate(RF24_250KBPS);
  Serial.println("Set ACK True");
  radio.setAutoAck(true);
  Serial.println("PA Level MAX");
  radio.setPALevel(RF24_PA_MAX);
  
  radio.openWritingPipe(addresses[0]);
  radio.openReadingPipe(1, addresses[1]);
  
  radio.stopListening();
  
  flat = 987.9876;
  flon = 978.9678;
  hdops = 99.9;
}


void loop() {
  delay(2000);
  gpsdata();
  
    radio.write(&data, sizeof(data));
    char hmsBuf[16];
    sprintf(hmsBuf, "%s", data);
    Serial.println(hmsBuf);
    Serial.print("wait reply ");

    recvData();
  }


void recvData() {
  
  radio.startListening();
  unsigned long started_waiting_at = micros();               // Set up a timeout period, get the current microseconds
    boolean timeout = false; 
  while ( ! radio.available() ){                             // While nothing is received
      if (micros() - started_waiting_at > 200000 ){            // If waited longer than 200ms, indicate timeout and exit while loop
          timeout = true;
          break;
      }      
    }
  
    if ( timeout ){                                             // Describe the results
        Serial.println(F("Failed, response timed out."));
        digitalWrite(eror,HIGH);
        digitalWrite(ok,LOW);
    }else{
      radio.read(&balas, sizeof(balas));
    Serial.println((char*)balas);
    digitalWrite(ok,HIGH);
        digitalWrite(eror,LOW);
    }

    radio.stopListening();
}

void gpsdata() {
  start = millis();
  do   {
    while (Serial1.available()) {
      g = Serial1.read();
      gps.encode(g);
      Serial.print(g);
    }
  }
  while (millis() - start < waktu);

  Serial.println(" ");
  Serial.flush();
  Serial1.flush();
  if (gps.location.isUpdated())  {
    flat = gps.location.lat();
    flon = gps.location.lng();
    hdops = float(gps.hdop.value()) / 100.00;
  }
  if (gps.charsProcessed() < 10)  {
  }
  if (gps.time.isValid())
    sprintf(tanggal, "%02d:%02d:%02d,", gps.time.hour(), gps.time.minute(), gps.time.second());
  else sprintf(tanggal, "99:99:99,");
  //07:08:09,42.1234,144,1234,20.11
  teks = String(tanggal) + String(flat, 4) + "," + String(flon, 4) + "," + String(hdops, 2);
  teks.toCharArray(data, 32);
}


