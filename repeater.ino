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

#include <avr/wdt.h>

////////////////////////////////////////////
// Static Repeater Configuration
////////////////////////////////////////////

// Debug Mode (set to 0 to enable Watchdog)
#define USE_DEBUGMODE 0

// PINS
#define PIN_1750 4
#define PIN_CTCSS 5
#define PIN_CARRIER 6
#define PIN_PTT 13
// PIN for MORSE OUTPUT is fixed to 6 for Arduino Leonardo
// /!\ WARNING DO NOT USE tone() function as we act directly on TIMERS

// DELAYS (in ms)
#define INACTIVE_CLOSE 10000
#define RELEASE_BEEP 200
#define BEEP_LENGTH 100
#define PTT_ON_DELAY 1000
#define PTT_OFF_DELAY 1000
#define CARRIER_1750_DELAY 200

// DELAYS (in ms)
#define BEACON_DELAY (600UL*1000UL)
#define TIMEOUT_DELAY (600UL*1000UL)

#define MORSE_DOT 60

// Morse definitions (do not touch)
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

// BEHAVIOUR of the roger beep define only _ONE_ of those
//#define ROGER_TONE
#define ROGER_K

// BEEP Frequencies
#define BEEP_FREQ 800
#define MORSE_FREQ 800

// Messages
const char openMsg[] = "ON4SEB";
const char closeMsg[] = "ON4SEB SK";
const char kMsg[] = "K";
const char beaconMsg[] = "ON4SEB";
const char timeoutMsg[] = "TOT";

// System Defines (DO NOT TOUCH)
enum State_t {
  REPEATER_CLOSED, // Repeater IDLE
  REPEATER_OPEN, // Repeater working
  REPEATER_ID, // Repeater identifying when IDLE
  REPEATER_OPENING, // Repeater sending morse opening message
  REPEATER_CLOSING, // Repeater sending morse closing message
  REPEATER_PTTON, // Wait before opening
  REPEATER_PTTOFF // Wait after closing
};

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

// Sine wave table
PROGMEM prog_uchar sine[] = {
  127,130,133,136,139,143,146,149,152,155,158,161,164,167,170,173,
  176,178,181,184,187,190,192,195,198,200,203,205,208,210,212,215,
  217,219,221,223,225,227,229,231,233,234,236,238,239,240,242,243,
  244,245,247,248,249,249,250,251,252,252,253,253,253,254,254,254,
  254,254,254,254,253,253,253,252,252,251,250,249,249,248,247,245,
  244,243,242,240,239,238,236,234,233,231,229,227,225,223,221,219,
  217,215,212,210,208,205,203,200,198,195,192,190,187,184,181,178,
  176,173,170,167,164,161,158,155,152,149,146,143,139,136,133,130,
  127,124,121,118,115,111,108,105,102,99,96,93,90,87,84,81,78,76,
  73,70,67,64,62,59,56,54,51,49,46,44,42,39,37,35,33,31,29,27,25,
  23,21,20,18,16,15,14,12,11,10,9,7,6,5,5,4,3,2,2,1,1,1,0,0,0,0,
  0,0,0,1,1,1,2,2,3,4,5,5,6,7,9,10,11,12,14,15,16,18,20,21,23,25,
  27,29,31,33,35,37,39,42,44,46,49,51,54,56,59,62,64,67,70,73,76,
  78,81,84,87,90,93,96,99,102,105,108,111,115,118,121,124
};

/////////////////////////////////////////////
// Setup phase
/////////////////////////////////////////////
void setup()
{
  ioSetup();
  setupTimer();
  Serial.begin(115200);
#if (!USE_DEBUGMODE)
  wdt_enable (WDTO_8S); // Enable watchdog
#endif
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
State_t State = REPEATER_CLOSED;
State_t nextState = REPEATER_CLOSED; // State after PTT ON delay

// Timers
ulong closeTimer;
ulong rogerBeepTimer;
ulong beepToneTimer;
ulong morseTimer;
ulong beaconTimer;
ulong timeoutTimer; // Cut too long keydowns
ulong pttEnableTimer; // PTT enabled to morse
ulong pttDisableTimer; // End of morse to PTT off
ulong carrier1750openTimer; // Carrier/1750 length before opening

ulong ddsPhaseAccu; // DDS Phase accumulator

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
#if (!USE_DEBUGMODE)
  wdt_reset(); // reset 8s watchdog
#endif
}

