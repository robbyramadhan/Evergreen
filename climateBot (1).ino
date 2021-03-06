#include <OneWire.h>
#include <DallasTemperature.h>
#include <farmBot.h>
#include <EEPROM.h>

/* I2C interface requirements */
#include <Wire.h>
 
/* allowcation of 1024 EEPROM  */
const byte I2CID = 0;  

int number = 0;
int state = 0;
double temp;
/* end I2C interface requirements */

// Pin assignment
const int ONE_WIRE_BUS = 3;        // air temperature pin in
const int lightsPin = 4;          // lights pin out
const int circulationFanPin = 13;  // circulation fan pin out

const int NORMAL = 1;             // processing state
const int SPECIAL = 2;           // processing state
const int OFF = 0;                // device mode
const int ON = 1;                 // device mode
const int AUTO = 3;               // device mode

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// Assign the addresses of your 1-Wire temp sensors.
// See the tutorial on how to obtain these addresses:
// http://www.hacktronics.com/Tutorials/arduino-1-wire-address-finder.html

DeviceAddress airThermometer = { 0x28, 0xFF, 0xA7, 0xF2, 0x05, 0x00, 0x00, 0x69 };
//DeviceAddress outsideThermometer = { 0x28, 0x6B, 0xDF, 0xDF, 0x02, 0x00, 0x00, 0xC0 };
//DeviceAddress dogHouseThermometer = { 0x28, 0x59, 0xBE, 0xDF, 0x02, 0x00, 0x00, 0x9F };

int systemMode = NORMAL;          // processing state (NORMAL, FILLTANK)

int lightsMode = AUTO;            // lights mode
int lightsState = LOW;            // lights current state
int lightsOnTime = 3;          // minutes that lights are ON
int lightsOffTime = 2;         // minutes that lights are OFF
unsigned long lightsTimer = 0;    // clock time when lights state should toggle

int circulationFanMode = AUTO;               // circulation fan mode
int circulationFanState = LOW;               // circulation fan current state
int circulationFanOnTime = 1;             // minutes that fan is ON
int circulationFanOffTime = 2;            // minutes that fan is OFF
unsigned long circulationFanTimer = 5000;       // clock time when fan state should toggle

byte airTempMSB = 83;            // thermometer integer full units ie. 93
byte airTempLSB = 2;            // thermometer rounded to one decimal place to the right ie. .2 so together 93.2
byte airTempTarget = 78;
byte airTempDelta = 3;
int airTempSampleRate = 3;      // seconds between samples
unsigned long airTempTimer = 0;

byte cmdRecieved = 0;
int cmdCnt;
int command;

void setup() {
  Serial.begin(9600);
  
  pinMode(lightsPin, OUTPUT);   
  digitalWrite(lightsPin, lightsState);      // set initial lights state 
  
  pinMode(circulationFanPin, OUTPUT);   
  digitalWrite(circulationFanPin, circulationFanState);            // set initial fan state 

  i2cSetup();
  oneWireSetup();
//  flowSetup();

} // setup

void loop() {
 
  switch (systemMode) {
     case NORMAL: normalMode(); break;
     case SPECIAL: specialMode(); break;
     default: normalMode(); break;
  }  
  
} // loop

void normalMode()
{
  if (circulationFanMode == AUTO) circulationFanControl();
  if (lightsMode == AUTO) lightsControl();
  airTemp();
} // normalMode

void airTemp()
{
  if (checkTimer(airTempTimer) == HIGH) {
    getAirTemp();
    airTempTimer = setSecondsTimer(airTempSampleRate);    // change value to mean seconds
//    Serial.print("sample temp \n");
  }
}  // airTemp

void circulationFanControl()
{
  if (airTempMSB >= airTempTarget)  // turn fan on
  { 
    circulationFanState = HIGH;
  }
  if (airTempMSB < airTempTarget - airTempDelta)  // turn fan off
  { 
    circulationFanState = LOW;
  }
  digitalWrite(circulationFanPin, circulationFanState);  
}  // circulationFanControl

void lightsControl()
{
  if (checkTimer(lightsTimer) == HIGH) {
    lightsState = !lightsState;
    digitalWrite(lightsPin, lightsState);
    if (lightsState == HIGH) {
      lightsTimer = setMinutesTimer(lightsOnTime);
    } else {
     lightsTimer = setMinutesTimer(lightsOffTime);
    }
  }
} // end lightsControl

void specialMode()
{
  systemMode = NORMAL;
} // specialMode

unsigned long setMinutesTimer(int waitTime)
{ 
  unsigned long endTime;

   endTime = millis() + (waitTime * 60000);  // convert back to milliseconds from minutes
   return endTime;
} // setMinutesTimer

unsigned long setSecondsTimer(int waitTime)
{ 
  unsigned long endTime;

   endTime = millis() + (waitTime * 1000);  // convert back to milliseconds from seconds
   return endTime;
} // setSecondsTimer

