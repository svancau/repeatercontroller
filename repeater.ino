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
#define USE_DEBUGMODE 1

#define DEFAULT_UNO

// PINS
#ifdef DEFAULT_UNO
#define PIN_1750 10
#define PIN_CTCSS 12
#define PIN_CARRIER 6
#define PIN_PTT 13
// Pins for Audio Mux
#define PIN_AMUX0 8
#define PIN_AMUX1 9
// PIN for MORSE OUTPUT is fixed to 5 for Arduino Leonardo
// PIN for MORSE OUTPUT is fixed to 11 for Arduino Uno
// /!\ WARNING DO NOT USE tone() function as we act directly on TIMERS

// DTMF decoder
#define PIN_8870_D0 2
#define PIN_8870_D1 3
#define PIN_8870_D2 4
#define PIN_8870_D3 5
#define PIN_8870_STB 7
#endif

// DELAYS (in ms)
#define INACTIVE_CLOSE 15000
#define RELEASE_BEEP 200
#define ROGER_BEEP_MINIMUM 2000
#define BEEP_LENGTH 100
#define PTT_ON_DELAY 1000
#define PTT_OFF_DELAY 1000
#define CARRIER_1750_DELAY 200

// DELAYS (in ms)
#define BEACON_DELAY (600UL*1000UL)
#define TIMEOUT_DELAY (600UL*1000UL)

#define MORSE_WPM 20

// Morse definitions (do not touch)
#define MORSE_DOT (1200/MORSE_WPM)
#define MORSE_DASH (3*MORSE_DOT)
// Spaces equal Number of dots - 1
#define MORSE_SYM_SPC MORSE_DOT
#define MORSE_LETTER_SPC (2*MORSE_DOT)
#define MORSE_WORD_SPC (5*MORSE_DOT)

// BEHAVIOUR (set to 1 to enable, 0 to disable)
#define USE_1750_OPEN 1
#define USE_CTCSS_OPEN 1
#define USE_CARRIER_OPEN 0

#define USE_CTCSS_BUSY 1
#define USE_CARRIER_BUSY 1
#define USE_OPEN_ROGER_BEEP 0

// BEHAVIOUR of the roger beep define only _ONE_ of those or none
//#define ROGER_TONE
#define ROGER_K
//#define ROGER_SMETER

// BEEP Frequencies (in tens of Hertz)
#define BEEP_FREQ 8000
#define MORSE_FREQ 6500

#define CPU_FREQ 16000000UL

// Define Test Pin (test millis() reference on PIN 12 500 Hz signal)
//#define TEST_ENABLE
#define TEST_PIN 12

// Messages
#define POWER_ON_MSG "ON4SEB QRV"
#define OPENMSG "ON4SEB"
#define CLOSEMSG "ON4SEB SK"
#define KMSG "K"
#define BEACONMSG "ON4SEB"
#define TIMEOUTMSG "TOT"

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

enum adminState_t {ADMIN_IDLE, ADMIN_AUTH, ADMIN_CMD};
adminState_t adminState = ADMIN_IDLE;

// Type definitions
typedef unsigned char uchar;
typedef unsigned long ulong;
struct Config_t
{
  bool onBeaconEnabled;
  bool offBeaconEnabled;
  bool rogerBeepEnabled;
  bool repeaterEnabled;
};

struct Config_t Configuration;

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
  repeaterSetup();
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
  pinMode (PIN_AMUX0, OUTPUT);
  pinMode (PIN_AMUX1, OUTPUT);

  // Set it low by default
  digitalWrite(PIN_PTT, LOW);
  digitalWrite(PIN_AMUX0, LOW);
  digitalWrite(PIN_AMUX1, LOW);
}

void repeaterSetup()
{
  Configuration.onBeaconEnabled = true;
  Configuration.offBeaconEnabled = true;
  Configuration.rogerBeepEnabled = true;
  Configuration.repeaterEnabled = true;
}

