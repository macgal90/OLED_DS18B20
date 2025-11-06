#ifndef DS18B20_H
#define DS18B20_H
#include <stdint.h>
#include <stdbool.h>

bool DS18B20_IsPresent(void);
bool DS18B20_StartConversion(void);
bool DS18B20_WaitForConversion(uint32_t timeout_ms);
bool DS18B20_ReadScratchpad(uint8_t buf[9]);
bool DS18B20_ReadTemperature(float *out_celsius);

#endif
