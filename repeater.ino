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
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/progmem.h>

////////////////////////////////////////////
// Static Repeater Configuration
////////////////////////////////////////////

// Debug Mode (set to 0 to enable Watchdog)
#define USE_DEBUGMODE 0

// PINS
#define PIN_1750 3
#define PIN_CTCSS 4
#define PIN_CARRIER 6
#define PIN_PTT 13
// PIN for MORSE OUTPUT is fixed to 5 for Arduino Leonardo
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

// BEHAVIOUR of the roger beep define only _ONE_ of those or none
//#define ROGER_TONE
#define ROGER_K

// BEEP Frequencies
#define BEEP_FREQ 800
#define MORSE_FREQ 800

#define CPU_FREQ 16000000UL

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

  // Set it low by default
  digitalWrite(PIN_PTT, LOW);
}

/////////////////////////////////////////////
// Main loop
/////////////////////////////////////////////
State_t State = REPEATER_CLOSED;
State_t nextState = REPEATER_CLOSED; // State after PTT ON delay

// Timers
ulong closeTimer;
ulong rogerBeepTimer;
ulong beaconTimer;
ulong timeoutTimer; // Cut too long keydowns
ulong pttEnableTimer; // PTT enabled to morse
ulong pttDisableTimer; // End of morse to PTT off
ulong carrier1750openTimer; // Carrier/1750 length before opening

bool beepEnabled; // A Roger beep can be sent
bool sqlOpen; // Current Squelch status
bool beepOn = false; // Beep is currently enabled
bool morseActive = false; // currently sending morse

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

// Check if opening is requested
bool openRequest()
{
  return ((USE_1750_OPEN && digitalRead(PIN_1750))
    || (USE_CTCSS_OPEN && digitalRead(PIN_CTCSS))
    || (USE_CARRIER_OPEN && digitalRead(PIN_CARRIER)));
}

// Check if RX channel is active
bool rxActive()
{
  return ((USE_CTCSS_BUSY && digitalRead(PIN_CTCSS))
    || (USE_CARRIER_BUSY && digitalRead(PIN_CARRIER)));
}

