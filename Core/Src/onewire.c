#include "onewire.h"

static inline void line_low(void) {
  HAL_GPIO_WritePin(ONEWIRE_GPIO_Port, ONEWIRE_Pin, GPIO_PIN_RESET);
}
static inline void line_release(void) {
  HAL_GPIO_WritePin(ONEWIRE_GPIO_Port, ONEWIRE_Pin, GPIO_PIN_SET);
}
static inline uint8_t line_read(void) {
  return (uint8_t)HAL_GPIO_ReadPin(ONEWIRE_GPIO_Port, ONEWIRE_Pin);
}

void DWT_Delay_Init(void) {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
void DWT_Delay_us(uint32_t us) {
  uint32_t cycles = (SystemCoreClock / 1000000U) * us;
  uint32_t start = DWT->CYCCNT;
  while ((DWT->CYCCNT - start) < cycles) { __NOP(); }
}

bool ONEWIRE_Reset(void) {
  uint8_t presence;
  __disable_irq();
  line_low();            DWT_Delay_us(480);
  line_release();        DWT_Delay_us(70);
  presence = (line_read() == GPIO_PIN_RESET);
  __enable_irq();
  DWT_Delay_us(410);
  return presence;
}

void ONEWIRE_WriteBit(uint8_t bit) {
  __disable_irq();
  line_low();
  if (bit) { DWT_Delay_us(6); line_release(); DWT_Delay_us(64); }
  else     { DWT_Delay_us(60); line_release(); DWT_Delay_us(10); }
  __enable_irq();
}

uint8_t ONEWIRE_ReadBit(void) {
  uint8_t r;
  __disable_irq();
  line_low();            DWT_Delay_us(3);
  line_release();        DWT_Delay_us(10);
  r = line_read();
  __enable_irq();
  DWT_Delay_us(55);
  return (r == GPIO_PIN_SET) ? 1U : 0U;
}

void ONEWIRE_WriteByte(uint8_t byte) {
  for (int i = 0; i < 8; i++) { ONEWIRE_WriteBit(byte & 0x01U); byte >>= 1; }
}
uint8_t ONEWIRE_ReadByte(void) {
  uint8_t b = 0;
  for (int i = 0; i < 8; i++) { b >>= 1; if (ONEWIRE_ReadBit()) b |= 0x80; }
  return b;
}