void setRepeaterState()
{
  switch (State) 
  {
  // ----------- REPEATER IS IDLE ----------------
  case REPEATER_CLOSED:
    // Open Repeater
    if (openRequest())
    {
      if (TIMER_ELAPSED(carrier1750openTimer))
      {
        UPDATE_TIMER(closeTimer,INACTIVE_CLOSE);
        State = REPEATER_PTTON;
        nextState = REPEATER_OPENING;
        UPDATE_TIMER(pttEnableTimer, PTT_ON_DELAY);
        Serial.print ("Opening\n");
      }
    }
    else
    { // When no opening request is done, update timer
      UPDATE_TIMER(carrier1750openTimer, CARRIER_1750_DELAY);
    }

    break;

  // ----------- REPEATER IS OPENED ----------------
  case REPEATER_OPEN:
    // Keep repeater open as long there is some input
    if (rxActive())
    {
      UPDATE_TIMER(closeTimer, INACTIVE_CLOSE); // Repeater closing timer
      sqlOpen = true;
    }
    else
    {
      UPDATE_TIMER(timeoutTimer, TIMEOUT_DELAY); // Update only when PTT is released
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
      Serial.print ("Closing\n");
      sendMorse (closeMsg, (int) sizeof(closeMsg));
      State = REPEATER_CLOSING;
      UPDATE_TIMER(beaconTimer,BEACON_DELAY); // Force Identification BEACON_DELAY time after closing
    }

    // If the PTT is hold for too long, close the repeater
    if (TIMER_ELAPSED(timeoutTimer))
    {
       Serial.print ("Timeout\n");
       sendMorse (timeoutMsg, (int)sizeof(timeoutMsg));
       State = REPEATER_CLOSING;
       UPDATE_TIMER(beaconTimer,BEACON_DELAY); // Force Identification BEACON_DELAY time after timeout
    }

    // Roger Beep
    if (beepEnabled && TIMER_ELAPSED(rogerBeepTimer)) // PTT Release time to roger beep
    {
      Serial.print ("Beep\n");
      beepEnabled = false;
      rogerBeep();
    }

    break;

    // --------------- REPEATER SENDING MORSE WHILE OPENING -------
    case REPEATER_OPENING: // Wait with PTT on till there is no morse to send
    static bool openSent = false;

    if (!openSent) // Send ID only once
      {
        sendMorse (openMsg, (int)sizeof(openMsg));
        openSent = true;
      }

    if (!morseActive)
      {
        openSent = false;
        State = REPEATER_OPEN;
      }

    break;

    // --------------- REPEATER SENDING MORSE WHILE OPENED -------
    case REPEATER_CLOSING: // Wait with PTT on till there is no morse to send
    if (!morseActive)
      {
        State = REPEATER_PTTOFF;
        UPDATE_TIMER(pttDisableTimer, PTT_OFF_DELAY);
      }

    break;

    // --------------- REPEATER SENDING MORSE WHILE CLOSED -------
    case REPEATER_ID: // Wait with PTT on till there is no morse to send
    static bool idSent = false;

    if (!idSent) // Send ID only once
      {
        sendMorse (beaconMsg, (int)sizeof(beaconMsg));
        idSent = true;
      }

    if (!morseActive)
      {
        idSent = false;
        State = REPEATER_PTTOFF;
        UPDATE_TIMER(pttDisableTimer, PTT_OFF_DELAY);
      }

    break;

    // --------------- REPEATER ON BEFORE MORSE -------
    case REPEATER_PTTON: // PTT on before sending morse

    if (TIMER_ELAPSED(pttEnableTimer))
    {
      State = nextState;
      
    }

    break;

    // --------------- REPEATER OFF AFTER MORSE -------
    case REPEATER_PTTOFF: // PTT on after sending morse

    if (TIMER_ELAPSED(pttDisableTimer))
    {
      State = REPEATER_CLOSED;
    }

    break;

  }
}

