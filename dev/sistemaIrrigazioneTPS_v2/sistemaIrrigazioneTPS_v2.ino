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

enum STATO {
  READ = 0,
  FETCH,
  ACTIVATE,
  HOUR,
  AUTO
};

uint8_t stato;
bool onHour;

int diHum[NUM_EV] = {30, 31, 32, 33, 34};
int EVArray[NUM_EV] = {23, 24, 25, 26, 27};

IRrecv irrecv(RECV_PIN);
decode_results results;

LiquidCrystal_I2C lcd(0x27, 16, 2);

bool hum[NUM_EV];

void setup() {
  int i = 0;

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
      Serial.println("In Read");
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
      if(onHour) {
        atCertainTime();
      } else {
        needsWater();
      }
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
  lcd.clear();
  
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
  String msg;

  if(isRaining){
    msg = "Rain: No.";
  } else {
    msg = "Rain: Yes.";
  }

  // writeLine(msg, 0, 100);

  return isRaining;
}

void checkHum() {
  int i = 0;

  for(; i < NUM_EV; i++) {
    hum[i] = digitalRead(diHum[i]);
  }

  Serial.println("ENDED CHECKHUM");
}

void fetch() {
  String msg;
  Serial.println("In Fetch");
  
  while (irrecv.decode(&results)) {
    switch (results.value) {
      case ZERO_BUTTON:
        startEV();
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

void needsWater() {
  int i = 0;
  lcd.clear();
  String msg = "Mode: Sensors.";

  for(; i < NUM_EV; i++) {
    if(diHum[i]) {
      digitalWrite(EVArray[i], LOW);
      delay(500);
      digitalWrite(EVArray[i], HIGH);
    }
  }

  writeLine(msg, 0, 100);
}

void atCertainTime() {
  lcd.clear();
  String msg = "Mode: Certain Time.";

  writeLine(msg, 0, 100);
}
