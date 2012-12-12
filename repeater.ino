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
#define USE_1750 1
#define USE_CTCSS 1
#define USE_COR 1
#define USE_BEEP 1

// BEEP Frequencies
#define BEEP_FREQ 800
#define MORSE_FREQ 800

// System Defines (DO NOT TOUCH)
#define REPEATER_CLOSED 0
#define REPEATER_OPEN 1

typedef unsigned char uchar;

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

unsigned long inputEvent;
unsigned long mikeReleaseEvent;
unsigned long idEvent;
bool beepEnabled;

void loop()
{
  updateIO(); // Control PTT
  setRepeaterState();
}

void setRepeaterState()
{
  switch (State) 
  {
  case REPEATER_CLOSED:
    // Open Repeater
    if ((USE_1750 && digitalRead(PIN_1750))
      || (USE_CTCSS && digitalRead(PIN_CTCSS))
      || (USE_COR && digitalRead(PIN_CARRIER)))
    {
      inputEvent = millis();
      State = REPEATER_OPEN;
      Serial.print ("Opening\n");
    }
    break;

  case REPEATER_OPEN:
    // Keep repeater open as long there is some input
    if ((USE_CTCSS && digitalRead(PIN_CTCSS))
      || (USE_COR && digitalRead(PIN_CARRIER)))
    {
      inputEvent = millis();
      beepEnabled = true;
    }

    // Set timeout after a known time    
    if ((millis() - inputEvent) >= INACTIVE_CLOSE)
    {
      State = REPEATER_CLOSED; 
      Serial.print ("Closing\n");
    }

    // Roger Beep
    if (beepEnabled && ((millis() - inputEvent) >= RELEASE_BEEP)) // PTT Release time to roger beep
    {
      tone (PIN_MORSEOUT, BEEP_FREQ);
      Serial.print ("Beep\n");
      beepEnabled = false;
      mikeReleaseEvent = millis();
    }
    
    // Stop beep
    if (!beepEnabled && ((millis() - inputEvent) >= (RELEASE_BEEP+BEEP_LENGTH)))
    {
      noTone(PIN_MORSEOUT); 
    }
    break;
  }
}

void updateIO()
{
  // Set the PTT control depending on repeater state
  if (State != REPEATER_CLOSED)
    digitalWrite(PIN_PTT, HIGH);
  else
    digitalWrite(PIN_PTT, LOW);
}


