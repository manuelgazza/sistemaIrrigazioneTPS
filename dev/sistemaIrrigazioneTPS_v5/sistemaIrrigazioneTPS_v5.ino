/* Importo Libreria Wire per gestire il protocollo I2C */
#include <Wire.h>

/* Importo le Librerie per gestire il Real Time Clock */
#include <Time.h>
#include <RTClib.h>

/* Importo la Libreria per gestire il protocollo I2C dell'LCD */
#include <LiquidCrystal_I2C.h>

/* Importo Librerie per gestire il sensore ad infrarossi */
#include <IRremoteInt.h>
#include <IRremote.h>

/* Definizione Pulsanti Telecomando */
#define OK_BUTTON 16712445
// #define DOWN_BUTTON 16754775
#define DOWN_BUTTON 2747854299
#define UP_BUTTON 16736925
#define ZERO_BUTTON 16730805
#define ONE_BUTTON 16738455
#define TWO_BUTTON 16750695
#define THREE_BUTTON 16756815
#define FOUR_BUTTON 16724175
#define FIVE_BUTTON 16718055
#define STAR_BUTTON 16728765    /* Time Mode */
#define HASHTAG_BUTTON 16732845 /* Sensors Mode */

/* definisco il numero di elettrovalvole */
#define NUM_EV 5

/* Setto il pin del Ricevitore Infrarossi */
#define RECV_PIN 22

/* Definisco il pin di entrata del sensore di pioggia */
#define diRain 31

/* La variabile contiene il valore orario da raggiongere per poter innaffiare */
String hourToMatch = "08:00";

/* Definisco un enum per lo stato */
enum STATO {
  READ = 0,
  FETCH,
  ACTIVATE,
  WRITE
};

/* Definisco una variabile int a 8 bit per lo stato */
uint8_t stato;

/* Il primo avvio viene fatto con la modalità a tempo */
bool onHour = true;

/* Dichiaro un vettore che contiene lo stato del sensore di umidità (true = attivo) */
int humBool[NUM_EV] = {true, true, true, true, true};

/* Dichiaro un vettore con i pin dei digital input dei sensori di umidità */
int diHum[NUM_EV] = {30, 31, 32, 33, 34};

/* Dichiaro un vettore con i pin del digital output delle elettrovalvole */
int EVArray[NUM_EV] = {23, 24, 25, 26, 27};

/* Dichiaro un vettore di booleani dove chiederò se la pianta ha bisgno di acqua */
bool hum[NUM_EV];

/* Creo un'istanza del ricevitore ad infrarossi */
IRrecv irrecv(RECV_PIN);

/* Metto i risultati del decoding del sensore in una variabile di tipo decode_results */
decode_results results;

/* Creo un'istanza del Real Time Clock */
RTC_DS3231 rtc;

/* Creo un'istanza del LCD */
LiquidCrystal_I2C lcd(0x27, 16, 2);


 /* * * * * * * * * * * * * * * * * * * * * * * /
 *  Funzione: setup()                          *
 *  Params: -\-                                *
 *  Info: -\-                                  *
 / * * * * * * * * * * * * * * * * * * * * * * */
 
void setup() {
  int i = 0;

  Wire.begin();

  /* Inizializzo la data a quella dell'ora della compilazione */
  if(rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  /* Inizializzo il Liquid Crystal Display */
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);

  /* Seleziono modalità IR */
  pinMode(22, OUTPUT);

  /* Azzero tutte le ElettroValvole (lavorano in logica invertita) */
  for(; i < NUM_EV; i++) {
    pinMode(EVArray[i], OUTPUT);
    digitalWrite(EVArray[i], HIGH);
  }

  /* Inizializzo la Seriale a 9600 baud */
  Serial.begin(9600);

  /* Attivo la ricezione IR */
  irrecv.enableIRIn();

  /* Inizializzo la macchina a stati */
  stato = STATO::READ;
}


 /* * * * * * * * * * * * * * * * * * * * * * * /
 *  Funzione: loop()                           *
 *  Params: -\-                                *
 *  Info: -\-                                  *
 / * * * * * * * * * * * * * * * * * * * * * * */
 
void loop() {
  bool rain;
  
  switch (stato) {
    
    /* Leggo sensori e tipo di modalità */
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

    /* Ricevo seganli IR */
    case STATO::FETCH:
      Serial.println("In Fetch");
      fetch();
      stato = STATO::ACTIVATE;
      break;

    /* Attivo i sensori all'occorrenza */
    case STATO::ACTIVATE:
      Serial.println("In Activate");
      if(!rain) {
        if(onHour) {
          atCertainTime();
        } else {
            needsWater();
        }
      }
      stato = STATO::READ;
      break;
  }
}


 /* * * * * * * * * * * * * * * * * * * * * * * /
 *  Funzione: startEV()                        *
 *  Params: -\-                                *
 *  Info: Starts all EVs.                      *
 / * * * * * * * * * * * * * * * * * * * * * * */
 
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


 /* * * * * * * * * * * * * * * * * * * * * * * /
 *  Funzione: writeLine()                      *
 *  Params: messaggio: message to print        *
 *          line: where the msg will be printed*
 *          delayMS: delay in milliseconds     *
 *  Info: Writes a String to the LCD           *
 / * * * * * * * * * * * * * * * * * * * * * * */
 
void writeLine(String messaggio, int line, int delayMS){
  int lunghezza = messaggio.length();
  int i = 0;
  lcd.setCursor(0, line);
  lcd.print(messaggio);
  delay(delayMS);
}


 /* * * * * * * * * * * * * * * * * * * * * * * /
 *  Funzione: checkForRain()                   *
 *  Params: -\-                                *
 *  Info: Check if is not raining              *
 / * * * * * * * * * * * * * * * * * * * * * * */

