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

ulong morseTimer;

char* morseStr; // Pointer to the morse string
int strCounter; // Morse string index
int strCounterMax; // End of message index

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
    currentChar = pgm_read_byte_near(morse_ascii + morseStr[strCounter]);
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

  if (TIMER_ELAPSED(morseTimer) && morseActive) // Generate symbols for every character
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

