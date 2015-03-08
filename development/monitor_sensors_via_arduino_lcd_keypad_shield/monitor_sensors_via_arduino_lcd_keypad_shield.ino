/*
 *  Monitor Sensors Via Arduino LCD Keypad Shield.
 *
 *  Copyright (C) 2010 Efstathios Chatzikyriakidis (contact@efxa.org)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// include necessary headers. 
#include <LCD4Bit_mod.h>
#include <SimpleTimer.h>

// function prototypes.
void fmt_double (double value, byte prec, char * buf, unsigned blen = 0xffff);
unsigned fmt_unsigned (unsigned long value, char * buf, unsigned blen = 0xffff, byte width = 0);

// the number of LCD's lines.
const int LCD_MAX_LINES = 2;

// the object to manage the LCD.
LCD4Bit_mod lcd = LCD4Bit_mod(LCD_MAX_LINES);

// the LCD's keypad pin number (for recognizing pressed keys).
const int keypadPin = 0;

// the array of voltage threshold values (for identifying LCD's keys).
const int adcKeyVal[] = { 30, 150, 360, 535, 760 };

// the number of different keys in the LCD's.
const int NUM_KEYS = (int) (sizeof (adcKeyVal) / sizeof (const int));

// the current key and voltage threshold value of the LCD.
int adcKeyIn, key;

// the bounce time (ms) for LCD's keys.
const int BOUNCE_DURATION = 70;

// the timer interval (ms) for recognizing LCD's keys.
const long intervalLCDKeypad = 100;

// the timer interval (ms) for updating LCD's screen.
const long intervalLCDScreen = 1500;

// the default sensor handler to execute.
int sensorIndex = 0;

// type name definition for a sensor object.
typedef struct sensorT {
  int pin;                            // the pin number of the sensor.
  void (*handler) (const int index);  // the sensor handler function pointer.
} sensorT;

// array of sensors' objects.
const sensorT sensors[] = {
  {2, tempSensorHandler  },
  {3, lightSensorHandler },
  {3, pirSensorHandler   }
};

// calculate the number of sensors' objects in the array.
const int NUM_SENSORS = (int) (sizeof (sensors) / sizeof (const sensorT));

// create a simple timer object to handle timer actions.
SimpleTimer timer;

// produce a formatted string in a buffer corresponding to the value provided as string.
unsigned
fmt_unsigned (unsigned long value, char * buf, unsigned blen, byte width) {
  if (!buf || !blen) return 0;

  // produce the digit string (backwards in the digit buffer).
  char dbuf[10];
  unsigned idx = 0;

  while (idx < sizeof (dbuf)) {
    dbuf[idx++] = (value % 10) + '0';
    if ((value /= 10) == 0)
      break;
  }

  // copy the optional leading zeroes and digits to the target buffer.
  unsigned len = 0;
  byte padding = (width > idx) ? width - idx : 0;
  char c = '0';

  while ((--blen > 0) && (idx || padding)) {
    if (padding)
      padding--;
    else
      c = dbuf[--idx];

    *buf++ = c;
    len++;
  }

  // add the null termination
  *buf = '\0';

  return len;
}

// format a floating point value with number of decimal places as string.
void
fmt_double (double value, byte prec, char * buf, unsigned blen) {
  if (!buf || !blen) return;

  // limit the precision to the maximum allowed value.
  const byte maxprec = 6;
  if (prec > maxprec)
    prec = maxprec;

  if (--blen > 0) {
    if (value < 0.0) {
      value = -value;
      *buf++ = '-';
      blen--;
    }

    // compute the rounding factor and fractional multiplier.
    double roundingFactor = 0.5;
    unsigned long mult = 1;

    for (byte i = 0; i < prec; i++) {
      roundingFactor /= 10.0;
      mult *= 10;
    }

    if (blen > 0) {
      // apply the rounding factor.
      value += roundingFactor;

      // add the integral portion to the buffer.
      unsigned len = fmt_unsigned ((unsigned long) value, buf, blen);

      buf += len;
      blen -= len;
    }

    // handle the fractional portion.
    if ((prec > 0) && (blen > 0)) {
      *buf++ = '.';

      if (--blen > 0)
        buf += fmt_unsigned ((unsigned long) ((value - (unsigned long) value) * mult), buf, blen, prec);
    }
  }

  // null-terminate the string.
  *buf = '\0';
}

// temperature sensor handler function.
void
tempSensorHandler(const int index) {
  // the value of temperature as string.
  char value[32];
  
  // get sensor value and convert it to Celsius temperature.
  double sensorValue = getCelsius(getVoltage(sensors[index].pin));

  // convert the value to string.
  fmt_double (sensorValue, 2, value, 32);

  // print to the LCD the appropriate data.
  lcd.printIn("[Temperature]");
  lcd.cursorTo(2, 0);
  lcd.printIn("Value: ");
  lcd.printIn(value);
  lcd.printIn(" C");
}

// light sensor handler function.
void
lightSensorHandler(const int index) {
  // the value of light as string.
  char value[32]; int lightValue;

  // get a value from the sensor.
  lightValue = analogRead(sensors[index].pin);

  // convert the value to string.
  sprintf(value, "%d", lightValue);

  // print to the LCD the appropriate data.
  lcd.printIn("[Light]");
  lcd.cursorTo(2, 0);
  lcd.printIn("Value: ");
  lcd.printIn(value);
}

// PIR sensor handler function.
void
pirSensorHandler(const int index) {
  // the value of motion as string.
  char value[32];

  // store the appropriate value according to sensor's state.
  sprintf(value, digitalRead(sensors[index].pin) == HIGH ? "ON" : "OFF");

  // print to the LCD the appropriate data.
  lcd.printIn("[Motion]");
  lcd.cursorTo(2, 0);
  lcd.printIn("Value: ");
  lcd.printIn(value);
}

// timer interrupt service routine function for updating LCD's screen.
void
updateLCDScreen() {
  // clear the LCD screen.
  lcd.clear();

  // call the appropriate sensor handler.
  sensors[sensorIndex].handler(sensorIndex);
}

// timer interrupt service routine function for checking LCD's keypad.
void
checkLCDKeypad() {
  // get a possible voltage threshold value from the LCD.
  adcKeyIn = analogRead(keypadPin);

  // decide which key from keypad was pressed.
  key = getKey(adcKeyIn);

  // if keypress is detected.
  if (key != -1) {
    // wait for debounce time.
    delay(BOUNCE_DURATION);

    adcKeyIn = analogRead(keypadPin); // get a possible voltage threshold value from the LCD.
    key = getKey(adcKeyIn);           // decide which key from keypad was pressed.

    // if there was a key pressed.
    if (key >= 0) {
      // check which key was pressed.
      switch (key) {
        // up key.
        case 1:
          sensorIndex++; // for calling the next sensor handler.
          break;
        // down key.
        case 2:
          sensorIndex--; // for calling the previous sensor handler.
          break;
      }

      // check bounds for the sensors' handlers (repeat).
      if (sensorIndex < 0) sensorIndex = NUM_SENSORS - 1;
      if (sensorIndex >= NUM_SENSORS) sensorIndex = 0;
    }
  }
}

// decide the key pressed from LCD's keypad.
int
getKey(unsigned int input) {
  // for indexing the voltage threshold values.
  int k;

  // loop through all the LCD's keypad keys.
  for (k = 0; k < NUM_KEYS; k++) {
    // check if it is the current key by the voltage.
    if (input < adcKeyVal[k])
      // return the index of the key.
      return k;
  }

  if (k >= NUM_KEYS)
    k = -1; // no valid key pressed.

  // return that no key was pressed.
  return k;
}

// get the voltage on the analog input pin.
double
getVoltage(const int pin) {
  // converting from a 0 to 1024 digital range to
  // 0 to 5 volts (each 1 reading equals ~ 5 mv).
  return (analogRead(pin) * .004882814);
}

// convert the voltage to Celsius temperature.
double
getCelsius(const double voltage) {
  return (voltage - .5) * 100;
}

// startup point entry (runs once).
void
setup() { 
  // set the mode of sensors' pins as input.
  for (int i = 0; i < NUM_SENSORS; i++)
    pinMode(sensors[i].pin, INPUT);

  // initialize the LCD.
  lcd.init();

  // set both LCD's screen & keypad timer actions.
  timer.setInterval(intervalLCDScreen, updateLCDScreen);
  timer.setInterval(intervalLCDKeypad, checkLCDKeypad);
}

// loop the main sketch.
void
loop() {
  // this is where the "polling" occurs about the timer.
  timer.run();
}
