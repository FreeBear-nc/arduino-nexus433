/* SPDX-License-Identifier: MIT
   SPDX-FileCopyrightText: © 2022 phpcoder https://github.com/phpcoder
*/
/*
  Decoder for Nexus Protocol
*/

#include <Arduino.h>
#include "Nexus_Decoder.h"

#ifndef NEXUS_RX_PIN
#define NEXUS_RX_PIN 14
#endif
#ifdef BUILTIN_LED
#define blink_led(x) digitalWrite(BUILTIN_LED, x)
#else
#define blink_led(x) {}
#endif

enum DecoderState { 
  IDLE, 

  // NEXUX States
  CATCH_SYNC_HIGH, 
  CATCH_SYNC_LOW, 
  READ_BIT_START,
  READ_BIT_END

};

#define PULSE_LENGTH_FIT(duration, discr, tol) (discr>duration-tol && discr<duration+tol)

#define NEXUS_PULSE_HIGH 500
#define NEXUS_PULSE_LOW_SYNC 4000
#define NEXUS_PULSE_LOW_ONE 2000
#define NEXUS_PULSE_LOW_ZERO 1000
#define NEXUS_BIT_TOLERANCE 300

#define NEXUS_PACKET_BITS_COUNT 36 // data bits in a packet

volatile uint32_t RX_Last_Change = 0;    // global var to store last signal transition timestamp

// protocol decoder variables
uint64_t RX_Bits = 0;           // received data bit array
uint8_t  RX_Bit_Counter = 0;    // received data bit counter
uint8_t  RX_Pin = 14;           // GPIO pin connected to the receiver - default GPIO14

volatile bool int_enable = true;
DecoderState s = CATCH_SYNC_HIGH;

void config_receiver (uint8_t pin)
{
  RX_Pin = pin;
  pinMode(pin, INPUT);
#ifdef BUILTIN_LED
  pinMode(BUILTIN_LED, OUTPUT);
#endif
  printf("NEXUS433 set up interrupt on GPIO%i\n", pin);
  // Set protocol ISR
  attachInterrupt(digitalPinToInterrupt(pin), Nexus_Decoder, CHANGE);
}

NEXUS_DATA decode_nexus_data ()
{
  NEXUS_DATA nexus_data;

  nexus_data.Timestamp   = millis();
  nexus_data.ID          = (RX_Bits & 0xFF0000000ULL)  >> 28;
  nexus_data.Battery     = (RX_Bits & 0x008000000ULL)  >> 27;
  nexus_data.Channel     = ((RX_Bits & 0x003000000ULL) >> 24);
  nexus_data.Temperature = ((RX_Bits & 0x000FFF000ULL) >> 12) * 0.1f;
  nexus_data.Humidity    = (RX_Bits & 0x0000000FFULL);
  return nexus_data;
}

void IRAM_ATTR Nexus_Decoder ()
{
  if (int_enable == 0) {
    return;
  }
  uint32_t Current_Timestamp = micros();
  uint32_t Pulse_Duration    = Current_Timestamp - RX_Last_Change;
  uint8_t  Current_Level     = digitalRead(RX_Pin);
  RX_Last_Change = Current_Timestamp;

  switch (s) {
    case CATCH_SYNC_HIGH: 
      //                      ______
      // catch pulse - HIGH  |<---->|
      if ( Current_Level == LOW ) {
        if ( PULSE_LENGTH_FIT(Pulse_Duration, NEXUS_PULSE_HIGH, NEXUS_BIT_TOLERANCE) ) {
          // pulse is within tolerance, continue
          s = CATCH_SYNC_LOW;
          return;
        } else {
          // wrong timing, start over
          break;
        }
      }
      return;

    case CATCH_SYNC_LOW: 
      //                     <---->
      // catch pulse - LOW  |______|
      if (Current_Level == HIGH) {
        if ( PULSE_LENGTH_FIT(Pulse_Duration, NEXUS_PULSE_LOW_SYNC, NEXUS_BIT_TOLERANCE) ) {
          // pulse is within tolerance, continue
          s = READ_BIT_START;
          RX_Bit_Counter = 0;
          RX_Bits = 0;
          return;
        } else {
          // wrong timing, start over
          break;
        }
      }
      return;

    // Sync bit is already captured, data bit sequence started
    case READ_BIT_START:
      if (Current_Level == LOW) {
        // bit start (high) is received
        // we do not analyse pulse length
        s = READ_BIT_END;
      }
      return;

    case READ_BIT_END:
      if (Current_Level == HIGH) {
        // zero bit end (low) is received
        if ( PULSE_LENGTH_FIT(Pulse_Duration, NEXUS_PULSE_LOW_ZERO, NEXUS_BIT_TOLERANCE) ) {
          // zero bit is received
        } else if ( PULSE_LENGTH_FIT(Pulse_Duration, NEXUS_PULSE_LOW_ONE, NEXUS_BIT_TOLERANCE) ) {
          // one bit is received
          RX_Bits |= (1ULL << (NEXUS_PACKET_BITS_COUNT - 1 - RX_Bit_Counter));
        } else {
          // Error in pulse length, cancel packet
          blink_led(0);
          break;
        }
        blink_led(1);
        RX_Bit_Counter++;
        s = READ_BIT_START;
        // check if all data received
        if (RX_Bit_Counter == NEXUS_PACKET_BITS_COUNT) {
          // all data bits are captured
          int_enable = 0;
          blink_led(0);
          break;
        }
        return;
      }
      return;

    default: return;
  }

  // wrong timing or error, start over
  s = CATCH_SYNC_HIGH;

}
