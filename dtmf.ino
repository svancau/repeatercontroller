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

// MT8870 DTMF table
PROGMEM prog_uchar dtmf_table[16] =
{'D','1','2','3','4','5','6','7',
 '8','9','0','*','#','A','B','C'};

#define dtmfBufferSz 16

char decodedString[dtmfBufferSz];
bool prevStrobe = false;
uchar decodedStrIndex;

void dtmfCaptureTask()
{
  uchar dtmfIn;
  uchar dtmfChar;
  if ((!prevStrobe) && digitalRead(PIN_8870_STB)) // If rising edge on 8870 strobe
  {
     dtmfIn = (digitalRead(PIN_8870_D0) | (digitalRead(PIN_8870_D1) << 1) |
     (digitalRead(PIN_8870_D2) << 2) | (digitalRead(PIN_8870_D3) << 3)); // Generate Input
     dtmfChar = pgm_read_byte_near (dtmf_table + dtmfIn);
     debugPrint (String("Got DTMF ")+dtmfChar+"\n");
     decodedString[decodedStrIndex] = dtmfChar;
     decodedStrIndex = (decodedStrIndex+1) % dtmfBufferSz;
   }

  prevStrobe = digitalRead(PIN_8870_STB);
}