int checkTimer(unsigned long timer)
{
   if (millis() > timer) {return HIGH;}
   else {return LOW;}
} // checkTimer

void debouncePin(DEBOUNCE_DEF& target) {
  long debounceDelay = 50;    // the debounce time; increase if the output flickers

   // read the state of the pin into a local variable:
  int reading = digitalRead(target.pin);

  // check to see if pin state has changed and you've waited
  // long enough since the last test to ignore any noise:  
  if (reading != target.pinState) {
    // reset the debouncing timer
    target.lastDebounceTime = millis();
    target.pinStable = LOW;
        if (target.pinOldState == LOW) {target.pinOldState = HIGH;} else {target.pinOldState = LOW;}
 } 
  
  if ((millis() - target.lastDebounceTime) > debounceDelay) {
    // whatever the reading is at, it's been there for longer
    // than the debounce delay, so take it as the actual current state:
	  target.pinStable = HIGH;
   }
  target.pinState = reading;
} // debouncePin

/* IC2 support */

void i2cSetup() {
  EEPROM.write(I2CID, 0x06);
  // set default i2c ID if not yet defined
 // if (EEPROM.read(I2CID)==0) { EEPROM.write(I2CID, 0x4); }
 
 // initialize i2c as slave
 Wire.begin(EEPROM.read(I2CID));
 
 // define callbacks for i2c communication
 Wire.onReceive(i2cCommand);
 Wire.onRequest(myRequest);
} // i2cSetup

byte i2cResponse[32];
int index = 0;

void myRequest() {    // callback for sending data
  Wire.write(i2cResponse[index]);
  ++index;
}
  
void i2cCommand(int byteCount){ // callback for received data
  unsigned char received;
//  int cmdCnt;
//  int command;
  int output;
 
  cmdCnt = Wire.available();
  if (cmdCnt > 0 ) {
    cmdRecieved++;
    command = Wire.read();      // first byte is the command
    switch (command)
    {              
      case 1:    // set Lights Mode (Off, On, Auto)
        if (cmdCnt == 2 ) {    // next byte is the mode
          lightsMode = Wire.read();      
          switch (lightsMode) {
            case ON: lightsState = HIGH; break;
            case OFF: lightsState = LOW; break;
          }
          digitalWrite(lightsPin, lightsState);
           Serial.print("Lights state = ");
           Serial.println(lightsState);
       }
        break;
        
      case 2:    // set Lights auto ON time
        if (cmdCnt == 3 ) {    // next 2 bytes is the time
          received = Wire.read();
          output = (received << 8);  // shift to high byte
          received = Wire.read();
          output = output + received; // add in low byte
          lightsOnTime = output;      // ON time in seconds
        }
         break;
        
      case 3:    // set Lights auto OFF time
        if (cmdCnt == 3 ) {    // next 2 bytes is the time
          received = Wire.read();
          output = (received << 8);    // shift to high byte
          received = Wire.read();
          output = output + received;  // add in low byte
          lightsOffTime = output;      // OFF time in seconds
        }
        break;
        
      case 4:    // set Circulation Fan (Off, On, Auto)
        if (cmdCnt == 2 ) {    // next byte is the mode
          circulationFanMode = Wire.read();      
          switch (circulationFanMode) {
            case ON: circulationFanState = HIGH; digitalWrite(circulationFanPin, circulationFanState); break;
            case OFF: circulationFanState = LOW; digitalWrite(circulationFanPin, circulationFanState); break;
          }
        }
        break;
           
      case 5:    // set Circulation Fan auto ON time
        if (cmdCnt == 3 ) {    // next 2 bytes is the time
          received = Wire.read();
          output = (received << 8);    // shift to high byte
          received = Wire.read();
          output = output + received;  // add in low byte
          circulationFanOnTime = output;          // ON time in seconds
        }
        break;
      
      case 6:    // set Circulation Fan auto OFF time
       if (cmdCnt == 3 ) {    // next 2 bytes is the time
          received = Wire.read();
          output = (received << 8);    // shift to high byte
          received = Wire.read();
          output = output + received;  // add in low byte
          circulationFanOffTime = output;         // OFF time in seconds
        }
        break;

      case 8:
          i2cRequest();
          index = 0;
          break;
             
       default:
         Serial.print("Unhandled command # ");
         Serial.print(command);
         Serial.print("\n");;
         break;
   } // end switch 
  } // end if cmdCnt
  
} // ic2Command
 

