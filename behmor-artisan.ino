/****************************************************************************
 * Behmor 1600 plus + Artisan + RoasterLogger
 * 
 * Version = 1.0;
 * 
 * Author = Maycon Mesquita
 *
 * Sends data and receives instructions as key=value pairs.                 
 * Sends temperatures T1 and T2 at sendTimePeriod intervals as "T1=123.6" 
 * Accepts instructions for heater level, set temperature etc.
 * Example: Send to Arduino "heater=80"  or "fan=20"
 ****************************************************************************/

#include <Wire.h>
#include <SPI.h>

#include <SimpleTimer.h>
SimpleTimer timer1; // Heater (OFF/ON cycle)
SimpleTimer timer2; // After Burner (ON 238ms/4ms blink)
SimpleTimer timer3; // Draw Fan  (ON 110ms/180ms blink)

#include <Adafruit_GFX.h> // Adafruit Display Lib
#include <Adafruit_SSD1306.h> // Adafruit Display Lib

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

#include "DHT.h"
#define DHTPIN A7 // data pin
#define DHTTYPE DHT22 // DHT 22 model

DHT dht(DHTPIN, DHTTYPE);

#define FAHRENHEIT // To output data in Celsius comment out this one line 
#define DP 1  // No. decimal places for serial output
#define maxLength 30   // maximum length for strings used

bool isArtisan = true;

// set pin numbers:
const int heaterPin =  7; // digital pin for pulse width modulation of heater
const int afterBurnerPin =  8;
const int drawFanPin =  9;
const int scrollFanPin =  10;
const int motorPin =  11;
const int motorCtrlPin =  30;
const int coolFanPin =  12;
const int lightPin =  31;

// thermocouple reading Max 6675 pins
const int SO  = 2;     // SO pin on MAX6675
const int SCKa = 3;    // SCKa pin on MAX6675
const int CS1 = 6;     // CS (chip 1 select) pin on MAX6675
const int CS2 = 5;     // CS (chip 2 select)  pin on MAX6675

float dhtHumidity = 0.0; // Init value
float dhtTemp = 0.0; // Init value

// time constants
const int timePeriod = 2000;   // total time period of PWM milliseconds see note on setupPWM before changing

// thermocouple settings
float calibrate1 = 0.0; // Temperature compensation for T1
float calibrate2 = 0.0; // Temperature compensation for T2

//temporary values for temperature to be read
float temp1 = 0.0;                   // temporary temperature variable
float temp2 = 0.0;                   // temporary temperature variable 
float t1 = 0.0;                      // Last average temperature on thermocouple 1 - average of four readings
float t2 = 0.0;                      // Last average temperature on thermocouple 2 - average of four readings
float tCumulative1 = 0.0;            // cumulative total of temperatures read before averaged
float tCumulative2 = 0.0;            // cumulative total of temperatures read before averaged
int noGoodReadings1 = 0;             // counter of no. of good readings for average calculation 
int noGoodReadings2 = 0;             // counter of no. of good readings for average calculation

int inByte = 0;                       // incoming serial byte
String inString = String(maxLength);  // input String

// loop control variables
unsigned long lastTCTimerLoop;        // for timing the thermocouple loop
int tcLoopCount = 0;                  // counter to run serial output once every 4 loops of 250 ms t/c loop

// PWM variables
int timeForHeaterOn;                  // millis PWM is on out of total of timePeriod (timeForHeaterOn = timePeriod for 100% heater)
unsigned long lastTimePeriod;         // millis since last turned on pwm
int heater = 0;                       // use as %, 100 is always on, 0 always off. default 0
int fan = 0;                          // use as %, 100 is always on, 0 always off. default 0
int motor = 0;                        // use as %, 100 is always on, 0 always off. default 0
int drawfan = 0;                      // use as %, 100 is always on, 0 always off. default 0
int afterburner = 0;                  // use as %, 100 is always on, 0 always off. default 0

int lightState = LOW;                 // behmor Light State. default LOW

