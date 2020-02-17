// IOT PRESSURE DATA LOGGER
// SENDING TO THINGSPEAK
// BOARD : ARDUINO PRO MINI
// DEVELOPED BY : MANTIS ID
// REVISED : 2020-2-16

#include <SoftwareSerial.h>

// nilai burst
const byte burst = 5;
const byte interval =1;

//KONFIGURASI IP. Pilih salah satu
String IP = "Telkomsel";
const char USER[] = "";
const char PWD[] = "";

//api key
String SECRET_WRITE_APIKEY = "SB97TGUB7QFMNJN9";  // replace XYZ with your channel write API Key

//pin sensor tekanan
#define pressurePin A1  // Put the data cable to pin A1

//KONFIGURASI SOFTWARE SERIAL
SoftwareSerial SIM7000(6, 7); // RX, TX 6,7

//KONFIGURASI PIN
#define powerSensor 4 // pin untuk menyalakan sensor
#define key 5         // pin power key
#define ledOK 2       // pin LED untuk tanda OK - 2
#define ledERROR 3    // pin LED untuk tanda ERROR - 3

String kalimat;
unsigned long mulai;
float tekanan;
char json[80];

void setup() {
  Serial.begin(9600);
  Serial.println(F("WATER PRESSURE SENSOR PDAM THINGSPEAK 2020"));
  delay(1000);
  Serial.println(F("- INISIALISASI SERIAL GSM "));
  SIM7000.begin(9600);
  delay(1000);

  Serial.println(F("< ATUR LED OK DAN LED ERROR >"));
  pinMode(ledOK, OUTPUT);
  pinMode(ledERROR, OUTPUT);
  pinMode(powerSensor, OUTPUT);
  pinMode(key, OUTPUT);
  delay(1000);

  Serial.println(F("< NYALAKAN LED >"));
  digitalWrite(ledOK, HIGH);
  delay(1000);
  digitalWrite(ledERROR, HIGH);
  delay(1000);
  digitalWrite(ledOK, 0);
  digitalWrite(ledERROR, 0);
  delay(1000);

  Serial.println(F("Tunggu Modul GSM Aktif - 20 detik"));
  delay(1000);
  for (byte i = 1; i <= 20; i++) {
    Serial.print(i);
    Serial.println(F(" detik"));
    digitalWrite(ledOK, 1);
    delay(700);
    digitalWrite(ledOK, 0);
    delay(300);
  }

  //INIT GSM
  Serial.println(F("\r\n- INIT GSM -"));
  initGSM();
  delay(1000);

}

void loop() {
  unsigned long Start=millis();
  Serial.println(F("\r\n- PENGAMBILAN DATA -"));
  Serial.println(F("\r\n< AMBIL DATA SENSOR >"));
  digitalWrite(powerSensor, 1);
  sensor();
  digitalWrite(powerSensor, 0);

  //cek data tekanan di serial monitor
  Serial.print(F("Tekanan="));
  Serial.print(tekanan);
  Serial.println(F(" bar\n"));

  Serial.println(F("\r\n<- KIRIM DATA KE THINGSPEAK ->"));
  gprsComm();
  sendServer();  //kirim data

  sisamemori();

  unsigned long Stop=millis();
  unsigned long sisa=interval * 60UL * 1000UL - (Stop - Start);
  Serial.println(sisa);
  unsigned long bagi=0;
  while(bagi<sisa){
    bagi=millis()-Stop;
    Serial.println(bagi);
    digitalWrite(ledOK,0);
    delay(700);
    digitalWrite(ledOK,1);
    delay(300);
  }

  sisamemori();
  digitalWrite(ledOK,0);
}

// ROUTINE FOR GSM
void initGSM() {
  // AT
  Serial.println(F("\r\n- CEK AT -"));
  at();

  // Check pin and functionality
  Serial.println(F("\r\n- CPIN & CFUN -"));
  gsmCheckFunc();
  delay(1000);

  // DEFINE PDP CONTEXT
  Serial.println(F("\r\n- ATUR PDP -"));
  gsmPDP();
  delay(1000);

  // REGISTRASI KARTU KE NETWORK PROVIDER
  Serial.println(F("\r\n- REGISTRASI KE NETWORK -"));
  gsmRegister();
  delay(1000);

  // CEK OPERATOR
  Serial.println(F("\r\n- CEK OPERATOR -"));
  gsmOperator();
  delay(1000);

  // CEK KEKUATAN SINYAL
  Serial.println(F("\r\n- CEK KEKUATAN SINYAL -"));
  gsmSignal();
  delay(1000);
}