bool checkForRain() {
  bool isRaining = digitalRead(diRain);
  
  return !isRaining;
}


 /* * * * * * * * * * * * * * * * * * * * * * * /
 *  Funzione: checkHum()                       *
 *  Params: -\-                                *
 *  Info: Checks any sensor to know if the     *                            
 *        humidity is over the selected level  *
 / * * * * * * * * * * * * * * * * * * * * * * */
 
void checkHum() {
  int i = 0;

  for(; i < NUM_EV; i++) {
    hum[i] = digitalRead(diHum[i]);
  }
}


 /* * * * * * * * * * * * * * * * * * * * * * * /
 *  Funzione: fetch()                          *
 *  Params: -\-                                *
 *  Info: checks the information sent with the *
 *        IR Remote                            *
 / * * * * * * * * * * * * * * * * * * * * * * */
 
void fetch() {
  String msg;
  Serial.println("In Fetch");
  
  while (irrecv.decode(&results)) {
    switch (results.value) {

      /* Alzo di 1 l'ora */
      case UP_BUTTON:
        raiseHour();
        break;

      /* Abbasso di 1 l'ora */
      case DOWN_BUTTON:
        lowerHour();
        break;

      /* Start Manuale */
      case ZERO_BUTTON:
        startEV();
        lcd.clear();
        break;

      /* Inverto lo stato (attivo/non attivo) */
      case ONE_BUTTON:
        humBool[0] = !humBool[0];
        break;

      case TWO_BUTTON:
        humBool[1] = !humBool[1];
        break;
        
      case THREE_BUTTON:
        humBool[2] = !humBool[2];
        break;
        
      case FOUR_BUTTON:
        humBool[3] = !humBool[3];
        break;
        
      case FIVE_BUTTON:
        humBool[4] = !humBool[4];
        break;

       /* Seleziono la modalità a Tempo */
       case STAR_BUTTON:
        lcd.clear();
        onHour = true;
        break;

       /* Seleziono la modalità a sensori */
       case HASHTAG_BUTTON:
        onHour = false;
        break;
    }
    
    /* Scrivo il valore IR ricevuto e riprendo la lettura */
    Serial.println(results.value);
    irrecv.resume();
  }
}


 /* * * * * * * * * * * * * * * * * * * * * * * /
 *  Funzione: needsWater()                     *
 *  Params: -\-                                *
 *  Info: Turn on the EV if the corresponding  *                               
 *        plant needs water                    *
 / * * * * * * * * * * * * * * * * * * * * * * */
 
void needsWater() {
  int i = 0;

  /* Attivo le elettrovalvole delle piante che necessitano di acqua */
  for(; i < NUM_EV; i++) {
    if(diHum[i]) {
      digitalWrite(EVArray[i], LOW);
      delay(500);
      digitalWrite(EVArray[i], HIGH);
    }
  }
}


 /* * * * * * * * * * * * * * * * * * * * * * * /
 *  Funzione: atCertainTime()                  *
 *  Params: -\-                                *
 *  Info: checks if a certain time is reached  *
 / * * * * * * * * * * * * * * * * * * * * * * */
 
char atCertainTime() {
  DateTime now;
  now = rtc.now();
  int i;
  bool isEqual = true;

  lcd.setCursor(11, 0);
  lcd.print(hourToMatch);

  char cTime[5];
  sprintf(cTime, "%02d:%02d", now.hour(), now.minute());
  
  for(i = 0; i < sizeof(hourToMatch); i++) {
    if(cTime[i] != hourToMatch[i]) {
      isEqual = false;
      break;
     }
    }
    
    if(isEqual) {
      Serial.println("Reached Time.");
      startEV();
    }

  return *cTime;
}


 /* * * * * * * * * * * * * * * * * * * * * * * /
 *  Funzione: getTime()                        *
 *  Params: -\-                                *
 *  Info: Gets and prints periodically-updated *                                
 *        time                                 *
 / * * * * * * * * * * * * * * * * * * * * * * */
 
void getTime() {
  Serial.println("In getTime");

  DateTime now;
  now = rtc.now();
  
  char cTime_ = atCertainTime();
  char currentDateTime[30];
  
  sprintf(currentDateTime, "%02d/%02d/%04d", now.day(), now.month(), now.year());
  Serial.println(currentDateTime);
  writeLine(currentDateTime, 0, 100);
}


 /* * * * * * * * * * * * * * * * * * * * * * * /
 *  Funzione: lowerHour()                      *
 *  Params: -\-                                *
 *  Info: Lowers the hour of 1 hour            *                                
 / * * * * * * * * * * * * * * * * * * * * * * */
 
void lowerHour() {
  int iHour;
  char aHourToMatch[5];
  char cHour[2] = {hourToMatch[0], hourToMatch[1]};

  iHour = atoi(cHour);

  if(iHour >= 0) {
    iHour--;
  }
  
  sprintf(aHourToMatch, "%02d:00", iHour);
  hourToMatch = aHourToMatch;
}


 /* * * * * * * * * * * * * * * * * * * * * * * /
 *  Funzione: raiseHour()                      *
 *  Params: -\-                                *
 *  Info: Raises the hour of 1 hour            *                                
 / * * * * * * * * * * * * * * * * * * * * * * */
 
void raiseHour() {
  int iHour;
  char aHourToMatch[5];
  char cHour[2] = {hourToMatch[0], hourToMatch[1]};

  iHour = atoi(cHour);

  if(iHour <= 24) {
    iHour++;
  }
  
  sprintf(aHourToMatch, "%02d:00", iHour);
  hourToMatch = aHourToMatch;
}
