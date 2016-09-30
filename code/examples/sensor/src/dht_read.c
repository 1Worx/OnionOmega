// Copyright (c) 2014 Adafruit Industries
// Author: Tony DiCola

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "dht_read.h"
//#include "onion_mmio.h"
#include "gpio.h"

// This is the only processor specific magic value, the maximum amount of time to
// spin in a loop before bailing out and considering the read a timeout.  This should
// be a high value, but if you're running on a much faster platform than a Raspberry
// Pi or Beaglebone Black then it might need to be increased.
#define DHT_MAXCOUNT 32000

// Number of bit pulses to expect from the DHT.  Note that this is 41 because
// the first pulse is a constant 50 microsecond pulse, with 40 pulses to represent
// the data afterwards.
#define DHT_PULSES 41

int _dht_read(int type, int pin, float* humidity, float* temperature) {
	int T;
  // Validate humidity and temperature arguments and set them to zero.
  if (humidity == NULL || temperature == NULL)
  {
    return DHT_ERROR_ARGUMENT;
  }
  *temperature = -255.0f;
  *humidity = -255.0f;

  //SetVerbosity(0);
  //SetDebugMode(0);

  // Initialize GPIO library.
  //if (pi_mmio_init() < 0) {
  if (false)
  {
    return DHT_ERROR_GPIO;
  }

  // Store the count that each DHT bit pulse is low and high.
  // Make sure array is initialized to start at zero.
  int pulseCounts[DHT_PULSES*2] = {0};

  // Set pin to output.
  //pi_mmio_set_output(pin);
  int ok = SetDirection(pin, 1);
  twSleepMsec(10);
  // Bump up process priority and change scheduler to try to try to make process more 'real time'.
  set_max_priority();

  // Set pin high for ~500 milliseconds.
  //pi_mmio_set_high(pin);
  ok = Set(pin, 1);
  //twSleepMsec(500);
  sleep_milliseconds(500);

  // The next calls are timing critical and care should be taken
  // to ensure no unnecssary work is done below.

  // Set pin low for ~20 milliseconds.
  //pi_mmio_set_low(pin);
  ok = Set(pin, 0);
  busy_wait_milliseconds(20);

  // Set pin at input.
  //pi_mmio_set_input(pin);
  ok = SetDirection(pin, 0);
  //twSleepMsec(10);
  // Need a very short delay before reading pins or else value is sometimes still low.
  {
    volatile int i = 0;
    for (i; i < 50; ++i)
    {
    }
  }

  // Wait for DHT to pull pin low.
  uint32_t count = 0;
  int value = 0;
  
  //while (pi_mmio_input(pin))
  do
  {
    if (++count >= DHT_MAXCOUNT)
    {
      // Timeout waiting for response.
		printf("first timeout");
      set_default_priority();
	  return DHT_ERROR_TIMEOUT;
    }

    Read(pin, &value);
    //printf("value: %i\n", value);
  }while (value);

  {
    int i = 0;
    // Record pulse widths for the expected result bits.
    for (i; i < DHT_PULSES*2; i+=2)
    {
      // Count how long pin is low and store in pulseCounts[i]
      //while (!pi_mmio_input(pin))
      do
      {
        if (++pulseCounts[i] >= DHT_MAXCOUNT)
        {
			printf("second timeout\n");
          // Timeout waiting for response.
          set_default_priority();
		  return DHT_ERROR_TIMEOUT;
        }
        Read(pin, &value);
       // printf("value2: %i, i=%d\n", value, i);
      }while (!value);

      // Count how long pin is high and store in pulseCounts[i+1]
      //while (pi_mmio_input(pin))
      do
      {
        if (++pulseCounts[i+1] >= DHT_MAXCOUNT)
        {
          // Timeout waiting for response.

			printf("third timeout");
          set_default_priority();
		  return DHT_ERROR_TIMEOUT;
        }
        Read(pin, &value);
        //printf("value3: %i\n", value);
      }while (value);
    }
  }

  // Done with timing critical code, now interpret the results.

  // Drop back to normal priority.
  set_default_priority();

  // Compute the average low pulse width to use as a 50 microsecond reference threshold.
  // Ignore the first two readings because they are a constant 80 microsecond pulse.
  uint32_t threshold = 0;
  {
    int i = 2;
    for (i; i < DHT_PULSES*2; i+=2) {
      threshold += pulseCounts[i];
    }
  }
  threshold /= DHT_PULSES-1;

  // Interpret each high pulse as a 0 or 1 by comparing it to the 50us reference.
  // If the count is less than 50us it must be a ~28us 0 pulse, and if it's higher
  // then it must be a ~70us 1 pulse.
  uint8_t data[5] = {0};
  {
    int i = 3;
    for (i; i < DHT_PULSES*2; i+=2) {
      int index = (i-3)/16;
      data[index] <<= 1;
      if (pulseCounts[i] >= threshold) {
        // One bit for long pulse.
        data[index] |= 1;
      }
      // Else zero bit for short pulse.
    }
  }

  // Useful debug info:
  //printf("Data: 0x%x 0x%x 0x%x 0x%x 0x%x\n", data[0], data[1], data[2], data[3], data[4]);

  // Verify checksum of received data.
  if (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
    if (type == DHT11) {
      // Get humidity and temp for DHT11 sensor.
      *humidity = (float)data[0];
      *temperature = (float)data[2];
    }
    else if (type == DHT22) {
      // Calculate humidity and temp for DHT22 sensor.
      *humidity = (data[0] * 256 + data[1]) / 10.0f;
      *temperature = ((data[2] & 0x7F) * 256 + data[3]) / 10.0f;
      if (data[2] & 0x80) {
        *temperature *= -1.0f;
      }
    }
    return DHT_SUCCESS;
  }
  else {
    return DHT_ERROR_CHECKSUM;
  }
}


int dht_read(int type, int pin, float* humidity, float* temperature) {
	int result = 0,i =0;
	for ( i = 0; i < 3; i++) 
	{
		result = _dht_read(type, pin, humidity, temperature);
		if (result == 0) 
			break;
	}
	return result;
}