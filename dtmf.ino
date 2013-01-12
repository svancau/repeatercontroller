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

#define dtmfBufferSz 4

#define DTMF_ENTER_CODE "A52D"
#define DTMF_PASS "1234"

const char authMsg[] = "AUTH";
const char okMsg[] = "OK";
const char nokMsg[] = "KO";

char dtmfString[dtmfBufferSz+1]; // Keep place for trailing zero
bool prevStrobe = false;
uchar dtmfStrIndex;

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
     dtmfString[dtmfStrIndex] = dtmfChar;
     dtmfStrIndex = (dtmfStrIndex+1) % dtmfBufferSz;
     if (dtmfStrIndex == 0) // If we got 4 chars, interpret it
     {
       debugPrint ("Analyzing DTMF code");
       interpretDTMF();
     }
   }

  prevStrobe = digitalRead(PIN_8870_STB);
}

void interpretDTMF()
{
    switch (dtmfState)
    {
      case DTMF_IDLE:
        if (String (dtmfString) == DTMF_ENTER_CODE)
        {
          dtmfState = DTMF_AUTH;
          sendMorse (authMsg);
        }
        break;

      case DTMF_AUTH:
        if (String (dtmfString) == DTMF_PASS)
        {
          dtmfState = DTMF_CMD;
          sendMorse (okMsg);
        }
        else
        {
          dtmfState = DTMF_IDLE;
          sendMorse(nokMsg);
        }
        break;

      case DTMF_CMD:
        break;
    }
}