char cekSerial(unsigned int times) {
  boolean indeks = 0;
  char karakter;
  unsigned long mulai = millis();
  while (millis() - mulai < times) {
    while (SIM7000.available() > 0) {
      karakter = SIM7000.read();
      Serial.write(karakter);
      if (karakter == 'O' || karakter == 'E') {
        indeks = 1;
        break;
      }
    }
    if (indeks == 1) break;
  }
  
  mulai = millis();
  while (millis() - mulai < 100) {
    while (SIM7000.available() > 0) {
      Serial.write(SIM7000.read());
    }
  }

  Serial.write(13);
  Serial.write(10);

  return karakter;
}

String bacaserial(unsigned int wait) {
  String kalimat = "";
  unsigned long mulai = millis();
  while ((mulai + wait) > millis()) {
    while (SIM7000.available() > 0) {
      char karakter = SIM7000.read();
      Serial.write(karakter);
      kalimat += karakter;
    }
  }
  SIM7000.flush();
  return kalimat;
}

void at() {
  for (byte i = 0; i < 6; i++) {
    SIM7000.println(F("AT"));
    char output = cekSerial(200);
    if (i == 5 && output != 'O') {
      Serial.println(F("GSM MODULE ERROR"));
      Serial.println(F("CONTACT CS!!!"));
      while (1);
    }
    if(output=='O') break;
    delay(1000);
  }
}

// Pengecekan kesiapan kartu dari pin dan functionality
void gsmCheckFunc() {
  SIM7000.println(F("AT+CPIN?;+CFUN=1"));
  cekSerial(200);
}

//set PDP context
void gsmPDP() {
  String output;
  output = "AT+CGDCONT=1,\"IP\",\"";
  output.concat(String(IP));
  output.concat("\"");
  SIM7000.println(output);
  cekSerial(500);
  output = "AT+CGDCONT=13,\"IP\",\"";
  output.concat(String(IP));
  output.concat("\"");
  SIM7000.println(output);
  cekSerial(500);
}

void gsmOperator() {
  byte i = 0;
  byte indeks = 255;
  while (indeks == 255) {
    String output = "";
    SIM7000.println(F("AT+COPS?"));
    output = bacaserial(200);
    indeks = output.indexOf("\"", 0);
    Serial.println(indeks);
    delay(2000);
    if (indeks == 255) {
      i++;
      if (i == 20) {
        Serial.println(F("OPERATOR PROVIDER TIDAK DITEMUKAN"));
        initGSM();
      }
    }
  }
}

void gsmRegister() {
  SIM7000.println(F("AT+CREG=1"));
  cekSerial(200);
}

void gsmSignal() {
  SIM7000.println(F("AT+CSQ"));
  cekSerial(200);
}

void sensor() {
  unsigned long nilai = 0;
  float pressVolt = 0;
  tekanan = 0.00;

  for (byte indeks = 0; indeks < burst; indeks++) {
    int pressADC = analogRead(pressurePin);
    nilai += pressADC;
  }

  pressVolt = (float (nilai / burst) / 1024.00 * 5.00);
  tekanan = (3.00 * pressVolt - 1.5);// - offset; // satuan bar
  if(tekanan <0) tekanan =0.00;
}

boolean gprsComm() {
  Serial.println(F("\r\n- TUTUP DULU GPRS -"));
  gprsShut();
  gsmShut();
  delay(200);

  // SET SINGLE IP CONNECTION
  Serial.println(F("\r\n- ATUR CIPMUX MENJADI SINGLE IP CONNECTION -"));
  gprsMux();
  delay(200);

  // DEFINE PDP CONTEXT
  Serial.println(F("\r\n- ATUR PDP -"));
  gsmPDP();

//  // ACTIVATE PDP CONTEXT
//  Serial.println(F("\r\n- AKTIVASI PDP -"));
//  while (PDPactivate() != 'O');

  //Attach from GPRS Service
  Serial.println(F("\r\n- AKTIFKAN GPRS SERVICE -"));
  gprsAttach();
  delay(200);

  //mulai task and set APN, user name, password
  Serial.println(F("\r\n- SET TCP IP APN -"));
  gprsCSTT();

  //Bring Up Wireless Connection with GPRS
  //  konek = gprsWirelessConnect();
  Serial.println(F("\r\n- CONNECTING TCP IP -"));
  if (gprsWirelessConnect() == 0) return 0;
  Serial.println(F("- CONNECTED -"));

  //Get Local IP Address
  Serial.println(F("\r\n- LOCAL IP ADDRESS  -"));
  gprsIP();

  return 1;
}

