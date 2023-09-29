// used libraries
#include <SoftwareSerial.h>
#include <IRremote.h>
#include "VoiceRecognitionV3.h"
#include <EEPROM.h>

// define voice command variable with their ID
#define POWER (1)
#define PAUSE (3)
#define RESUME (4)
#define VOLUME (5)
#define UP (6)
#define DOWN (7)
#define CHANNEL (8)
#define NEXT (9)
#define PREVIOUS (10)
#define NUMBER (11)
#define ZERO (12)
#define ONE (13)
#define TWO (14)
#define THREE (15)
#define FOUR (16)
#define FIVE (17)
#define SIX (18)
#define SEVEN (19)
#define EIGHT (20)
#define NINE (21)

// set BT object and pins to connect to Blutooth Module
SoftwareSerial BT(0, 1);
VR myVR(3, 4);
uint8_t records[7];  // save record
uint8_t buf[64];

// string that forms message recived from the app
String state;

// for measuring response time and latency
unsigned long startTimeRT;
unsigned long endTimeRT;
unsigned long startTimeLate;
unsigned long endTimeLate;

// variables used for training remote with IR codes so it can be universal
int numCodesSaved = 0;
unsigned long irCodes[16];
int resetButtonState = 0;

// PIN variables 
const byte BUTTON_PIN_MIC = 2;
const byte LED_PIN = 5;
const byte LED_PIN_POWER = 6;
const byte IR_EMITTER = 7;
const byte LED_VALID_COMMAND = 8;
const byte LED_APP_CONNECT = 9;
const byte BUTTON_CODES_RESET = 10;
const byte IR_LED = 13;

// set with IR recivier and IR LED, so IR codes can be sent and recived
IRsend irsend(IR_LED);
IRrecv IR(IR_EMITTER);

// -------------------------------------------------------------------------------------

// ------ pre-render code for the voice control module that was not written by me. ------

void printSignature(uint8_t *buf, int len) {
  int i;
  for (i = 0; i < len; i++) {
    if (buf[i] > 0x19 && buf[i] < 0x7F) {
      Serial.write(buf[i]);
    } else {
      Serial.print("[");
      Serial.print(buf[i], HEX);
      Serial.print("]");
    }
  }
}

void printVR(uint8_t *buf) {
  Serial.println("VR Index\tGroup\tRecordNum\tSignature");
  Serial.print(buf[2], DEC);
  Serial.print("\t\t");

  if (buf[0] == 0xFF) {
    Serial.print("NONE");
  } else if (buf[0] & 0x80) {
    Serial.print("UG ");
    Serial.print(buf[0] & (~0x80), DEC);
  } else {
    Serial.print("SG ");
    Serial.print(buf[0], DEC);
  }
  Serial.print("\t");
  Serial.print(buf[1], DEC);
  Serial.print("\t\t");
  if (buf[3] > 0) {
    printSignature(buf + 4, buf[3]);
  } else {
    Serial.print("NONE");
  }
  Serial.println("\r\n");
}

// -------------------------------------------------------------------------------------

// ------ all the code from here on is written by me ------

// saves given variables in the ROM so the values remain after the program is restarted.
// this was used to save the IR codes.
void saveEEPROM() {
  // 2 variables are save being, irCodes and numCodesSaved.
  EEPROM.begin();
  EEPROM.put(sizeof(bool), numCodesSaved);
  EEPROM.put(sizeof(bool) + sizeof(int), irCodes);
  EEPROM.end();
}

// loads the saved variables with intact values from the ROM.
void loadEEPROM() {
  // 3 variables are loaded being, irCodes and numCodesSaved.
  EEPROM.begin();
  EEPROM.get(sizeof(bool), numCodesSaved);
  EEPROM.get(sizeof(bool) + sizeof(int), irCodes);
}

// resets the saved variables in the ROM.
// so different IR codes from different remotes can be configured.
void resetEEPROM() {
  // loops the ROM values and sets them all to 0.
  for (int i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0);
  }
}

// used for calculating response time for preformance testing
void startTimerRT() {
  startTimeRT = micros();
}
void endTimerRT() {
  endTimeRT = micros();
}
void calculateRT(unsigned long endTime, unsigned long startTime) {
  unsigned long responseTime = endTime - startTime;
  Serial.print("\nRT: ");
  Serial.print(responseTime);
  Serial.println(" μs");
}

// used for calculating latency for preformance testing
void startTimerLate() {
  startTimeLate = micros();
}
void endTimerLate() {
  endTimeLate = micros();
}
void calculateLate(unsigned long endTime, unsigned long startTime) {
  unsigned long latency = endTime - startTime;
  Serial.print("\nL: ");
  Serial.print(latency);
  Serial.println(" μs");
}


// resets the program which needs to happen if voice control module loses power, which happends if the mic button is not pressed.
void (*resetFunc)(void) = 0;

