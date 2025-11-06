#ifndef ONEWIRE_H
#define ONEWIRE_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

void DWT_Delay_Init(void);
void DWT_Delay_us(uint32_t us);

bool ONEWIRE_Reset(void);
void ONEWIRE_WriteBit(uint8_t bit);
uint8_t ONEWIRE_ReadBit(void);
void ONEWIRE_WriteByte(uint8_t byte);
uint8_t ONEWIRE_ReadByte(void);

#endif