// Generation of the Roger beep
void rogerBeep()
{
#ifdef ROGER_TONE
  if (!morseActive)
    startBeep (BEEP_FREQ, BEEP_LENGTH);
#endif

#ifdef ROGER_K
  sendMorse(kMsg, (int)sizeof(kMsg));
#endif
}

// Beacon task
void beaconTask()
{
  if (TIMER_ELAPSED(beaconTimer))
  {
    if (State == REPEATER_CLOSED)
    {
      nextState = REPEATER_ID;
      State = REPEATER_PTTON;
      UPDATE_TIMER(pttEnableTimer, PTT_ON_DELAY);
      Serial.print ("Beacon closed\n");
    }
    else if (State == REPEATER_OPEN && (!morseActive) && (!beepEnabled) && (!rxActive()))
    {
      sendMorse (beaconMsg, (int)sizeof(beaconMsg));
      Serial.print ("Beacon open\n");
    }

    UPDATE_TIMER(beaconTimer, BEACON_DELAY);
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

  updateBeep(); // Stop the beep when timer elapsed
}

// Configure timer for morse PWM
void setupTimer()
{
#if defined(__AVR_ATmega32U4__)
  TCCR3A = (1 << WGM30); // Enable Fast PWM mode
  TCCR3B = (1 << WGM32) | (1 << CS30); // Prescaler = 1
  DDRC |= (1 << DDC6); // Pin 6 as output
#endif
}

// Do a beep for a known time
void startBeep(unsigned int freq, unsigned long duration)
{
  if (!beepOn)
  {
#if defined(__AVR_ATmega32U4__)
    TCCR3A |= (1 << COM3A1); // Enable comparator output
    TIMSK3 |= (1 << TOIE3); // Enable Timer interrupt
#endif

    UPDATE_TIMER(beepToneTimer,duration);
    beepOn = true;
  }
}

// Check that the beep time is elapsed and stop it if needed
// CALLED only once in updateIO
void updateBeep()
{
  if (beepOn && TIMER_ELAPSED(beepToneTimer))
  {
#if defined(__AVR_ATmega32U4__)
    TCCR3A &= ~(1 << COM3A1); // Enable comparator output
    TIMSK3 &= ~(1 << TOIE3); // Enable Timer interrupt
#endif

    beepOn = false;
  }
}

// Check if opening is requested
bool openRequest()
{
  if ((USE_1750_OPEN && digitalRead(PIN_1750))
      || (USE_CTCSS_OPEN && digitalRead(PIN_CTCSS))
      || (USE_CARRIER_OPEN && digitalRead(PIN_CARRIER)))
      {
        return true;
      }
  else
      {
        return false;
      }
}

// Check if RX channel is active
bool rxActive()
{
   if ((USE_CTCSS_BUSY && digitalRead(PIN_CTCSS))
      || (USE_CARRIER_BUSY && digitalRead(PIN_CARRIER)))
    {
      return true;
    }
    else
    {
      return false;
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
    startBeep (MORSE_FREQ, MORSE_DASH);
    UPDATE_TIMER(morseTimer,MORSE_DASH+MORSE_SYM_SPC);
  }
}

void morseDot()
{
  if (TIMER_ELAPSED(morseTimer))
  {
    startBeep (MORSE_FREQ, MORSE_DOT);
    UPDATE_TIMER(morseTimer,MORSE_DOT+MORSE_SYM_SPC);
  }
}

// Tone generation DDS timer overflow interrupt
// For Arduino Leonardo
#if defined(__AVR_ATmega32U4__)
ISR(TIMER3_OVF_vect)
{
  unsigned char index; // Sample Index
  ddsPhaseAccu += 0x04400000UL;
  index = ddsPhaseAccu >> 24;
  OCR3A = pgm_read_byte_near(sine + index);
}
#endif