void debugPrint (String msg)
{
#if (USE_DEBUGMODE)
  Serial.print (String(millis()));
  Serial.println (" "+msg);
#endif
}

/////////////////////////////////////////////
// Main loop
/////////////////////////////////////////////
State_t State = REPEATER_CLOSED;
State_t nextState = REPEATER_CLOSED; // State after PTT ON delay

// Timers
ulong closeTimer;
ulong rogerBeepTimer;
ulong rogerBeepMinumumTimer; // Minumum time before triggering the roger beep
ulong beaconTimer;
ulong timeoutTimer; // Cut too long keydowns
ulong pttEnableTimer; // PTT enabled to morse
ulong pttDisableTimer; // End of morse to PTT off
ulong carrier1750openTimer; // Carrier/1750 length before opening

bool beepEnabled; // A Roger beep can be sent
bool timeOutEnabled; // TimeOut is enabled
bool prevRxActive; // Current Squelch status
bool beepOn = false; // Beep is currently enabled
bool morseActive = false; // currently sending morse
bool firstIdentification = true; // First time the CPU

String cwMessage; // Message to send when off
unsigned int cwFreq;

// Analog measurements
int analogValues [6];

void loop()
{
  updateIO(); // Control PTT
  setRepeaterState();
  morseGenerator(); // Morse generator task
  beaconTask();
  dtmfCaptureTask();
  analogTask();
#if defined(TEST_ENABLE)
  testTask();
#endif
#if (!USE_DEBUGMODE)
  wdt_reset(); // reset 8s watchdog
#else
  if (millis() % 60000 == 0)
    debugPrint ("Timestamp");
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
        debugPrint ("Opening");
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
      UPDATE_TIMER(rogerBeepTimer,RELEASE_BEEP); // Roger beep start timer
      timeOutEnabled = true;
    }
    else
    {
      UPDATE_TIMER(timeoutTimer, TIMEOUT_DELAY); // Update only when PTT is released
      timeOutEnabled = false;
    }

    if (!prevRxActive && rxActive()) // On squelch opening, rising edge
    {
      UPDATE_TIMER(rogerBeepMinumumTimer,ROGER_BEEP_MINIMUM); // Count Minimum time to trigger roger beep
    }

    if (prevRxActive && !rxActive()) // If the Squelch was previously opened, falling edge
    {
      if (TIMER_ELAPSED(rogerBeepMinumumTimer)) // Enable roger beep after some time only
      {
        beepEnabled = true;
      }
    }

    prevRxActive = rxActive();

    // Set timeout after a known time    
    if (TIMER_ELAPSED(closeTimer))
    {
      debugPrint ("Closing");
      sendMorse (CLOSEMSG, MORSE_FREQ);
      State = REPEATER_CLOSING;
      UPDATE_TIMER(beaconTimer,BEACON_DELAY); // Force Identification BEACON_DELAY time after closing
    }

    // If the PTT is hold for too long, close the repeater
    if (timeOutEnabled && TIMER_ELAPSED(timeoutTimer))
    {
       debugPrint ("Timeout");
       sendMorse (TIMEOUTMSG, MORSE_FREQ);
       State = REPEATER_CLOSING;
       UPDATE_TIMER(beaconTimer,BEACON_DELAY); // Force Identification BEACON_DELAY time after timeout
       timeOutEnabled = false;
    }

    // Roger Beep
    if (beepEnabled && TIMER_ELAPSED(rogerBeepTimer)) // PTT Release time to roger beep
    {
      debugPrint ("Beep");
      beepEnabled = false;
      rogerBeep();
    }

    break;

    // --------------- REPEATER SENDING MORSE WHILE OPENING -------
    case REPEATER_OPENING: // Wait with PTT on till there is no morse to send
    static bool openSent = false;

    if (!openSent) // Send ID only once
      {
        sendMorse (OPENMSG, MORSE_FREQ);
        openSent = true;
      }

    if (!morseActive)
      {
        openSent = false;
        State = REPEATER_OPEN;
        if (USE_OPEN_ROGER_BEEP)
          rogerBeep(); // Send a roger beep once opened
      }

    break;

    // --------------- REPEATER SENDING MORSE WHILE OPENED -------
    case REPEATER_CLOSING: // Wait with PTT on till there is no more morse to send
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
        sendMorse (cwMessage, cwFreq);
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
  if (Configuration.rogerBeepEnabled)
  {
#ifdef ROGER_TONE
    if (!morseActive)
      startBeep (BEEP_FREQ, BEEP_LENGTH);
#endif

#ifdef ROGER_K
    sendMorse(KMSG, BEEP_FREQ);
#endif

#ifdef ROGER_SMETER
    // TODO: Attach a Smeter measurement to this
    sendMorse("9", BEEP_FREQ);
#endif
  }
}