void setup() {
  Serial.begin(115200, SERIAL_8N1); // Set this in Artisan

  // use establish contact if you want to wait until 'A' sent to Arduino before start - not used in this version
  // establishContact();  // send a byte to establish contact until receiver responds 
  setupPWM();

  pinMode(heaterPin, OUTPUT);
  pinMode(afterBurnerPin, OUTPUT);
  pinMode(drawFanPin, OUTPUT);
  pinMode(scrollFanPin, OUTPUT);
  pinMode(motorPin, OUTPUT);
  pinMode(motorCtrlPin, OUTPUT);
  pinMode(coolFanPin, OUTPUT);
  pinMode(lightPin, OUTPUT);
  
  // set up pin modes for Max6675's
  pinMode(CS1, OUTPUT);
  pinMode(CS2, OUTPUT);
  pinMode(SO, INPUT);
  pinMode(SCKa, OUTPUT);

  // deselect both Max6675's
  digitalWrite(CS1, HIGH);
  digitalWrite(CS2, HIGH);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x64) - init done

  display.clearDisplay(); // Clear the buffer.
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);

  dht.begin();

  // delay(100); // delay for prevent Artisan init erors
}

/****************************************************************************
 * Set up pwm control for heater. Hottop uses a triac that switches
 * only on zero crossing so normal pwm will not work.
 * Minimum time slice is a half cycle or 10 millisecs in UK.  
 * Loop in this prog may need up to 10 ms to complete.
 * I will use 20 millisecs as time slice with 100 power levels that
 * gives 2000 milliseconds total time period.
 * The Hottop heater is very slow responding so 2 sec period is not a problem.
 ****************************************************************************/
void setupPWM() {
  lastTimePeriod = millis(); // setup delay
}

// this not currently used
void establishContact() {
  //  while (Serial.available() <= 0) {
  //    Serial.print('A', BYTE);   // send a capital A
  //    delay(300);
  //  }

  Serial.println("Opened but no contact yet - send A to start");
  int contact = 0;
  while (contact == 0) {
    if (Serial.available() > 0) {
      inByte = Serial.read();
      Serial.println(inByte);
      if (inByte == 'A') { 
        contact = 1;
        Serial.println("Contact established starting to send data");
      }    
    }
  }  
}

// #1 PWM Way (for SSR use)
void doPWM_Way() {
  // Heater (temp disabling)
  int heater100to255 = map(heater, 0, 100, 0, 255); // 0-100 to 0-255 (duty cycle)
  analogWrite(heaterPin, heater100to255);

  // Fan
  int fan100to255 = map(fan, 0, 100, 0, 255); // 0-100 to 0-255 (duty cycle)
  analogWrite(scrollFanPin, fan100to255);
  
}

// fan
unsigned long previousMillisFan = 0;
int fanState = HIGH;

// heater (cycle)
int heater_timeOn = 0;
int heater_timeOff = 0;
int heaterLastValue = 0;
int heaterOnTimer;
int heaterOffTimer;

// after burner (blink)
int afterBurner_timeOn = 240;
int afterBurner_timeOff = 10;
int afterBurnerLastValue = 0;
int afterBurnerOnTimer;
int afterBurnerOffTimer;

// draw fan (blink)
int drawFan_timeOn = 100;
int drawFan_timeOff = 180;
int drawFanLastValue = 0;
int drawFanOnTimer;
int drawFanOffTimer;

