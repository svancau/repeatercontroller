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

// Constants
// Thanks to KB8OJH
#define MORSE_NONE 1
PROGMEM prog_uchar morse_ascii[] = {
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

