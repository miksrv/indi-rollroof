/*
   Firmware for a roll off roof. A simple state-machine based on StandardFirmata,
   comminicates with indiserver indi_aldiroof driver.

   Firmata is a generic protocol for communicating with arduino microcontrollers.
   This code based on a the SimpleFirmata and EchoString firmware that comes with the ArduinoIDE (file>examples>firmata)

   4 commands are sent from the driver to this firmware, [ABORT,OPEN,CLOSE,QUERY].
   'QUERY' is used to determine if the roof is fully open or fully closed.
   Unlike the usual firmata scenario, the client does not have direct control over the pins.

   Plug pins to motor controller (this is a custom plug wired beween contactors and motor to enable easy maintenance, not used in code)
   1 Live Supply
   2 Neutral Supply
   3 Close live
   4 Open live
   5 Close neutral
   6 Open neutral
   7 NA
   8 NA

*/
#include <Wire.h>
#include <Firmata.h>

/*==============================================
   ROOF SPECIFIC GLOBAL VARIABLES
  ==============================================*/
// PIN Constants
const int pinRelayOpen        = 3;
const int pinRelayClose       = 2;    
const int pinLimitSwitchClose = 4;
const int pinLimitSwitchOpen  = 5;
const int parkTelescopeRA     = 6;
const int parkTelescopeDEC    = 7;

const int ledPin =  13;
//Roof state constants
const int roofClosed = 0;
const int roofOpen = 1;
const int roofStopped = 2;
const int roofClosing = 3;
const int roofOpening = 4;
//actual state of the roof
volatile int roofState = roofStopped;
volatile int previousRoofState = roofStopped;
long hoistOnTime = 0;
long ledToggleTime = 0;
bool ledState;

/*==============================================================================
   SETUP()
  ============================================================================*/
void setup()
{
  Firmata.setFirmwareVersion(FIRMATA_MAJOR_VERSION, FIRMATA_MINOR_VERSION);
  Firmata.attach(STRING_DATA, stringCallback);
  Firmata.begin(57600);
  pinMode(pinRelayOpen, OUTPUT);
  pinMode(pinRelayClose, OUTPUT);
  
  pinMode(pinLimitSwitchOpen, INPUT);
  pinMode(pinLimitSwitchClose, INPUT);

  pinMode(parkTelescopeRA, INPUT_PULLUP);
  pinMode(parkTelescopeDEC, INPUT_PULLUP);
  
  pinMode(ledPin, OUTPUT);

  motorOff();
}

/*==============================================================================
   LOOP()
  ============================================================================*/
void loop()
{
  while (Firmata.available()) {
    Firmata.processInput();
  }
  handleState();
}

/*==============================================================================
   ROLL OFF ROOF SPECIFIC COMMANDS
  ============================================================================*/
void stringCallback(char *myString)
{
  int openState  = digitalRead(pinLimitSwitchOpen);
  int closeState = digitalRead(pinLimitSwitchClose);
  
  String commandString = String(myString);
  if (commandString.equals("OPEN")) {
    if (digitalRead(parkTelescopeRA) == LOW && digitalRead(parkTelescopeDEC) == LOW) {
      roofState = roofOpening;
    } else {
      Firmata.sendString("NOTELESCOPEPARK");
    }

    roofState = roofOpening;
  } else if (commandString.equals("CLOSE")) {
    if (digitalRead(parkTelescopeRA) == LOW && digitalRead(parkTelescopeDEC) == LOW) {
      roofState = roofClosing;
    } else {
      Firmata.sendString("NOTELESCOPEPARK");
    }

    roofState = roofClosing;
  } else if (commandString.equals("ABORT")) {
    roofState = roofStopped;
  } else if (commandString.equals("QUERY")) {
    
    if (digitalRead(parkTelescopeRA) != LOW && digitalRead(parkTelescopeDEC) != LOW) {
      Firmata.sendString("NOTELESCOPEPARK");
    } else {
      if (openState == HIGH && closeState == LOW) {
        Firmata.sendString("OPEN");
      } else if (closeState == HIGH && openState == LOW) {
        Firmata.sendString("CLOSED");
      } else {
        Firmata.sendString("UNKNOWN");
      }
    }
  }
}

/**
   Handle the state of the roof. Act on state change
*/
void handleState() {
  handleLEDs();
  safetyCutout();
  if (roofState != previousRoofState) {
    previousRoofState = roofState;
    if (roofState == roofOpening) {
      motorFwd();
    } else if (roofState == roofClosing) {
      motorReverse();
    }  else {
      motorOff();
    }
  }
}

/**
   Switch off all motor relays
*/
void motorOff() {
  digitalWrite(pinRelayOpen, HIGH);
  digitalWrite(pinRelayClose, HIGH);
  delay(1000);
}

/**
   Switch on relays to move motor rev
*/
void motorReverse() {
  motorOff();

  digitalWrite(pinRelayOpen, LOW);
  unsigned long activateTime = millis();

  while((millis() - activateTime) < 800) {}
  digitalWrite(pinRelayOpen, HIGH);

  hoistOnTime = millis();
}

/**
   Switch on relays to fwd motor
*/
void motorFwd() {
  motorOff();
  digitalWrite(pinRelayClose, LOW);
  unsigned long activateTime = millis();

  while((millis() - activateTime) < 800) {}
  digitalWrite(pinRelayClose, HIGH);

  hoistOnTime = millis();
}

/**
   Return the duration in miliseconds that the roof motors have been running
*/
long roofMotorRunDuration() {
  if (roofState == roofOpening || roofState == roofClosing) {
    return millis() - hoistOnTime;
  } else {
    return 0;
  }
}


void safetyCutout() {
  if (roofMotorRunDuration() > 20000) {
    if ((roofState == roofOpening && digitalRead(pinLimitSwitchOpen) == HIGH) || (roofState == roofClosing && digitalRead(pinLimitSwitchClose) == HIGH)) {
      roofState = roofStopped;
    }    
  }
}

/**
   LED is used to provide visual clues to the state of the roof controller.
   Really not necessary, but handy for debugging wiring and mechanical problem
*/
void handleLEDs() {
  // Set an LED on if the corresponding fully open switch is on.
  if (digitalRead(pinLimitSwitchClose) == HIGH && digitalRead(pinLimitSwitchOpen) == LOW) {
    toggleLed(50);
  } else if (digitalRead(pinLimitSwitchOpen) == HIGH && digitalRead(pinLimitSwitchClose) == LOW) {
    toggleLed(1000);
  } else {
    digitalWrite(ledPin, LOW);
  }
}

void toggleLed(int duration) {
  if (millis() - ledToggleTime > duration) {
    ledToggleTime = millis();
    if (ledState == false ) {
      ledState = true;
      digitalWrite(ledPin, HIGH);
    } else {
      ledState = false;
      digitalWrite(ledPin, LOW);
    }
  }
}