// #2 Toggle Relays Way
void doToggle_Way() {  
  /*********** Heater ***********/
  if (heater == 0) digitalWrite(heaterPin, LOW);
  else if (heater == 100) digitalWrite(heaterPin, HIGH);
  else if (heater > 0 && heater < 100) {
    if (heater == 25) { heater_timeOn = 5000;  heater_timeOff = 10000; } // Behmor default: timeOn = 5000;  timeOff = 12000;
    if (heater == 50) { heater_timeOn = 10000; heater_timeOff = 7000;  } // Behmor default: timeOn = 10000; timeOff = 8000;
    if (heater == 75) { heater_timeOn = 12000; heater_timeOff = 4000;  } // Behmor default: timeOn = 12000; timeOff = 4000;

    /* custom cycles */
    if (heater == 65) { heater_timeOn = 10000; heater_timeOff = 4500;  }
    if (heater == 85) { heater_timeOn = 14000; heater_timeOff = 3000;  }
    if (heater == 95) { heater_timeOn = 15000; heater_timeOff = 2000;  }

    if (heater == 25 || heater == 50 || heater == 75 || heater == 65 || heater == 85 || heater == 95) {
      if (heater != heaterLastValue ) {
        timer1.disable(heaterOnTimer);
        timer1.disable(heaterOffTimer);
      }

      heaterLastValue = heater;

      if (!timer1.isEnabled(heaterOnTimer) && !timer1.isEnabled(heaterOffTimer)) {
        digitalWrite(heaterPin, HIGH); // Heater ON
        heaterOnTimer = timer1.setTimeout(heater_timeOn, heaterOffWaiting); // Wait some seconds
      }
    }
  }

  timer1.run(); // Timer ON!

  /*********** Motor Speed ***********/
  if (motor < 1) motorOff();
  if (motor >= 50) motorOn();
  if (motor == 50) motorSlow();
  if (motor == 100) motorFast();

  /*********** Draw Fan ***********/
  if (drawfan == 0) drawFanOff();
  else if (drawfan == 100) drawFanOn(false);
  else if (drawfan > 0 && drawfan < 100) { drawFanOn(true); }

  /*********** Fan ***********/
  if (fan == 0) coolFanOff();
  else if (fan == 100) coolFanOn(false);
  else if (fan > 0 && fan < 100) {
    coolFanOn(true);
  }

  /*********** After Burner ***********/
  if (afterburner < 1) afterBurnerOff();
  else if (afterburner > 0) afterBurnerOn();
}

/* Heater */
void heaterOffWaiting() {
  digitalWrite(heaterPin, LOW);
  heaterOffTimer = timer1.setTimeout(heater_timeOff, heaterOff); // Wait some seconds
}

void heaterOff() {
  digitalWrite(heaterPin, LOW);
}
/* Fim Heater *

/* After Burner -> behmor default frequency: 238ms (on) & 4ms (off) */
void afterBurnerOn() {
  if (afterburner != afterBurnerLastValue) {
    timer2.disable(afterBurnerOnTimer);
    timer2.disable(afterBurnerOffTimer);
  }

  afterBurnerLastValue = afterburner;

  if (!timer2.isEnabled(afterBurnerOnTimer) && !timer2.isEnabled(afterBurnerOffTimer)) {
    digitalWrite(afterBurnerPin, HIGH);
    afterBurnerOnTimer = timer2.setTimeout(afterBurner_timeOn, afterBurnerOffWaiting); // Wait some seconds
  }

  timer2.run(); // Timer ON!
}

void afterBurnerOffWaiting() {
  digitalWrite(afterBurnerPin, LOW);
  afterBurnerOffTimer = timer2.setTimeout(afterBurner_timeOff, afterBurnerOff); // Wait some seconds
}

void afterBurnerOff() { digitalWrite(afterBurnerPin, LOW); }

// Draw Fan Blink -> behmor default frequency: 110ms (on) & 180ms (off)
void drawFanOn(bool pulse) {
  if (!pulse) {
    digitalWrite(drawFanPin, HIGH);
  } else {
    if (drawfan != drawFanLastValue) {
      timer3.disable(drawFanOnTimer);
      timer3.disable(drawFanOffTimer);
    }

    drawFanLastValue = drawfan;

    if (!timer3.isEnabled(drawFanOnTimer) && !timer3.isEnabled(drawFanOffTimer)) {
      digitalWrite(drawFanPin, HIGH); // Draw Fan ON
      drawFanOnTimer = timer3.setTimeout(drawFan_timeOn, drawFanOffWaiting); // Wait some seconds
    }
  }

  timer3.run(); // Timer ON!
}

void drawFanOffWaiting() {
  digitalWrite(drawFanPin, LOW); // Draw Fan Off
  drawFanOffTimer = timer3.setTimeout(drawFan_timeOff, drawFanOff); // Wait some seconds
}

void drawFanOff() { digitalWrite(drawFanPin, LOW);}

