#include "ds18b20.h"
#include "onewire.h"

#define CMD_SKIP_ROM      0xCC
#define CMD_CONVERT_T     0x44
#define CMD_READ_SCRATCH  0xBE

static uint8_t ds_crc8(const uint8_t *data, uint8_t len) {
  uint8_t crc = 0;
  for (uint8_t i = 0; i < len; i++) {
    uint8_t in = data[i];
    for (uint8_t j = 0; j < 8; j++) {
      uint8_t mix = (crc ^ in) & 0x01;
      crc >>= 1;
      if (mix) crc ^= 0x8C;
      in >>= 1;
    }
  }
  return crc;
}

bool DS18B20_IsPresent(void) { return ONEWIRE_Reset(); }

bool DS18B20_StartConversion(void) {
  if (!ONEWIRE_Reset()) return false;
  ONEWIRE_WriteByte(CMD_SKIP_ROM);
  ONEWIRE_WriteByte(CMD_CONVERT_T);
  return true;
}

bool DS18B20_WaitForConversion(uint32_t timeout_ms) {
  while (timeout_ms--) {
    if (ONEWIRE_ReadBit()) return true; // 1 = gotowe
    HAL_Delay(1);
  }
  return false;
}

bool DS18B20_ReadScratchpad(uint8_t buf[9]) {
  if (!ONEWIRE_Reset()) return false;
  ONEWIRE_WriteByte(CMD_SKIP_ROM);
  ONEWIRE_WriteByte(CMD_READ_SCRATCH);
  for (int i = 0; i < 9; i++) buf[i] = ONEWIRE_ReadByte();
  return ds_crc8(buf, 8) == buf[8];
}

bool DS18B20_ReadTemperature(float *out_celsius) {
  if (!DS18B20_StartConversion()) return false;
  if (!DS18B20_WaitForConversion(800)) return false;
  uint8_t s[9];
  if (!DS18B20_ReadScratchpad(s)) return false;
  int16_t raw = (int16_t)((s[1] << 8) | s[0]);
  *out_celsius = (float)raw / 16.0f;
  return true;
}
