/*
  Simple receiver to display how many packets per second are being received
  nRF24L01 library: https://github.com/gcopeland/RF24
  I2C OLED screen library: https://github.com/olikraus/u8glib

  nRF24L01 connections
  1 - GND
  2 - VCC 3.3V !!! Ideally 3.0v, definitely not 5V
  3 - CE to Arduino pin 9
  4 - CSN to Arduino pin 10
  5 - SCK to Arduino pin 13
  6 - MOSI to Arduino pin 11
  7 - MISO to Arduino pin 12
  8 - UNUSED

  OLED connections
  GND - GND
  VCC - VCC
  SDA - Arduino pin A4
  SCL - Arduino pin A5

*/

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define ok 47
#define eror 46

const uint64_t pipeIn =  0xE8E8F0F0E1LL;
byte addresses[][6] = {"1serv", "2Node", "3Node","4Node"};

RF24 radio(48, 49);

// The sizeof this struct should not exceed 32 bytes

char data[32];

/**************************************************/

int packetCounts[10];
int packetCountIndex = 0;
int packetCountTotal = 0;

unsigned long packetsRead = 0;
unsigned long lastScreenUpdate = 0;
unsigned long lastAvgUpdate = 0;
unsigned long lastRecvTime = 0;
unsigned long drops = 0;
char ppsBuf[16];
char Buf[4] = "ack";
char hmsBuf[16];
/**************************************************/

void setup()
{
  Serial.begin(9600);
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
  pinMode(ok, OUTPUT);
  digitalWrite(ok, LOW);
  pinMode(eror, OUTPUT);
  digitalWrite(eror, LOW);
  Serial.println(" lora server ");
  //startup
  digitalWrite(ok,HIGH);
  delay(1000);
  digitalWrite(eror,HIGH);
  delay(1000);
  digitalWrite(ok, LOW);
  digitalWrite(eror, LOW);
  delay(2000);
  
  // Set up radio module
  if (!radio.begin()) {
    Serial.println("begin error");
  }
  Serial.println("Data Rate RF24_250KBPS");
  radio.setDataRate(RF24_250KBPS);
  Serial.println("Set ACK True");
  radio.setAutoAck(true);
  Serial.println("PA Level MAX");
  radio.setPALevel(RF24_PA_MAX);
  Serial.println("Ready to receive data");
  Serial.flush();
}


void recvData() {
  radio.openReadingPipe(1, addresses[0]);
  radio.startListening();

  //wait data come in
  while ( radio.available() ) {
	  digitalWrite(ok,HIGH);
    radio.read(&data, sizeof(data));
    packetsRead++;
    // moving average over 1 second
    packetCountTotal -= packetCounts[packetCountIndex];
    packetCounts[packetCountIndex] = packetsRead;
    packetCountTotal += 1;

    packetCountIndex = (packetCountIndex + 1) % 10;
    packetsRead = 0;

    sprintf(ppsBuf, "PPS: %d", packetCountTotal);
    Serial.print(ppsBuf);
    Serial.print(" ");
    sprintf(hmsBuf, "%s", data);
    Serial.print(hmsBuf);
    sendData();
  }
  digitalWrite(ok,LOW);
}

/**************************************************/


void loop() {
  recvData();
}

void sendData() {
  radio.openWritingPipe(addresses[1]);
  radio.stopListening();

  radio.write(Buf, sizeof(Buf));
  radio.openReadingPipe(1, addresses[0]);
  radio.startListening();

  Serial.print(" - ");
  Serial.println((char*)Buf);
}