void scrollFanOn() {  digitalWrite(scrollFanPin, HIGH); }
void scrollFanOff() { digitalWrite(scrollFanPin, LOW); }

void motorOn() { digitalWrite(motorPin, HIGH); }
void motorOff() { digitalWrite(motorPin, LOW); }

void motorSlow() { digitalWrite(motorCtrlPin, HIGH); } // Slow Speed
void motorFast() { digitalWrite(motorCtrlPin, LOW); } // Fast Speed

void coolFanOn(bool pwm) {
  if (!pwm) {
    digitalWrite(coolFanPin, HIGH); 
  } else {
    int v100to255 = map(fan, 0, 100, 0, 255); // 0-100 to 0-255 (duty cycle)
    analogWrite(coolFanPin, v100to255);
  }
}

void coolFanOff() {
  digitalWrite(coolFanPin, LOW);
}

void lightToggle() {
  if (lightState == LOW) lightState = HIGH;
  else lightState = LOW;

  digitalWrite(lightPin, lightState);
}

void lightOn() {
  digitalWrite(lightPin, HIGH);
}
void lightOff() {
  digitalWrite(lightPin, LOW);
}

void powerOn() {
  setHeaterLevel(100);
  setCoolFanLevel(100);
  setMotorSpeed(50);
}

void powerOff() {
  setHeaterLevel(0);
  setCoolFanLevel(0);
  setAfterBurnerLevel(0);
  
  scrollFanOff();

  setDrawFanLevel(0);
  setMotorSpeed(0);
  lightOff();
}

void cool() {
  setHeaterLevel(0);

  setAfterBurnerLevel(100); // Blink @ 50ms
  scrollFanOn(); // Blink for 20s @ 300ms

  setCoolFanLevel(100);
  setDrawFanLevel(100); // Blink @ 280ms
  setMotorSpeed(100); // Power on / High Speed motor
}

/****************************************************************************
 * Toggles the heater on/off based on the current heater level.
 ****************************************************************************/
void controlRoaster() {
  // doPWM_Way(); // this mode will need SSR control
  doToggle_Way(); // this mode will use native heater system
}

void setHeaterLevel(int h) {
  if (h > -1 && h < 101) heater = h;
}

void setMotorSpeed(int m) {
  if (m > -1 && m < 101) motor = m;
}

void setDrawFanLevel(int d) {
  if (d > -1 && d < 101) drawfan = d;
}

void setCoolFanLevel(int f) {
  if (f > -1 && f < 101) fan = f;
}

void setAfterBurnerLevel(int a) {
  if (a > -1 && a < 101) afterburner = a;
}

/****************************************************************************
 * Called when an input string is received from computer
 * designed for key=value pairs or simple text commands. 
 * Performs commands and splits key and value 
 * and if key is defined sets value otherwise ignores
 ****************************************************************************/
void doInputCommand() {
  float v = -1;
  inString.toLowerCase();
  int indx = inString.indexOf('=');

  if (indx < 0) {  // this is a message not a key value pair (Artisan)
    if (inString.equals("read")) {  // send data to artisan
      //         null      1-ET               2-BT            null        4-DHT22     null
      String x = "0," + String(t2) + "," + String(t1) + "," + "0" + String(dhtTemp) + ",0";
      Serial.println(x);
    }

    else if(inString.equals("chan;1200")) {
      isArtisan = true;
      Serial.println("# CHAN;1200");      
    }

    else if(inString.equals("power-on"))    powerOn();
    else if(inString.equals("power-off"))   powerOff();
    else if(inString.equals("cool"))        cool();

    else if(inString.equals("after-burner-on"))   setAfterBurnerLevel(100);
    else if(inString.equals("after-burner-off"))  afterBurnerOff();

    else if(inString.equals("scroll-fan-on"))   scrollFanOn();
    else if(inString.equals("scroll-fan-off"))  scrollFanOff();

    else if(inString.equals("light-toggle"))  lightToggle();

    // else if(inString.equals("light-on"))   lightOn();
    // else if(inString.equals("light-off"))  lightOff();

    Serial.flush();
  } else {  // this is a key value pair for decoding
    String key = inString.substring(0, indx);
    String value = inString.substring(indx+1, inString.length());

    // parse string value and return float v
    char buf[value.length()+1];
    value.toCharArray(buf,value.length()+1);
    v = atof (buf); 

    // only set value if we have a valid positive number - atof will return 0.0 if invalid
    if (v >= 0) {
      if (key.equals("heater") && v < 101){  
        setHeaterLevel((long) v); // convert v to integer for heater control
      }

      else if (key.equals("motor") && v < 101) {
        setMotorSpeed((long) v); // convert v to integer for motor control 
      }

      else if (key.equals("drawfan") && v < 101) {
        setDrawFanLevel((long) v); // convert v to integer for motor control 
      }

      else if (key.equals("fan") && v < 101) {
        setCoolFanLevel((long) v); // convert v to integer for fan control 
      }

      else if (key.equals("afterburner") && v < 101) {
        setAfterBurnerLevel((long) v); // convert v to integer for fan control 
      }
    }
  }
}