// loads the inital batch of commands whith, 3 of them being parent commands that loads even more commands as well as clearing the inital commands.
// this was to not cause heavy load onto the arduino to load all the commands at once.
void commandLoad() {
  myVR.clear();
  myVR.load((uint8_t)POWER);    // normal command
  myVR.load((uint8_t)VOLUME);   // parent command
  myVR.load((uint8_t)CHANNEL);  // parent command
  myVR.load((uint8_t)NUMBER);   // parent command
  myVR.load((uint8_t)RESUME);   // normal command
  myVR.load((uint8_t)PAUSE);    // normal command
}

// prints the given IR code to the specified IR LED with "IRsend irsend(IR_LED);".
void printIRcode(int HEX_CODE) {
  // measuring latency. calcuation is done BEFORE the output
  endTimerLate();
  calculateLate(endTimeLate, startTimeLate);
  irsend.sendNEC(HEX_CODE, 32);
  // measuring response time. calcuation is done AFTER the output
  endTimerRT();
  calculateRT(endTimeRT, startTimeRT);
}

// function handels all the app logic
void appLogic() {
  myVR.clear();
  BT.begin(9600);
  bool validCommand = true;

  // for calculating reponse time and latency of the apps voice/button controls
  if (BT.available()) {
    startTimerRT();
    startTimerLate();
  }

  // loops to build the incoming message from the app
  // state string stores the message
  while (BT.available()) {  
    char c = BT.read();     
    state += c;            
  }

  // now checks what message was said to print out coresponding IR code to the message
  if (state.length() > 0) {
    //Serial.println(state);
    if (state == "enter") {
      digitalWrite(LED_APP_CONNECT, HIGH);
    } else if (state == "exit") {
      digitalWrite(LED_APP_CONNECT, LOW);
    } else if (state == "power") {
      printIRcode(irCodes[0]);
    } else if (state == "pause") {
      printIRcode(irCodes[1]);
    } else if (state == "resume") {
      printIRcode(irCodes[2]);
    } else if (state == "number zero" || state == "number 0") {
      printIRcode(irCodes[3]);
    } else if (state == "number one" || state == "number 1") {
      printIRcode(irCodes[4]);
    } else if (state == "number two" || state == "number 2") {
      printIRcode(irCodes[5]);
    } else if (state == "number three" || state == "number 3") {
      printIRcode(irCodes[6]);
    } else if (state == "number four" || state == "number 4") {
      printIRcode(irCodes[7]);
    } else if (state == "number five" || state == "number 5") {
      printIRcode(irCodes[8]);
    } else if (state == "number six" || state == "number 6") {
      printIRcode(irCodes[9]);
    } else if (state == "number seven" || state == "number 7") {
      printIRcode(irCodes[10]);
    } else if (state == "number eight" || state == "number 8") {
      printIRcode(irCodes[11]);
    } else if (state == "number nine" || state == "number 9") {
      printIRcode(irCodes[12]);
    } else if (state == "volume up") {
      for (int i = 0; i < 5; i++) {
        printIRcode(irCodes[13]);
      }
    } else if (state == "volume down") {
      for (int i = 0; i < 5; i++) {
        printIRcode(irCodes[14]);
      }
    } else if (state == "channel next") {
      printIRcode(irCodes[15]);
    } else if (state == "channel previous") {
      printIRcode(irCodes[16]);
    } else {
      validCommand = false;
    }
    // state string is reset to nothing so a new message can be created after new data from the app is loaded.
    state = "";

    // check if input was a valid command.
    if (validCommand) {
      userFeedback();
    }
  }
}

// provides visual feedback to the user of an LED turining on for 2 seconds for valid and successfull actions.
// was also used for testing and debugging
void userFeedback() {
  digitalWrite(LED_VALID_COMMAND, HIGH);
  delay(500);
  digitalWrite(LED_VALID_COMMAND, LOW);
}

// checkResetButton function that checks if the reset button was pressd, if so it resets saved ROM variables.
void checkResetButton() {
  if (digitalRead(BUTTON_CODES_RESET) == LOW) {
    Serial.print("\nIR codes Reset");
    resetEEPROM();
    resetFunc();
  }
}

// COMMENT
void waitForButton() {
  myVR.clear();
  Serial.begin(9600);

  // this loops while the mic button is not pressed
  while (digitalRead(BUTTON_PIN_MIC) == HIGH) {
    appLogic();
    checkResetButton();

    // checks for incoming IR signals 
    if (IR.decode()) {
      // for calculating reponse time and latency of the main button controls
      startTimerRT();
      startTimerLate();
      // save hex code as string, and then unsigned long
      String inputCode = String(IR.decodedIRData.decodedRawData, HEX);
      unsigned long hexCode = strtoul(inputCode.c_str(), NULL, 16);


      // code trains IR signals 
      if (numCodesSaved < 17 && hexCode != 0) {
        irCodes[numCodesSaved] = hexCode;
        numCodesSaved++;
        Serial.print("\nCode captured: ");
        Serial.print(numCodesSaved);
        Serial.print(" - ");
        Serial.print(hexCode);
        userFeedback();
        if (numCodesSaved == 17) {
          saveEEPROM();
          // saves hex code in the ROM
        }
      } else {
        // normal button controls to output hex code through IR LED
        if (hexCode != 0) {
          printIRcode(hexCode);
          Serial.println(hexCode, HEX);
        }
      }
      IR.resume();
    }
  }
}

