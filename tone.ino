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

// Dependant on the arduino model we are using

// Timers
ulong beepToneTimer;
ulong ddsTuningWord; // DDS tuning word
ulong ddsPhaseAccu; // DDS Phase accumulator

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

// Configure timer for morse PWM
void setupTimer()
{
  // Arduino Leonardo
#if defined(__AVR_ATmega32U4__)
  TCCR3A = (1 << WGM30); // Enable Fast PWM mode
  TCCR3B = (1 << WGM32) | (1 << CS30); // Prescaler = 1
  DDRC |= (1 << DDC6); // Pin 6 as output
  // Arduino Uno
#else
  TCCR2A = (1 << WGM21) | (1 << WGM20); // Enable Fast PWM mode
  TCCR2B = (1 << CS20); // Prescaler = 1
  DDRB |= (1 << DDB3); // Pin 3 as output
  OCR2A = 127;
#endif
}

// Do a beep for a known time
void startBeep(unsigned int freq, unsigned long duration)
{
  ddsTuningWord = (4294967295UL / (CPU_FREQ/256)) * freq;
  if (!beepOn)
  {
    // Arduino Leonardo
#if defined(__AVR_ATmega32U4__)
    TCCR3A |= (1 << COM3A1); // Enable comparator output
    TIMSK3 |= (1 << TOIE3); // Enable Timer interrupt
    // Arduino Uno
#else
    TCCR2A |= (1 << COM2A1); // Enable comparator output
    TIMSK2 |= (1 << TOIE2); // Enable Timer interrupt
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
    // Arduino Leonardo
#if defined(__AVR_ATmega32U4__)
    TCCR3A &= ~(1 << COM3A1); // Disable comparator output
    TIMSK3 &= ~(1 << TOIE3); // Disable Timer interrupt
    // Arduino Uno
#else

    TCCR2A &= ~(1 << COM2A1); // Disable comparator output
    TIMSK2 &= ~(1 << TOIE2); // Disable Timer interrupt

#endif

    beepOn = false;
  }
}

// Tone generation DDS timer overflow interrupt
// For Arduino Leonardo
#if defined(__AVR_ATmega32U4__)
ISR(TIMER3_OVF_vect)
{
  unsigned char index; // Sample Index
  ddsPhaseAccu += ddsTuningWord;
  index = ddsPhaseAccu >> 24;
  OCR3A = pgm_read_byte_near(sine + index);
}
#endif

// Tone generation DDS timer overflow interrupt
// For Arduino Uno
#if !defined(__AVR_ATmega32U4__)
ISR(TIMER2_OVF_vect)
{
  unsigned char index; // Sample Index
  ddsPhaseAccu += ddsTuningWord;
  index = ddsPhaseAccu >> 24;
  OCR2A = pgm_read_byte_near(sine + index);
}
#endif