/****************************************************************************
 * check if serial input is waiting if so add it to inString.  
 * Instructions are terminated with \n \r or 'z' 
 * If this is the end of input line then call doInputCommand to act on it.
 ****************************************************************************/
void getSerialInput() {
  //check if data is coming in if so deal with it
  if (Serial.available() > 0) {

    // read the incoming data as a char:
    char inChar = Serial.read();

    // if it's a newline or return or z, print the string:
    if ((inChar == '\n') || (inChar == '\r') || (inChar == 'z')) {

      // do whatever is commanded by the input string
      if (inString.length() > 0) doInputCommand();

      inString = "";        //reset for next line of input
    } else {
      // if we are not at the end of the string, append the incoming character
      if (inString.length() < maxLength) {
        inString += inChar; 
      } else {
        inString = "";
        inString += inChar;
      }
    }
  }
}

/****************************************************************************
 * Send data to computer once every second.  Data such as temperatures, etc.
 * This allows current settings to be checked by the controlling program
 * and changed if, and only if, necessary.
 * This is quicker that resending data from the controller each second
 * and the Arduino having to read and interpret the results.
 ****************************************************************************/
void doSerialOutput() {
  if (isArtisan) {
    return;
  }

  Serial.print("t1=");
  Serial.println(t1,DP);

  Serial.print("t2=");
  Serial.println(t2,DP);

  Serial.print("heater=");
  Serial.println(heater);

  Serial.print("afterburner=");
  Serial.println(afterburner);

  Serial.print("motor=");
  Serial.println(motor);

  Serial.print("drawfan=");
  Serial.println(drawfan);

  Serial.print("fan=");
  Serial.println(fan);

  Serial.print("ambient=");
  Serial.println(dhtTemp);
}

/****************************************************************************
 * Read temperatures from Max6675 chips Sets t1 and t2, -1 if an error
 * occurred.  Max6675 needs 240 ms between readings or will return last
 * value again. I am reading it once per second.
 ****************************************************************************/
void getTemperatures() {
 temp1 = readThermocouple(CS1, calibrate1);
 temp2 = readThermocouple(CS2, calibrate2);  
  
 if (temp1 > 0.0) {
    tCumulative1 = tCumulative1 + temp1;
    noGoodReadings1 ++;
 }

 if (temp2 > 0.0) {
    tCumulative2 = tCumulative2 + temp2;
    noGoodReadings2 ++;
 }
}

void updateDHT() {
  if (isnan(dht.readHumidity()) || isnan(dht.readTemperature())) {
    dhtHumidity = 0.0;
    dhtTemp = 0.0;
  } else {
    dhtHumidity = dht.readHumidity();

    #ifdef FAHRENHEIT
      dhtTemp = dht.readTemperature(true); // Fahrenheit
    #else
      dhtTemp = dht.readTemperature(); // Celsius
    #endif
  }
}

/****************************************************************************
 * Update the values on the OLED Display
 ****************************************************************************/