// -------------------------------------------------------------------------------------

// setup
void setup() {
  // loads variables saved in ROM before anything else
  EEPROM.begin();
  loadEEPROM();

  // set all pin modes
  pinMode(BUTTON_PIN_MIC, INPUT_PULLUP);
  pinMode(BUTTON_CODES_RESET, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_PIN_POWER, OUTPUT);
  pinMode(LED_VALID_COMMAND, OUTPUT);
  pinMode(LED_APP_CONNECT, OUTPUT);
  pinMode(IR_LED, OUTPUT);

  // shows that the device is on
  digitalWrite(LED_PIN_POWER, HIGH);

  IR.enableIRIn();
  myVR.begin(9600);
  Serial.begin(115200);

  // checks if the mic has power or not
  if (myVR.clear() == 0) {
    Serial.println("Recognizer cleared.");
    commandLoad();
  } else {
    waitForButton();
    resetFunc();
  }
}

// -------------------------------------------------------------------------------------

// loop
void loop() {
  static bool lastButtonState = LOW;
  bool currentButtonState = digitalRead(BUTTON_PIN_MIC);

  
  if (currentButtonState == HIGH && lastButtonState == LOW) {
    digitalWrite(LED_PIN, LOW);
    resetFunc();  // reset the program
  }
  lastButtonState = currentButtonState;  // update last button state

  bool validCommand = true;

  int ret;
  ret = myVR.recognize(buf, 50);
  if (ret > 0) {
    startTimerRT();
    startTimerLate();
    switch (buf[1]) {
      case POWER:
        printIRcode(irCodes[0]);
        break;
      case PAUSE:
        printIRcode(irCodes[1]);
        break;
      case RESUME:
        printIRcode(irCodes[2]);
        break;
      case NUMBER:
        digitalWrite(LED_PIN, HIGH);
        myVR.clear();
        myVR.load((uint8_t)SEVEN);  // sub command
        myVR.load((uint8_t)EIGHT);  // sub command
        myVR.load((uint8_t)NINE);   // sub command
        myVR.load((uint8_t)ZERO);   // sub command
        myVR.load((uint8_t)ONE);    // sub command
        myVR.load((uint8_t)TWO);    // sub command
        myVR.load((uint8_t)THREE);  // sub command
        myVR.load((uint8_t)FOUR);   // sub command
        myVR.load((uint8_t)FIVE);   // sub command
        myVR.load((uint8_t)SIX);    // sub command
        startTimerRT();
        startTimerLate();
        break;
      case ZERO:
        printIRcode(irCodes[3]);
        break;
      case ONE:
        printIRcode(irCodes[4]);
        break;
      case TWO:
        printIRcode(irCodes[5]);
        break;
      case THREE:
        printIRcode(irCodes[6]);
        break;
      case FOUR:
        printIRcode(irCodes[7]);
        break;
      case FIVE:
        printIRcode(irCodes[8]);
        break;
      case SIX:
        printIRcode(irCodes[9]);
        break;
      case SEVEN:
        printIRcode(irCodes[10]);
        break;
      case EIGHT:
        printIRcode(irCodes[11]);
        break;
      case NINE:
        printIRcode(irCodes[12]);
        break;
      case VOLUME:
        /** turn on LED */
        digitalWrite(LED_PIN, HIGH);
        myVR.clear();
        myVR.load((uint8_t)UP);    // sub command
        myVR.load((uint8_t)DOWN);  // sub command
        startTimerRT();
        startTimerLate();
        break;
      case UP:
        for (int i = 0; i < 5; i++) {
          printIRcode(irCodes[13]);
        }
        break;
      case DOWN:
        for (int i = 0; i < 5; i++) {
          printIRcode(irCodes[14]);
        }
        break;
      case CHANNEL:
        digitalWrite(LED_PIN, HIGH);
        myVR.clear();
        myVR.load((uint8_t)NEXT);      // sub command
        myVR.load((uint8_t)PREVIOUS);  // sub command
        startTimerRT();
        startTimerLate();
        break;
      case NEXT:
        printIRcode(irCodes[15]);
        break;
      case PREVIOUS:
        printIRcode(irCodes[16]);
        break;
      default:
        Serial.println("Voice command undefined");
        validCommand = false;
        break;
    }
    /** voice recognized */
    printVR(buf);

    // check if input was a valid command.
    if (validCommand) {
      userFeedback();
    }
  }
}
