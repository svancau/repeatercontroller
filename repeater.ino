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

#define BEEP_OFF 0
#define BEEP_ON 1


typedef unsigned char uchar;

/////////////////////////////////////////////
// Setup phase
/////////////////////////////////////////////
void setup()
{
  ioSetup();  
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

unsigned long beepEvent;
unsigned long inputEvent;
unsigned long mikeReleaseEvent;
unsigned long idEvent;

void loop()
{
  updateIO();
  checkRepeaterState();
}

void checkRepeaterState()
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
    }
    break;

  case REPEATER_OPEN:
    // Keep repeater open as long there is some input
    if ((USE_CTCSS && digitalRead(PIN_CTCSS))
      || (USE_COR && digitalRead(PIN_CARRIER)))
    {
      inputEvent = millis();
    }

    // Set timeout after a known time    
    if ((millis() - inputEvent) >= INACTIVE_CLOSE)
    {
      State = REPEATER_CLOSED; 
    }

    // Roger Beep
    if ((millis() - inputEvent) >= RELEASE_BEEP) // PTT Release time to roger beep
    {
      beepEvent = millis();
      tone (PIN_MORSEOUT, BEEP_FREQ);
      mikeReleaseEvent = millis();
    }
    
    // Stop beep
    if ((millis() - beepEvent) >= BEEP_LENGTH)
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


