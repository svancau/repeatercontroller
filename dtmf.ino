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

ulong adminModeExitTimer;
ulong bufferClearTimer; // Clear buffer after some time

#define ADMIN_TIMEOUT 30000u
#define BUFFER_CLEAR 2000u

#define dtmfBufferSz 4

#define DTMF_ENTER_CODE "A52D"
#define DTMF_PASS "1234"

// DTMF Commands
#define CMD_DTMF_ALL_OFF "0000"
#define CMD_DTMF_ALL_ON "1000"
#define CMD_DTMF_EXIT "####"

#define AUTHMSG "AUTH"
#define OKMSG "OK"
#define NOKMSG "KO"
#define EXITMSG "EXIT"

String dtmfString = "    ";
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
     debugPrint (String("Got DTMF ")+(char)dtmfChar);
     dtmfString[dtmfStrIndex] = dtmfChar;
     dtmfStrIndex = (dtmfStrIndex+1) % dtmfBufferSz;
     if (dtmfStrIndex == 0) // If we got 4 chars, interpret it
     {
       debugPrint ("Analyzing DTMF code " + dtmfString);
       interpretDTMF();
     }
     UPDATE_TIMER(adminModeExitTimer, ADMIN_TIMEOUT);
     UPDATE_TIMER(bufferClearTimer, BUFFER_CLEAR);
   }

  // After some time, clear the input buffer
  if (dtmfStrIndex != 0 && TIMER_ELAPSED(bufferClearTimer))
  {
    debugPrint ("Clear DTMF buffer");
    dtmfString = "    ";
    dtmfStrIndex = 0;
  }

  // Exit Admin mode some time after the last command
  if (dtmfState != DTMF_IDLE && TIMER_ELAPSED(adminModeExitTimer))
  {
    exitAdminMode();
  }

  prevStrobe = digitalRead(PIN_8870_STB);
}

void interpretDTMF()
{
    UPDATE_TIMER(adminModeExitTimer, ADMIN_TIMEOUT); // Update Admin mode exit timer every time a new word is received
    switch (dtmfState)
    {
      case DTMF_IDLE:
        if (dtmfString == DTMF_ENTER_CODE)
        {
          dtmfState = DTMF_AUTH;
          sendMorse (AUTHMSG, MORSE_FREQ);
        }
        else
        {
          sendMorse (NOKMSG, MORSE_FREQ);
        }
        break;

      case DTMF_AUTH:
        if (dtmfString == DTMF_PASS)
        {
          dtmfState = DTMF_CMD;
          sendMorse (OKMSG, MORSE_FREQ);
        }
        else
        {
          dtmfState = DTMF_IDLE;
          sendMorse(NOKMSG, MORSE_FREQ);
        }
        break;

      case DTMF_CMD:
        if (dtmfString == CMD_DTMF_ALL_OFF)
        {
          Configuration.onBeaconEnabled = false;
          Configuration.offBeaconEnabled = false;
          Configuration.repeaterEnabled = false;
          sendMorse (OKMSG, MORSE_FREQ);
        }
        else if (dtmfString == CMD_DTMF_ALL_ON)
        {
          Configuration.onBeaconEnabled = true;
          Configuration.offBeaconEnabled = true;
          Configuration.repeaterEnabled = true;
          sendMorse (OKMSG, MORSE_FREQ);
        }
        else if (dtmfString == CMD_DTMF_EXIT)
        {
          exitAdminMode();
        }

        // Default case : generate error
        else
        {
          sendMorse(NOKMSG, MORSE_FREQ);
        }

        break;
    }
}

// Action to do on admin mode exit
void exitAdminMode ()
{
  sendMorse ("EXIT", MORSE_FREQ);
  dtmfState = DTMF_IDLE;
}

