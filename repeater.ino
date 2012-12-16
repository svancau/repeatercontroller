/*
	Ham radio repeater controller
    Copyright (C) 2012 Sebastien Van Cauwenberghe ON4SEB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////
// Static Repeater Configuration
////////////////////////////////////////////
// PINS
#define PIN_1750 4
#define PIN_CTCSS 5
#define PIN_CARRIER 6
#define PIN_PTT 13
#define PIN_MORSEOUT 14

// DELAYS (in ms)
#define INACTIVE_CLOSE 10000
#define RELEASE_BEEP 200
#define BEEP_LENGTH 100
#define BEACON_DELAY (60000UL)

#define MORSE_DOT 60
#define MORSE_DASH (3*MORSE_DOT)
// Spaces equal Number of dots - 1
#define MORSE_SYM_SPC MORSE_DOT
#define MORSE_LETTER_SPC (2*MORSE_DOT)
#define MORSE_WORD_SPC (5*MORSE_DOT)

// BEHAVIOUR (set to 1 to enable, 0 to disable)
#define USE_1750_OPEN 1
#define USE_CTCSS_OPEN 1
#define USE_CARRIER_OPEN 1

#define USE_CTCSS_BUSY 1
#define USE_CARRIER_BUSY 1

#define USE_BEEP 1

// BEHAVIOUR of the roger beep define only _ONE_ of those
//#define ROGER_TONE
#define ROGER_K

// BEEP Frequencies
#define BEEP_FREQ 800
#define MORSE_FREQ 800

const char openMsg[] = "ON4SEB";
const char closeMsg[] = "ON4SEB SK";
const char kMsg[] = "K";
const char beaconMsg[] = "ON4SEB";

// System Defines (DO NOT TOUCH)
#define REPEATER_CLOSED 0
#define REPEATER_OPEN 1
#define REPEATER_MORSE 2

// Type definitions
typedef unsigned char uchar;
typedef unsigned long ulong;

// Macros
#define UPDATE_TIMER(tname,tval) tname = millis() + tval
#define TIMER_ELAPSED(tname) ((long)(millis() - tname) >= 0)

// Constants
// Thanks to KB8OJH
#define MORSE_NONE 1
const unsigned char morse_ascii[] = {
  MORSE_NONE, MORSE_NONE, MORSE_NONE, MORSE_NONE,
  MORSE_NONE, MORSE_NONE, MORSE_NONE, MORSE_NONE,
  MORSE_NONE, MORSE_NONE, MORSE_NONE, MORSE_NONE,
  MORSE_NONE, MORSE_NONE, MORSE_NONE, MORSE_NONE,
  MORSE_NONE, MORSE_NONE, MORSE_NONE, MORSE_NONE,
  MORSE_NONE, MORSE_NONE, MORSE_NONE, MORSE_NONE,
  MORSE_NONE, MORSE_NONE, MORSE_NONE, MORSE_NONE,
  MORSE_NONE, MORSE_NONE, MORSE_NONE, MORSE_NONE,
  MORSE_NONE, MORSE_NONE, MORSE_NONE, MORSE_NONE,
  MORSE_NONE, MORSE_NONE, MORSE_NONE, MORSE_NONE,
  MORSE_NONE, MORSE_NONE, MORSE_NONE, MORSE_NONE,
  0x73, MORSE_NONE, 0x55, 0x32,                   /* , _ . / */
  0x3F, 0x2F, 0x27, 0x23,                         /* 0 1 2 3 */
  0x21, 0x20, 0x30, 0x38,                         /* 4 5 6 7 */
  0x3C, 0x37, MORSE_NONE, MORSE_NONE,             /* 8 9 _ _ */
  MORSE_NONE, 0x31, MORSE_NONE, 0x4C,             /* _ = _ ? */
  MORSE_NONE, 0x05, 0x18, 0x1A,                   /* _ A B C */
  0x0C, 0x02, 0x12, 0x0E,                         /* D E F G */
  0x10, 0x04, 0x17, 0x0D,                         /* H I J K */
  0x14, 0x07, 0x06, 0x0F,                         /* L M N O */
  0x16, 0x1D, 0x0A, 0x08,                         /* P Q R S */
  0x03, 0x09, 0x11, 0x0B,                         /* T U V W */
  0x19, 0x1B, 0x1C, MORSE_NONE,                   /* X Y Z _ */
  MORSE_NONE, MORSE_NONE, MORSE_NONE, MORSE_NONE,
  MORSE_NONE, 0x05, 0x18, 0x1A,                   /* _ A B C */
  0x0C, 0x02, 0x12, 0x0E,                         /* D E F G */
  0x10, 0x04, 0x17, 0x0D,                         /* H I J K */
  0x14, 0x07, 0x06, 0x0F,                         /* L M N O */
  0x16, 0x1D, 0x0A, 0x08,                         /* P Q R S */
  0x03, 0x09, 0x11, 0x0B,                         /* T U V W */
  0x19, 0x1B, 0x1C, MORSE_NONE,                   /* X Y Z _ */
  MORSE_NONE, MORSE_NONE, MORSE_NONE, MORSE_NONE,
};