void gprsAttach() {
  // ATTACH gprs SERVICE
  SIM7000.println(F("AT+CGATT=1"));
  cekSerial(200);
}

void gprsShut() {
  //DEAKTIVASI gprsPDP CONTEXT DAN TUTUP BEARER
  SIM7000.println(F("AT+CIPCLOSE"));
  cekSerial(1000);
}

void gsmShut() {
  //DEAKTIVASI gprsPDP CONTEXT DAN TUTUP BEARER
  SIM7000.println(F("AT+CIPSHUT"));
  cekSerial(1000);
}

void gprsCSTT() {
  SIM7000.print(F("AT+CSTT=\""));
  SIM7000.print(IP);
  SIM7000.print(F("\",\""));
  SIM7000.print(USER);
  SIM7000.print(F("\",\""));
  SIM7000.print(PWD);
  SIM7000.println(F("\""));
  cekSerial(5000);
}

boolean gprsWirelessConnect() {
  SIM7000.println(F("AT+CIICR"));
  char g = cekSerial(85000);

  if (g != 'O') {
    return 0;
  }
  return 1;
}

void gprsIP() {
  SIM7000.println(F("AT+CIFSR"));
  bacaserial(200);
}

//char PDPactivate() {
//  // ATTACH gprs SERVICE
//  SIM7000.println(F("AT+CGACT=1,3"));
//  char g = cekSerial(200);
//  return g;
//}

void gprsMux() {
  // ATTACH gprs SERVICE
  SIM7000.println(F("AT+CIPMUX=0"));
  cekSerial(200);
}

void sendServer() {


  Serial.println(F("\r\n<- SET ECHO ->"));
  SIM7000.println(F("AT+CIPSPRT=1"));
  bacaserial(200);
  delay(1000);


  //SET HTTP PARAMETERS VALUE
  Serial.println(F("\r\n<- TCP IP START ->"));
  SIM7000.println("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",\"80\"");
  bacaserial(200);
  cekSerial(16000);
  bacaserial(200);
  delay(2000);

  Serial.println(F("\r\n<- SEND DATA ->"));
  SIM7000.println(F("AT+CIPSEND"));
  bacaserial(200);
  

  // THINGSPEAK GET
  for ( byte i = 0; i < sizeof(json); i++) { //clear variable
    json[i] = (char)0;
  }
  strcpy(json, "GET http://api.thingspeak.com/update?api_key=");
  strcat(json, SECRET_WRITE_APIKEY.c_str());
  strcat(json, "&field1=");
  kalimat = String(tekanan, 2);
  strcat(json, kalimat.c_str());
  Serial.println(json);
  SIM7000.println(json);
  bacaserial(200);
  delay(200);
  SIM7000.write(26);
  Serial.println(millis());
  while (1) {
    char g;
    while (SIM7000.available() > 0) {
      g = SIM7000.read();
      Serial.print(g);
      if (g == 'O') break;
    }
    if (g == 'O') break;
  }
  bacaserial(200);
  Serial.println(millis());

  Serial.println(F("\r\n<- GPRS SHUT ->"));
  gprsShut();
  gsmShut();
}

// variables created by the build process when compiling the sketch
extern int __bss_end;
extern void *__brkval;

// function to return the amount of free RAM
int memoryFree() {
  int freeValue;
  if ((int)__brkval == 0)
    freeValue = ((int)&freeValue) - ((int)&__bss_end);
  else
    freeValue = ((int)&freeValue) - ((int)__brkval);
  return freeValue;
}

void sisamemori() {
  Serial.print(F("Sisa Memori = "));
  Serial.print(memoryFree()); // print the free memory
  Serial.println(' '); // print a space
}
