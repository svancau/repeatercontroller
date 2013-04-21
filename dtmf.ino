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
#define BUFFER_CLEAR 1500u

#define dtmfBufferSz 4

#define DTMF_ENTER_CODE "A52D"
#define DTMF_PASS "1234"

// Messages
#define AUTHMSG "PASS"
#define OKMSG "OK"
#define NOKMSG "ERR"
#define EXITMSG "EXIT"
#define PERMMSG "PERM"

// Command dispatcher return values
#define RET_CMD_OK 0
#define RET_CMD_FAIL 1
#define RET_CMD_PERM 2
#define RET_CMD_NOTEXIST 3

String dtmfString = "    ";
bool prevStrobe = false;
uchar dtmfStrIndex;

// Structure containing all the possible commands
typedef struct
{
  String commandStr; // 4 Digits command string
  bool priviledged; // Checks for authentication for this function
  void (*runFunction)(void); // Call function when OK
}
commandEntry_t;

// Do not forget to update this constant when adding entries to commandList
#define COMMAND_COUNT 3

commandEntry_t commandList[COMMAND_COUNT]
  // CODE   PERMISSION REQUIRED   FUNCTION NAME
= {
   {"0000", true,                 &adminDisableAll},
   {"1000", true,                 &adminEnableAll},
   {"####", true,                 &adminExitMode},
  };

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
  if (adminState != ADMIN_IDLE && TIMER_ELAPSED(adminModeExitTimer))
  {
    adminExitMode();
  }

  prevStrobe = digitalRead(PIN_8870_STB);
}

// Interpret function and administration FSM
void interpretDTMF()
{
    UPDATE_TIMER(adminModeExitTimer, ADMIN_TIMEOUT); // Update Admin mode exit timer every time a new word is received
    switch (adminState)
    {
      case ADMIN_IDLE: // Enter privileged mode
        if (dtmfString == DTMF_ENTER_CODE)
        {
          adminState = ADMIN_AUTH;
          sendMorse (AUTHMSG, MORSE_FREQ);
        }
        else // Use unprivileged commands
        {
          getCommands();
        }
        break;

      case ADMIN_AUTH: // Get credentials
        if (dtmfString == DTMF_PASS)
        {
          adminState = ADMIN_CMD;
          sendMorse (OKMSG, MORSE_FREQ);
        }
        else
        {
          adminState = ADMIN_IDLE;
          sendMorse(PERMMSG, MORSE_FREQ);
        }
        break;

      case ADMIN_CMD: // Privileged mode
        getCommands();
        break;
    }
}

// Get commands from command list
void getCommands()
{
  char retVal = RET_CMD_NOTEXIST;
  int i;
  for (i = 0; i < COMMAND_COUNT; i++)
  {
    if (commandList[i].commandStr == dtmfString) // Check for input command
    {
      debugPrint ("Found match for "+dtmfString);
      if (commandList[i].priviledged)
      {
        if (adminState == ADMIN_CMD) // Check for permissions
        { // Return OK, run function, function need to send whatever it wants in morse
          debugPrint ("Permission granted for "+dtmfString);
          retVal = RET_CMD_OK;
        }
        else // Permission denied, go to admin mode first
        {
          debugPrint ("Permission denied for "+dtmfString);
          sendMorse (PERMMSG, MORSE_FREQ);
          retVal = RET_CMD_PERM;
        }
      }
      else // No permission required
      { // Return OK, run function, function need to send whatever it wants in morse
          debugPrint ("No permission required for "+dtmfString);
          retVal = RET_CMD_OK;
      }
      break;
    }
  }

  if (retVal == RET_CMD_OK) // Action for OK commands
  {
    if (commandList[i].runFunction)
      commandList[i].runFunction();
  }

  if (retVal == RET_CMD_NOTEXIST) // Action for Not existing commands
  {
    debugPrint ("No match found for "+dtmfString);
    sendMorse (NOKMSG, MORSE_FREQ);
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions to be executed upon DTMF command
// prototype is void function (void);
// Each function has to send the morse feedback itself
// DO NOT INLINE

// Enable all (Beacon when opened, close, repeater itself)
void adminEnableAll()
{
  Configuration.onBeaconEnabled = true;
  Configuration.offBeaconEnabled = true;
  Configuration.repeaterEnabled = true;
  sendMorse ("OK QRV", MORSE_FREQ);
}

// Disable all (Beacon when opened, close, repeater itself)
void adminDisableAll()
{
  Configuration.onBeaconEnabled = false;
  Configuration.offBeaconEnabled = false;
  Configuration.repeaterEnabled = false;
  sendMorse ("OK QRT", MORSE_FREQ);
}

// Action to do on admin mode exit
void adminExitMode()
{
  sendMorse ("EXIT", MORSE_FREQ);
  adminState = ADMIN_IDLE;
}

