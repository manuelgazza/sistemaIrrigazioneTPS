#include <Wire.h>
#include <Time.h>
#include <RTClib.h>

#include <LiquidCrystal_I2C.h>

#include <boarddefs.h>
#include <IRremoteInt.h>
#include <IRremote.h>

#define NUM_EV 5

#define OK_BUTTON 16712445
#define ZERO_BUTTON 16730805
#define ONE_BUTTON 16738455
#define TWO_BUTTON 16750695
#define THREE_BUTTON 16756815
#define FOUR_BUTTON 16724175
#define FIVE_BUTTON 16718055
#define STAR_BUTTON 16728765
#define HASHTAG_BUTTON 16732845

#define RECV_PIN 22
#define diRain 31

String hourToMatch = "08:00";

enum STATO {
  READ = 0,
  FETCH,
  ACTIVATE,
  WRITE
};

uint8_t stato;
bool onHour = true;

int diHum[NUM_EV] = {30, 31, 32, 33, 34};
int EVArray[NUM_EV] = {23, 24, 25, 26, 27};

IRrecv irrecv(RECV_PIN);
decode_results results;

RTC_DS3231 rtc;

LiquidCrystal_I2C lcd(0x27, 16, 2);

bool hum[NUM_EV];

void setup() {
  int i = 0;

  Wire.begin();

  if(rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  
  pinMode(22, OUTPUT);

  for(; i < NUM_EV; i++) {
    pinMode(EVArray[i], OUTPUT);
    digitalWrite(EVArray[i], HIGH);
  }
  
  Serial.begin(9600);
  
  irrecv.enableIRIn();

  stato = STATO::READ;
}

void loop() {
  bool rain;
  
  switch (stato) {
    case STATO::READ:
      if(onHour) {
        writeLine("Mode: Time.", 1, 100);
      } else {
        writeLine("Mode: Sensors.", 1, 100);
      }
      
      getTime();
      rain = checkForRain();
      checkHum();
      stato = STATO::FETCH;
      break;
      
    case STATO::FETCH:
      Serial.println("In Fetch");
      fetch();
      stato = STATO::ACTIVATE;
      break;
      
    case STATO::ACTIVATE:
      Serial.println("In Activate");
      if(!rain) {
        if(onHour) {
          atCertainTime();
        } else {
            needsWater();
        }
      }
      stato = STATO::WRITE;
      break;

    case STATO::WRITE:
      writeOnLCD();
      stato = STATO::READ;
      break;
  }
}

void startEV() {
  int i = 0;

  String msg = "Starting EVs.";
  writeLine(msg, 1, 100);
  
  for (; i < NUM_EV; i++) {
    digitalWrite(EVArray[i], LOW);
    delay(100);
    digitalWrite(EVArray[i], HIGH);
    delay(100);
  }
  
}

void writeLine(String messaggio, int line, int delayMS){
  int lunghezza = messaggio.length();
  int i = 0;
  lcd.setCursor(0, line);
  lcd.print(messaggio);
  delay(delayMS);
}

bool checkForRain() {
  bool isRaining = digitalRead(diRain);
  
  return !isRaining;
}

void checkHum() {
  int i = 0;

  for(; i < NUM_EV; i++) {
    hum[i] = digitalRead(diHum[i]);
  }
}

void fetch() {
  String msg;
  Serial.println("In Fetch");
  
  while (irrecv.decode(&results)) {
    switch (results.value) {
      case ZERO_BUTTON:
        startEV();
        lcd.clear();
        break;
        
      case ONE_BUTTON:
        if(diHum[0] == -1) {
          diHum[0] = diHum[1]-1;
        } else {
          diHum[0] = -1;
        }
        
      case TWO_BUTTON:
        if(diHum[1] == -1) {
          diHum[1] = diHum[2]-1;
        } else {
          diHum[1] = -1;
        }
        
      case THREE_BUTTON:
        if(diHum[2] == -1) {
          diHum[2] = diHum[3]-1;
        } else {
          diHum[2] = -1;
        }
        break;
        
      case FOUR_BUTTON:
        if(diHum[3] == -1) {
          diHum[3] = diHum[4]-1;
        } else {
          diHum[3] = -1;
        }
        break;
        
      case FIVE_BUTTON:
        if(diHum[4] == -1) {
          diHum[4] = diHum[3]+2;
        } else {
          diHum[4] = -1;
        }
        break;

       case STAR_BUTTON:
        lcd.clear();
        onHour = true;
        break;

       case HASHTAG_BUTTON:
        onHour = false;
        break;
    }

    writeLine(msg, 1, 100);
    Serial.println(results.value);
    irrecv.resume();
  }
}

void writeOnLCD() {
  String msg;
  
  if(!onHour) {
    String msg = "Mode: Sensors.    ";
  } else {
    String msg = "Mode: Time.";
  }
  writeLine(msg, 1, 100);

  getTime();
}

void needsWater() {
  int i = 0;

  for(; i < NUM_EV; i++) {
    if(diHum[i]) {
      digitalWrite(EVArray[i], LOW);
      delay(500);
      digitalWrite(EVArray[i], HIGH);
    }
  }
}

char atCertainTime() {
  DateTime now;
  now = rtc.now();
  int i;
  bool isEqual = true;

  char cTime[5];
  sprintf(cTime, "%02d/%02d", now.hour(), now.minute());
  
  for(i = 0; i < sizeof(hourToMatch); i++) {
    if(cTime[i] != hourToMatch[i]) {
      isEqual = false;
      break;
    }

    if(isEqual) {
      startEV();
    }
  }

  return *cTime;
}

void getTime() {
  Serial.println("In getTime");

  DateTime now;
  now = rtc.now();
  
  char cTime_ = atCertainTime();
  char currentDateTime[30];
  
  sprintf(currentDateTime, "%02d/%02d/%04d %02d:%02d", now.day(), now.month(), now.year(), now.hour(), now.minute());
  Serial.println(currentDateTime);
  writeLine(currentDateTime, 0, 100);
}