void i2cRequest() {    // callback for sending data
  byte i2cResponseLen = 0;                 // response length
  unsigned long currentTime = millis();
  unsigned long transitionTime = 0;
  byte lightsStatus = 0;
  byte circulationFanStatus = 0;
    
    i2cResponse[i2cResponseLen] = (byte)(airTempMSB);  // left of decimal byte
    i2cResponseLen++;
    i2cResponse[i2cResponseLen] = (byte)(airTempLSB);  // right if decimal byte
    i2cResponseLen++;
    
    i2cResponse[i2cResponseLen] = airTempTarget;  // byte
    i2cResponseLen++;  

    i2cResponse[i2cResponseLen] = airTempDelta;  // byte
    i2cResponseLen++;  

    lightsStatus = (lightsMode << 6) | (lightsState << 5 );
    circulationFanStatus = (circulationFanMode << 3) | circulationFanState << 2;
    i2cResponse[i2cResponseLen] = lightsStatus | circulationFanStatus;      //  lights, circulation fan mode and status
    i2cResponseLen++;

    if (currentTime < lightsTimer) {transitionTime = lightsTimer - currentTime;} else {transitionTime = 0;}
    transitionTime = transitionTime / 60000;  // convert to minutes
    i2cResponse[i2cResponseLen] = (byte)(transitionTime >> 8); // high byte
    i2cResponseLen++;
    i2cResponse[i2cResponseLen] = (byte)transitionTime; // low byte
    i2cResponseLen++;
    
    i2cResponse[i2cResponseLen] = (byte)(lightsOnTime >> 8); // high byte
    i2cResponseLen++;
    i2cResponse[i2cResponseLen] = (byte)lightsOnTime; // low byte
    i2cResponseLen++;

    i2cResponse[i2cResponseLen] = (byte)(lightsOffTime >> 8); // high byte
    i2cResponseLen++;
    i2cResponse[i2cResponseLen] = (byte)lightsOffTime; // low byte 17
    i2cResponseLen++;
    
    if (currentTime < circulationFanTimer) {transitionTime = circulationFanTimer - currentTime;} else {transitionTime = 0;}
    transitionTime = transitionTime / 60000;  // convert to minutes
    i2cResponse[i2cResponseLen] = (byte)(transitionTime >> 8); // high byte
    i2cResponseLen++;
    i2cResponse[i2cResponseLen] = (byte)transitionTime; // low byte
    i2cResponseLen++;

    i2cResponse[i2cResponseLen] = (byte)(circulationFanOnTime >> 8); // high byte
    i2cResponseLen++;
    i2cResponse[i2cResponseLen] = (byte)circulationFanOnTime; // low byte
    i2cResponseLen++;
    
    i2cResponse[i2cResponseLen] = (byte)(circulationFanOffTime >> 8); // high byte
    i2cResponseLen++;
    i2cResponse[i2cResponseLen] = (byte)circulationFanOffTime; // low byte 22
    i2cResponseLen++;    // compensate for zero based array 24
  
} // i2cRequest
 
double GetChipTemp(void)
{  // Get the internal temperature of the arduino
  unsigned int wADC;
  double t;
  ADMUX = (_BV(REFS1) | _BV(REFS0) | _BV(MUX3));
  ADCSRA |= _BV(ADEN); // enable the ADC
  delay(20); // wait for voltages to become stable.
  ADCSRA |= _BV(ADSC); // Start the ADC
  while (bit_is_set(ADCSRA,ADSC));
  wADC = ADCW;
  t = (wADC - 324.31 ) / 1.22;
  return (t);
} // GetChipTemp

// end I2C support
// git clone http://github.com/jrowberg/i2cdevlib

// Reads DS18B20 "1-Wire" digital temperature sensors.
// Tutorial: http://www.hacktronics.com/Tutorials/arduino-1-wire-tutorial.html

void oneWireSetup()
{
  // Start up the library
  sensors.begin();
  sensors.setResolution(airThermometer, 10); // set the resolution to 10 bit (good enough?)
//  sensors.setResolution(outsideThermometer, 10);
//  sensors.setResolution(dogHouseThermometer, 10);
} // oneWireSetup

float getTemperature(DeviceAddress deviceAddress)
{
  float tempF = 70;
  float tempC = sensors.getTempC(deviceAddress);
  if (tempC == -127.00) {
 //   Serial.print("Error getting temperature");
  } else {
    tempF = DallasTemperature::toFahrenheit(tempC);
//    Serial.print("C: ");
//    Serial.print(tempC);
//    Serial.print(" F: ");
//    Serial.print(DallasTemperature::toFahrenheit(tempC));
//    Serial.print("\n");;
  }
  return tempF;
}

void getAirTemp()
{ 
  float tempF = 0;
  double leftPart;
  double rightPart;
  double newRight;
  sensors.requestTemperatures();
  tempF = getTemperature(airThermometer);
 
  rightPart = modf(tempF, &leftPart);
  airTempMSB = (byte)leftPart;
  newRight = rightPart*10;
  rightPart = modf(newRight, &leftPart);
  airTempLSB = (byte)leftPart;
}
// end DS18B20 support

