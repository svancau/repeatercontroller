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
  ddsTuningWord = (4294967295UL / (CPU_FREQ/256)) * freq;
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

