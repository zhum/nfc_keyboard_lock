/*
 * Digital lock with keyboard (4x4) and NFC.
 * 16 cards are supported. To add card, press 'program' key,
 * then '*' on keyboard, bring the card, them press number,
 * corresponding to this card index.
 * To delete card, press 'program' key, then '#', then card index.
 * To delete ALL cards, press 'program', then '0'.
 * To write open sequence, press 'program', then 'D',
 * then 4 keys of sequence (length can be changed in SEQ_LEN).
 * 
 * Card index can be 0-9 and A-D.
 * 
 * You can change values of secret_*** variables to set sequences
 * for add/del cards without 'program' key. It is INSECURE and not
 * recommended.
 * 
 * Connections:
 * MFRC522:
 *  NSS = 10
 *  SCK = 13
 *  MOSI = 11
 *  MISO = 12
 * 
 * Keyboard:
 *  pins 2-9
 * 
 * A0 = open
 * A1 = blue LED
 * A2 = green LED
 * A3 = red LED
 * A4 = program
 * A5 = buzzer
 * 
 */


#include "pitches.h"

#include <EEPROM.h>

#include <Keypad.h>

#include <SPI.h>
#include <MFRC522.h>
//
// NSS = 10
// SCK = 13
// MOSI = 11
// MISO = 12
// 



#define SS_PIN 10
#define RST_PIN 9
MFRC522 mfrc522(SS_PIN, RST_PIN);       // объект MFRC522
unsigned long uidDec, uidDecTemp;       // для отображения номера карточки в десятичном формате
byte bCounter, readBit;
unsigned long ticketNumber;

const byte ROWS = 4; // 4 строки
const byte COLS = 4; // 4 столбца
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
}; 
byte rowPins[ROWS] = {9, 8, 7, 6}; 
byte colPins[COLS] = {5, 4, 3, 2}; 
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

char entered[16]="x";
char secret_open[]="5173";    //reverse! real sequence will be 3715
char secret_new[]="x*1425*";   // add card !reverse!
char secret_del[]="x*7765*";   // del card !reverse!
char secret_clear[]="x*0921*"; // del all !reverse!

#define NUM_CODES 16
#define OPENER A0
#define IPIN1 A1
#define IPIN2 A2
#define IPIN3 A3
#define KEY_PIN A4

#define RED IPIN3
#define GREEN IPIN2
#define BLUE IPIN1

#define BUZZER A5
#define NOTE1 NOTE_A2
#define BUZ_TIME 50

#define OPEN_TIME 500
#define NONVALID 90909090L

#define Waiting 0
#define NewCard 10
#define DelCard 20
#define NewCode 30
#define Clear   40
#define Program 50

#define OPEN_ADDR 500
#define SEQ_LEN 4

unsigned long codes[NUM_CODES];
unsigned long num_codes;
short int mode=Waiting;

const int ulen=sizeof(unsigned long);