/////////////////////////////////////////////
// Setup phase
/////////////////////////////////////////////
void setup()
{
  ioSetup();  
  Serial.begin(115200);
}

void ioSetup()
{
  // Configure Pins properly
  pinMode (PIN_1750, INPUT);
  pinMode (PIN_CTCSS, INPUT);
  pinMode (PIN_CARRIER, INPUT);
  pinMode (PIN_PTT, OUTPUT);
  pinMode (PIN_MORSEOUT, OUTPUT);

  // Set it low by default
  digitalWrite(PIN_PTT, LOW);
  digitalWrite(PIN_MORSEOUT, LOW);
}

/////////////////////////////////////////////
// Main loop
/////////////////////////////////////////////
uchar State = REPEATER_CLOSED;

// Timers
ulong closeTimer;
ulong rogerBeepTimer;
ulong beepToneTimer;
ulong morseTimer;
ulong beaconTimer;

bool beepEnabled; // A Roger beep can be sent
bool sqlOpen; // Current Squelch status
bool beepOn = false; // Beep is currently enabled
bool morseActive = false; // currently sending morse

char* morseStr; // Pointer to the morse string
int strCounter; // Morse string index
int strCounterMax; // End of message index

void loop()
{
  updateIO(); // Control PTT
  setRepeaterState();
  morseGenerator(); // Morse generator task
  beaconTask();
}

void setRepeaterState()
{
  switch (State) 
  {
  // ----------- REPEATER IS IDLE ----------------
  case REPEATER_CLOSED:
    // Open Repeater
    if ((USE_1750_OPEN && digitalRead(PIN_1750))
      || (USE_CTCSS_OPEN && digitalRead(PIN_CTCSS))
      || (USE_CARRIER_OPEN && digitalRead(PIN_CARRIER)))
    {
      UPDATE_TIMER(closeTimer,INACTIVE_CLOSE);
      State = REPEATER_OPEN;
      Serial.print ("Opening\n");
      sendMorse(openMsg, (int)sizeof(openMsg));
    }

    break;

  // ----------- REPEATER IS OPENED ----------------
  case REPEATER_OPEN:
    // Keep repeater open as long there is some input
    if ((USE_CTCSS_BUSY && digitalRead(PIN_CTCSS))
      || (USE_CARRIER_BUSY && digitalRead(PIN_CARRIER)))
    {
      UPDATE_TIMER(closeTimer,INACTIVE_CLOSE); // Repeater closing timer
      sqlOpen = true;
    }
    else
    {
      if (sqlOpen) // If the Squelch was previously opened
      {
        UPDATE_TIMER(rogerBeepTimer,RELEASE_BEEP); // Roger beep start timer
        beepEnabled = true;
      }
      sqlOpen = false;
    }

    // Set timeout after a known time    
    if (TIMER_ELAPSED(closeTimer))
    {
      State = REPEATER_CLOSED;
      Serial.print ("Closing\n");
      sendMorse (closeMsg, (int) sizeof(closeMsg));
      State = REPEATER_MORSE;
      UPDATE_TIMER(beaconTimer,BEACON_DELAY); // Force Identification BEACON_DELAY time after closing
    }

    // Roger Beep
    if (beepEnabled && TIMER_ELAPSED(rogerBeepTimer)) // PTT Release time to roger beep
    {
      Serial.print ("Beep\n");
      beepEnabled = false;
      rogerBeep();
    }

    break;
    // --------------- REPEATER SENDING MORSE -------
    case REPEATER_MORSE: // Wait with PTT on till there is no morse to send
    if (!morseActive)
      {
        State = REPEATER_CLOSED;
      }
  }
}

