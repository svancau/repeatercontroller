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

// Calibration values (output in mV by default)
#define VREF 5000
#define RANGE 256

#define A0_NUM 1
#define A0_DEN 1
#define A0_OFFSET 0

#define A1_NUM 1
#define A1_DEN 1
#define A1_OFFSET 0

#define A2_NUM 1
#define A2_DEN 1
#define A2_OFFSET 0

#define A3_NUM 1
#define A3_DEN 1
#define A3_OFFSET 0

#define A4_NUM 1
#define A4_DEN 1
#define A4_OFFSET 0

#define A5_NUM 1
#define A5_DEN 1
#define A5_OFFSET 0

void analogTask()
{
  static bool acquiredInThisCycle = false;
  int rawAnalog [6];
  long intermediateVal [6];
  if (millis() % 1000) // Measure every second
  {
    if (!acquiredInThisCycle) // Acquire measurements only once
    {
      rawAnalog[0] = analogRead (A0);
      rawAnalog[1] = analogRead (A1);
      rawAnalog[2] = analogRead (A2);
      rawAnalog[3] = analogRead (A3);
      rawAnalog[4] = analogRead (A4);
      rawAnalog[5] = analogRead (A5);

      // Compute Physical value
      // /!\ Keep output within a 16 bits signed range -32768 to 32767 /!\
      // ------------------------------------------------------------------

      intermediateVal [0] = ((VREF * rawAnalog [0] * A0_NUM) / A0_DEN / RANGE) - A0_OFFSET;
      analogValues[0] = (int)intermediateVal [0];

      intermediateVal [1] = ((VREF * rawAnalog [1] * A1_NUM) / A1_DEN / RANGE) - A1_OFFSET;
      analogValues[1] = (int)intermediateVal [1];

      intermediateVal [2] = ((VREF * rawAnalog [2] * A2_NUM) / A2_DEN / RANGE) - A2_OFFSET;
      analogValues[2] = (int)intermediateVal [2];

      intermediateVal [3] = ((VREF * rawAnalog [3] * A3_NUM) / A3_DEN / RANGE) - A3_OFFSET;
      analogValues[3] = (int)intermediateVal [3];

      intermediateVal [4] = ((VREF * rawAnalog [4] * A4_NUM) / A4_DEN / RANGE) - A4_OFFSET;
      analogValues[4] = (int)intermediateVal [4];

      intermediateVal [5] = ((VREF * rawAnalog [5] * A5_NUM) / A5_DEN / RANGE) - A5_OFFSET;
      analogValues[5] = (int)intermediateVal [5];

      acquiredInThisCycle = true;
    }
  }
  else // Outside the 1 ms window every second, reset the flag
  {
    acquiredInThisCycle = false;
  }
}

