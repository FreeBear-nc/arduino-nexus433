/* SPDX-License-Identifier: MIT
   SPDX-FileCopyrightText: © 2022 phpcoder https://github.com/phpcoder
*/
/*
  Decoder for Nexus Protocol
*/

// Nexus parameters
typedef struct NEXUS_DATA {
  uint16_t Timestamp;
  uint8_t  ID;
  uint8_t  Battery;
  uint8_t  Channel;
  uint8_t  Humidity;
  float    Temperature;
} NEXUS_DATA;


void config_receiver (uint8_t pin);
NEXUS_DATA decode_nexus_data (void);
void Nexus_Decoder (void);
extern volatile bool int_enable;