// Generation of the Roger beep
void rogerBeep()
{
#ifdef ROGER_TONE
  if (!morseActive)
  startBeep (PIN_MORSEOUT, BEEP_FREQ, BEEP_LENGTH);
#endif

#ifdef ROGER_K
  sendMorse(kMsg, (int)sizeof(kMsg));
#endif
}

// Beacon task
void beaconTask()
{
  if (TIMER_ELAPSED(beaconTimer) && (State == REPEATER_CLOSED))
  {
    State = REPEATER_MORSE;
    sendMorse (beaconMsg, (int)sizeof(beaconMsg));
    UPDATE_TIMER(beaconTimer, BEACON_DELAY);
    Serial.print ("Beacon\n");
  }
}

// Control IO Pins
void updateIO()
{
  // Set the PTT control depending on repeater state
  if (State != REPEATER_CLOSED)
    digitalWrite(PIN_PTT, HIGH);
  else
    digitalWrite(PIN_PTT, LOW);

  updateBeep(PIN_MORSEOUT); // Stop the beep when timer elapsed
}

// Do a beep for a known time
void startBeep(unsigned int pin, unsigned int freq, unsigned long duration)
{
  if (!beepOn)
  {
    tone (pin, freq);
    UPDATE_TIMER(beepToneTimer,duration);
    beepOn = true;
  }
}

// Check that the beep time is elapsed and stop it if needed
// CALLED only once in updateIO
void updateBeep(unsigned int pin)
{
  if (beepOn && TIMER_ELAPSED(beepToneTimer))
  {
    noTone(pin);
    beepOn = false;
  }
}

// Send morse command
void sendMorse (const char* message, int length)
{
  if (! morseActive) // If no morse is being sent do it otherwise drop
  {
    morseActive = true;
    morseStr = (char*)message;
    strCounter = 0;
    strCounterMax = length;
  }
}

// Morse generation task
void morseGenerator()
{
  static bool newChar = true;
  static char currentChar = 0;
  static char symCounter = 0;

  if (newChar && morseActive) // Find first one
  {
    currentChar = morse_ascii[morseStr[strCounter]];
    strCounter++;
    if (strCounter >= strCounterMax) // If end of string stop sending
    {
      morseActive = false;
      currentChar = 0;
    }

    while (!(currentChar & 0x80) && (symCounter < 8)) // Shift to find first one
    {
      currentChar <<= 1;
      symCounter ++;
    }
    currentChar <<= 1; // Shift one to get the first element of the morse symbol
    symCounter ++;
    newChar = false;
  }

  if (TIMER_ELAPSED(morseTimer) && morseActive) // Generate symbols for every 
  {
    if (symCounter < 8) 
    {
      if (currentChar & 0x80) // Decode the higher bit
        morseDash();
      else
        morseDot();
  
      currentChar <<= 1; 
      symCounter ++;
    }
    else
    {
      symCounter = 0;
      newChar = true;
      currentChar = 0x0C;
      UPDATE_TIMER(morseTimer,MORSE_LETTER_SPC);
    }
  }
}

void morseDash()
{
  if (TIMER_ELAPSED(morseTimer))
  {
    startBeep (PIN_MORSEOUT, MORSE_FREQ, MORSE_DASH);
    UPDATE_TIMER(morseTimer,MORSE_DASH+MORSE_SYM_SPC);
  }
}

void morseDot()
{
  if (TIMER_ELAPSED(morseTimer))
  {
    startBeep (PIN_MORSEOUT, MORSE_FREQ, MORSE_DOT);
    UPDATE_TIMER(morseTimer,MORSE_DOT+MORSE_SYM_SPC);
  }
}