void outputOLED() {
  display.setCursor(0,0); // x,y
  
  display.print("BT = ");
  display.print(t1, 1);
  display.print("F");
  display.println();

  display.print("ET = ");
  display.print(t2, 1);
  display.print("F");
  display.println();

  display.print("Ambient = ");
  display.print(dhtTemp, 1);
  display.print("F");
  display.println();

  //display.print("Heater = ");
  //display.print(heater);
  //display.println();

  //display.print("Fan = ");
  //display.print(fan);
  //display.println();

  //display.print("Motor = ");
  //display.print(motor);
  //display.println();

  display.print("Humidity = ");
  display.print(dhtHumidity, 1);
  display.print("%");
  display.println();
 
  display.display();
  display.clearDisplay();
}

/****************************************************************************
 * Called by main loop once every 250 ms
 * Used to read each thermocouple once every 250 ms
 *
 * Once per second averages temperature results, updates potentiometer and outputs data
 * to serial port.
 ****************************************************************************/
void do250msLoop() {
  getTemperatures();

  if (tcLoopCount > 3) { // once every four loops (1 second) calculate average temp, update Pot and do serial output
    tcLoopCount = 0;

    if (noGoodReadings1 > 0)  t1 = tCumulative1 / noGoodReadings1; else t1 = -1.0;
    if (noGoodReadings2 > 0)  t2 = tCumulative2 / noGoodReadings2; else t2 = -1.0;
    noGoodReadings1 = 0;
    noGoodReadings2 = 0;
    tCumulative1 = 0.0;
    tCumulative2 = 0.0;

    #ifdef FAHRENHEIT
      if (t1 > 0) t1 = (t1 * 9 / 5) + 32;
      if (t2 > 0) t2 = (t2 * 9 / 5) + 32;
    #endif

    updateDHT(); //<- delaying the loop, function disabled
    outputOLED();
    doSerialOutput(); // once per second
  }

  tcLoopCount++;
}

/****************************************************************************
 * Main loop must not use delay!  PWM heater control relies on loop running
 * at least every 40 ms.  If it takes longer then heater will be on slightly
 * longer than planned. Not a big problem if 1% becomes 1.2%! But keep loop fast.
 * Currently loop takes about 4-5 ms to run so no problem.
 ****************************************************************************/
void loop() {
  getSerialInput();// check if any serial data waiting

  // loop to run once every 250 ms to read TC's
  if (millis() - lastTCTimerLoop >= 250) {
    lastTCTimerLoop = millis();
    do250msLoop();
  }

  controlRoaster(); // Toggle heater on/off based on heater setting
}


/*****************************************************************
 * Read the Max6675 device 1 or 2.  Returns temp as float or  -1.0
 * if an error reading device.
 * Note at least 240 ms should elapse between readings of a device
 * to allow it to settle to new reading.  If not the last reading 
 * will be returned again.
 *****************************************************************/
float readThermocouple(int CS, float calibrate) { //device selected by passing in the relavant CS (chip select)
  int value = 0;
  int error_tc = 0;
  float temp = 0.0;

  digitalWrite(CS, LOW); // Enable device

  // wait for it to settle
  delayMicroseconds(1);

  // Cycle the clock for dummy bit 15
  digitalWrite(SCKa, HIGH);
  digitalWrite(SCKa, LOW);

  // wait for it to settle
  delayMicroseconds(1);

  /* Read bits 14-3 from MAX6675 for the Temp 
   Loop for each bit reading the value and 
   storing the final value in 'temp' 
  */
  for (int i=11; i>=0; i--) {
    digitalWrite(SCKa, HIGH);  // Set Clock to HIGH
    value += digitalRead(SO) << i;  // Read data and add it to our variable
    digitalWrite(SCKa, LOW);  // Set Clock to LOW
  }

  // Read the TC input to check for error
  digitalWrite(SCKa, HIGH); // Set Clock to HIGH
  error_tc = digitalRead(SO); // Read data
  
  digitalWrite(SCKa, LOW);  // Set Clock to LOW
  digitalWrite(CS, HIGH); // Disable device 1

  value = value + calibrate;      // Add the calibration value
  temp = (value*0.25);            // Multiply the value by 0.25 to get temp in ˚C

  // return -1 if an error occurred, otherwise return temp
  if (error_tc == 0) return temp; 
  else return -1.0; 
}