void setup() {
  Serial.begin(9600);     
  SPI.begin();            // init SPI
  mfrc522.PCD_Init();     // init MFRC522
  Serial.println(F("Starting..."));

  EEPROM.get(0, num_codes);
  if(num_codes>NUM_CODES)
    num_codes=NUM_CODES;

  // read all cards
  for(int i=0;i<num_codes;++i){
    EEPROM.get((i+1)*ulen, codes[i]);
  }

  // init pins
  pinMode(OPENER, OUTPUT);
  pinMode(KEY_PIN, INPUT);
  pinMode(IPIN1, OUTPUT);
  pinMode(IPIN2, OUTPUT);
  pinMode(IPIN3, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  analogWrite(OPENER,0);
  analogWrite(IPIN1, 0);
  analogWrite(IPIN2, 0);
  analogWrite(BUZZER, 0);

  read_open_seq();

  // salute by leds!
  //tone1();
  analogWrite(IPIN1, 256);
  delay(500);
  analogWrite(IPIN1, 0);
  analogWrite(IPIN2, 256);
  delay(500);
  analogWrite(IPIN2, 0);
  analogWrite(IPIN3, 256);
  delay(500);
  analogWrite(IPIN3, 0);
}

void read_open_seq(){
  for(int i=0;i<SEQ_LEN;++i)
    EEPROM.get(OPEN_ADDR+i,secret_open[i]);
}

void write_open_seq(const char *seq){
  for(int i=0;i<SEQ_LEN;++i)
    EEPROM.put(OPEN_ADDR+i,secret_open[i]);
}

// key pressed, add it to buffer
void add_key(char key){
  //shift!
  for(int i=sizeof(entered)-1;i>0;--i){
    entered[i]=entered[i-1];
  }
  entered[0]=key;
}

void leds_off(){
  analogWrite(IPIN1, 0);
  analogWrite(IPIN2, 0);
  analogWrite(IPIN3, 0);  
}

// indicate idle mode
void ind_waiting(){
  leds_off();
  Serial.println(F("waiting mode..."));
}

// indicate waiting for card index enter (blue led)
void ind_get_index(){
  Serial.println(F("Enter index:"));
  leds_off();
  analogWrite(BLUE, 255);
}

// indicate success (short green led flash, beep)
void ind_success(){
  Serial.println(F("Success!"));
  leds_off();
  analogWrite(GREEN, 255);
  tone1();
  delay(300);
  analogWrite(GREEN, 0);
}

// indicate operation cancelling (short magenta flash)
void ind_cancel(){
  Serial.println(F("Cancelled."));
  leds_off();
  analogWrite(BLUE, 255);
  analogWrite(RED, 255);
  delay(300);
  leds_off();
}

// indicate open (short green+blue flash)
void ind_opening(){
  Serial.println(F("Opening..."));
  leds_off();
  analogWrite(GREEN, 255);
  analogWrite(BLUE, 255);
  delay(300);
  leds_off();
}

// play short buzzer tone
void tone1(){
    tone(BUZZER, NOTE1, BUZ_TIME);
    delay(BUZ_TIME);
    noTone(BUZZER);  
}

// indicate fail (red led flash, two beeps)
void ind_failed(){
  Serial.println(F("Failed!"));
  leds_off();
  analogWrite(RED, 255);
  tone1();
  delay(100);
  tone1();
  delay(500);
  analogWrite(RED, 0);
}

// erase all cards!!!
void clear_all(){
  EEPROM.put(0, 0L);// zero cards
  for(int i=0;i<NUM_CODES;++i){
    EEPROM.put((i+1)*ulen, NONVALID);
  }
  num_codes=0;
}

// add new card
void add_code(unsigned long id){
  char key='\0';
  
  ind_get_index();

  while(!key){
    key = keypad.getKey();
    delay(10);
  }
  tone1();
  if(key=='*' || key=='#'){
    ind_cancel();
    return;
  }
  key-='0';
  if(key>9){
    key=key+'0'-'A'+10;
  }
  key+=1;
  if(key>num_codes)
    num_codes=key;
  EEPROM.put(key*ulen,id);
  ind_success();
  
  Serial.print(F("Added new card: "));
  Serial.println(key-1);

}

// delete card
void del_code(){
  char key='\0';
  
  Serial.println(F("Choose index"));

  while(!key){
    key = keypad.getKey();
    delay(10);
  }
  tone1();
  if(key=='*' || key=='#'){
    ind_cancel();
    return;
  }
  key-='0';
  if(key>9){
    key=key+'0'-'A'+10;
  }
  key+=1;
  EEPROM.put(key*ulen,NONVALID);
  ind_success();
  Serial.print(F("Deleted card: "));
  Serial.println(key-1);
}

// new open sequence
void new_seq(){
  static char key;
  
  Serial.println(F("New open sequence"));

  for(int i=SEQ_LEN-1; i>=0; --i){
    key='\0';
    while(!key){
      key = keypad.getKey();
      delay(100);
    }
    tone1();
    secret_open[i]=key;
  }
  secret_open[SEQ_LEN]='\0'; // for sure...
  write_open_seq(secret_open);
  ind_success();
  Serial.print(F("New reversed sequence: "));
  Serial.println(secret_open);
}

// check card (1=success, 0=fail)
int check_code(unsigned long id){
  static unsigned long saved;
  
  for(int i=1;i<=num_codes; ++i){
    EEPROM.get(i*ulen,saved);
    if(saved==id){
      Serial.print(F("Card index="));
      Serial.println(i-1);
      //Serial.println(F(". Opening..."));
      ind_success();
      return 1;
    }
  }
  ind_failed();
  return 0;
}

// open the door!!!
void do_open(){
  ind_opening();
  analogWrite(OPENER,255);
  delay(OPEN_TIME);
  analogWrite(OPENER,0);
}



void do_program(){
  if(entered[0]=='*'){  //add card
    //add card
    mode=NewCard;
  }
  else if(entered[0]=='#'){ // del card
    //delete card
    del_code();
    mode=Waiting;
  }
  else if(entered[0]=='0'){ // del all cards
    clear_all();
    mode=Waiting;
  }
  else if(entered[0]=='D'){  // new open sequence
    new_seq();
    mode=Waiting;
  }
  else{
    ind_cancel();
    mode=Waiting;  
  }
}

/******************************************************************
 * 
 *****************************************************************/

void loop() {
  static char key;
  static int pressed;
  
  key = keypad.getKey();
  if (key){
    //Serial.println(key);
    Serial.println(entered);
    tone1();
    add_key(key);
    if(mode==Program)
      return do_program();
    if(strncmp(secret_new,entered,strlen(secret_new))==0){
      Serial.println(F("Adding card!"));
      mode=NewCard;
    }
    else if(strncmp(secret_del,entered,strlen(secret_del))==0){
      Serial.println(F("Del card!"));
      del_code();
      mode=Waiting;
    }
    else if(strncmp(secret_clear,entered,strlen(secret_clear))==0){
      Serial.println(F("Clear all!"));
      clear_all();
      mode=Waiting;
    }
    else if(strncmp(secret_open,entered,strlen(secret_open))==0){
      Serial.println(F("Open!!"));
      do_open();
      mode=Waiting;
    }
  }

  // if 'program' pressed?
  pressed=analogRead(KEY_PIN);
  if(pressed>100){
    delay(100); // DUMMY DEBOUNCE
    Serial.println(F("Program!"));
    mode=Program;
    return;
  }
  
  // check for card bringing
  if( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  Serial.println(F("Card bringed!"));
        
  // read card serial
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    Serial.println(F("Cannot read serial!"));
    return;
  }

   uidDec = 0;

   // print card serial
   Serial.print(F("Card UID: "));
   for (byte i = 0; i < mfrc522.uid.size; i++) {
     // Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
     // Serial.print(mfrc522.uid.uidByte[i], HEX);
     uidDecTemp=mfrc522.uid.uidByte[i];
     uidDec=uidDec*256+uidDecTemp;
   } 
   Serial.println(uidDec);
   Serial.println();

   // do action using bringed card and mode
   switch(mode){
     case Waiting:
       if(check_code(uidDec)){
         do_open();
       }
       break;
     case NewCard:
       //Serial.println(F("--------->"));
       add_code(uidDec);
       mode=Waiting;
       break;
     default:
       Serial.print(F("invalid mode="));
       Serial.println(mode);
   }
}

/*************************************************
 * 
 * Unused code
 * 
 */








////////////////////////////////////////////////////////////////
//
//   Metro
//
////////////////////////////////////////////////////////////////
void printIssueDate(unsigned int incoming) {

boolean isLeap = true; // признак високосного года
int days[]={0,31,59,90,120,151,181,212,243,273,304,334}; // последний по порядку день месяца для обычного года
byte dayOfMonth, monthCounter;
unsigned int yearCount;

incoming = incoming+1; // подогнал под ответ, но возможно это как раз необходимая коррекция, потому что начало отсчета - 01.01.1992, а не 00.01.1992

for (yearCount = 1992; incoming >366; yearCount++) { // считаем год и количество дней, прошедших с выдачи билета

        if ((yearCount % 4 == 0 && yearCount % 100 != 0) ||  yearCount % 400 == 0) {
                incoming = incoming - 366;
                isLeap = true;
          } else {
                incoming = incoming - 365;
                isLeap = false;
                }
        }

for (monthCounter = 0; incoming > days[monthCounter]; monthCounter++) { // узнаем номер месяца
}

// считаем день месяца

if (isLeap == true) { // если високосный год
  if (days[monthCounter-1]>31) { // если не первый месяц, то добавляем к последнему дню месяца единицы
  dayOfMonth = incoming - (days[monthCounter-1]+ 1);
  } else {
    dayOfMonth = incoming - (days[monthCounter-1]); // если первый - ничего не добавляем, потому что сдвиг начинается с февраля
  }
} else {
  dayOfMonth = incoming - (days[monthCounter-1]); // если не високосный год
}

Serial.print(dayOfMonth);
Serial.print(".");
Serial.print(monthCounter);
Serial.print(".");
Serial.print(yearCount);
Serial.println();

        

}




void setBitsForGood(byte daBeat) {


  
        if (daBeat == 1) {
                bitSet(ticketNumber, bCounter);
                bCounter=bCounter+1;
                }
        else {
                bitClear(ticketNumber, bCounter);
                bCounter=bCounter+1;
        }
}

void metro(){
        // Выдача типа карты
        MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak); // запрос типа
        Serial.print("Card type: ");
        Serial.println(mfrc522.PICC_GetTypeName(piccType)); // трансляция типа в читаемый вид
        
        if (piccType != MFRC522::PICC_TYPE_MIFARE_UL) { // если не билетная карта
                Serial.print("Not a valid card: "); // так и говорим
                Serial.println(piccType);                               
                // Halt PICC
                mfrc522.PICC_HaltA();                   // остановка чипа
                return;
        }

// сюда мы приедем, если чип правильный

        MFRC522::StatusCode status;
        byte byteCount;
        byte buffer[18]; // длина массива (16 байт + 2 байта контрольная сумма) 
        byte pages[2]={4, 8}; // страницы с данными
        byte pageByte; // счетчик байтов страницы
        
        byteCount = sizeof(buffer);
        byte bCount=0;
                

        for (byte i=0; i<2; i++) { // начинаем читать страницы
        status = mfrc522.MIFARE_Read(pages[i], buffer, &byteCount);
        
        if (status != MFRC522::STATUS_OK) {
                Serial.print("Read error: ");
                Serial.println(mfrc522.GetStatusCodeName(status));}
                else {
                      if (pages[i] == 4) {
                                bCounter = 0; // 32-битный счетчик для номера
                                
                                // биты 0-3
                                for (bCount=0; bCount<4; bCount++) {
                                        readBit = bitRead(buffer[6], (bCount+4));
                                        setBitsForGood(readBit);
                                }

                                // биты 4 - 27
                                for (pageByte=5; pageByte > 2; pageByte--) {
                                        for (bCount=0; bCount<8; bCount++) {
                                                readBit = bitRead(buffer[pageByte], bCount);
                                                setBitsForGood(readBit);
                                        }
                                }

                                // биты 28-31
                                for (bCount=0; bCount<4; bCount++) {
                                        readBit = bitRead(buffer[2], bCount);
                                        setBitsForGood(readBit);
                                }

                                Serial.print("Ticket number: ");
                                Serial.println(ticketNumber, DEC);
                         }
                        
                          if (pages[i] == 8) { // читаем дату выдачи

                                Serial.print("Issued: ");
                                unsigned int issueDate = buffer[0] * 256 + buffer[1]; // количество дней с 01.01.1992 в десятичном формате, 256 - сдвиг на 8 бит
                                printIssueDate(issueDate);

                                Serial.print("Good for (days): "); // срок действия
                                Serial.print(buffer[2], DEC);                                                           
                                Serial.println();                               
                        
                                Serial.print("Trip reminder: "); // количество оставшихся поездок
                                Serial.print(buffer[5], DEC);                                                           
                                Serial.println();       
                        }
                        
                }
        }
        // Halt PICC
    mfrc522.PICC_HaltA();                         
}


