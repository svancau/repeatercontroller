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

// BEHAVIOUR (set to 1 to enable, 0 to disable)
#define USE_1750_OPEN 1
#define USE_CTCSS_OPEN 1
#define USE_CARRIER_OPEN 1

#define USE_CTCSS_BUSY 1
#define USE_CARRIER_BUSY 1

#define USE_BEEP 1

// BEEP Frequencies
#define BEEP_FREQ 800
#define MORSE_FREQ 800

// System Defines (DO NOT TOUCH)
#define REPEATER_CLOSED 0
#define REPEATER_OPEN 1

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

bool beepEnabled; // A Roger beep can be sent
bool sqlOpen; // Current Squelch status
bool beepOn = false; // Beep is currently enabled

void loop()
{
  updateIO(); // Control PTT
  setRepeaterState();
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
      noTone(PIN_MORSEOUT); // Remove any beep
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
    }

    // Roger Beep
    if (beepEnabled && TIMER_ELAPSED(rogerBeepTimer)) // PTT Release time to roger beep
    {
      Serial.print ("Beep\n");
      beepEnabled = false;
      startBeep (PIN_MORSEOUT, BEEP_FREQ, BEEP_LENGTH);
    }

    break;
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
    UPDATE_TIMER(beepToneTimer,BEEP_LENGTH);
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