// Beacon task
void beaconTask()
{
  if (TIMER_ELAPSED(beaconTimer))
  {
    if (State == REPEATER_CLOSED && Configuration.offBeaconEnabled)
    {
      if (firstIdentification) // Power on identification message
      {
        sendClosedMorse (POWER_ON_MSG, MORSE_FREQ);
        firstIdentification = false;
      }
      else
      {
        sendClosedMorse (BEACONMSG, MORSE_FREQ);
      }

      debugPrint ("Beacon closed");
      UPDATE_TIMER(beaconTimer, BEACON_DELAY);
    }
    else if (State == REPEATER_OPEN && (!morseActive) && (!beepEnabled) && (!rxActive()) && Configuration.onBeaconEnabled)
    {
      sendMorse (BEACONMSG, MORSE_FREQ);
      debugPrint ("Beacon open");
      UPDATE_TIMER(beaconTimer, BEACON_DELAY); // Update timer here to ensure that it will be retransmitted when rx is not active
    }
  }
}

// Control IO Pins
void updateIO()
{
  // Set the PTT control depending on repeater state
  if (State != REPEATER_CLOSED || morseActive)
    digitalWrite(PIN_PTT, HIGH);
  else
    digitalWrite(PIN_PTT, LOW);

  // Control the Audio Mux depending on repeater state
  if (State != REPEATER_OPEN)
  {
    // Use CW input
    digitalWrite(PIN_AMUX0, HIGH);
    digitalWrite(PIN_AMUX1, LOW);
  }
  else
  {
    if (morseActive || beepOn || adminState != ADMIN_IDLE || !rxActive()) // If no beep, morse or DTMF is present
    {
      // Use CW input
      digitalWrite(PIN_AMUX0, HIGH);
      digitalWrite(PIN_AMUX1, LOW);
    }
    else
    {
      // Use Audio Input
      digitalWrite(PIN_AMUX0, HIGH);
      digitalWrite(PIN_AMUX1, HIGH);
    }
  }

  updateBeep(); // Stop the beep when timer elapsed
}

// Check if opening is requested
bool openRequest()
{
  return (((USE_1750_OPEN && digitalRead(PIN_1750))
    || (USE_CTCSS_OPEN && digitalRead(PIN_CTCSS))
    || (USE_CARRIER_OPEN && digitalRead(PIN_CARRIER)))
    && Configuration.repeaterEnabled
    && adminState == ADMIN_IDLE); // Do not open repeater when in admin mode
}

// Check if RX channel is active
bool rxActive()
{
  return ((USE_CTCSS_BUSY && digitalRead(PIN_CTCSS))
    || (USE_CARRIER_BUSY && digitalRead(PIN_CARRIER)));
}

// Send Morse when closed with PTT control
void sendClosedMorse(String msg, unsigned int freq)
{
  nextState = REPEATER_ID;
  State = REPEATER_PTTON;
  UPDATE_TIMER(pttEnableTimer, PTT_ON_DELAY);
  cwMessage = msg;
  cwFreq = freq;
}

#if defined(TEST_ENABLE)
void testTask()
{
  pinMode (TEST_PIN, OUTPUT);
  digitalWrite (TEST_PIN, millis() & 1);
}
#endif

