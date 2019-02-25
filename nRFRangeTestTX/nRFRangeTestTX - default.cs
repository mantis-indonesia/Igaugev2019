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

const uint64_t pipeOut =  0xE8E8F0F0E1LL;
byte addresses[][6] = {"1serv", "2Node"};

// The sizeof this struct should not exceed 32 bytes
struct PacketData {
  unsigned long hours;
  unsigned long minutes;
  unsigned long seconds;
};

PacketData data;

RF24 radio(48, 49);
char balas[4];

void setup() {
  Serial.begin(9600);
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);

  if (!radio.begin()) {
    Serial.println("begin error");
  }
  else Serial.println("oK");
  radio.setAutoAck(true);
  radio.setDataRate(RF24_250KBPS);
  radio.openWritingPipe(addresses[0]);
  radio.openReadingPipe(1, addresses[1]);
  
  radio.stopListening();
  
  data.hours = 0;
  data.minutes = 0;
  data.seconds = 0;
}

unsigned long lastTick = 0;

void loop() {
  unsigned long now = millis();
  if ( now - lastTick >= 1000 ) {
    data.seconds++;
    if ( data.seconds >= 60 ) {
      data.seconds = 0;
      data.minutes++;
    }
    if ( data.minutes >= 60 ) {
      data.minutes = 0;
      data.hours++;
    }
    lastTick = now;


    radio.write(&data, sizeof(PacketData));
    char hmsBuf[16];
    sprintf(hmsBuf, "%02ld:%02ld:%02ld", data.hours, data.minutes, data.seconds);
    Serial.println(hmsBuf);
    Serial.print("wait reply ");

    recvData();
  }
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
    }else{
      radio.read(&balas, sizeof(balas));
    Serial.println((char*)balas);
    }

    radio.stopListening();
}

