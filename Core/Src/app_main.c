#include "app_main.h"
#include "oled/ssd1306.h"
#include "oled/ssd1306_fonts.h"
#include "ds18b20.h"
#include <stdio.h>
#include "main.h"  // dla HAL_GetTick(), HAL_Delay itp.

void App_Main(void)
{
    float t = 0.0f;
    char buf[16];
    uint32_t last = 0;

    while (1)
    {
        uint32_t now = HAL_GetTick();
        if (now - last >= 1000)  // co 1s
        {
            last = now;
            ssd1306_Fill(Black);

            if (DS18B20_ReadTemperature(&t))
            {
                sprintf(buf, "%.2f C", t);
                ssd1306_SetCursor(10, 10);
                ssd1306_WriteString(buf, Font_11x18, White);
            }
            else
            {
                ssd1306_SetCursor(10, 10);
                ssd1306_WriteString("Blad czyt", Font_11x18, White);
            }

            ssd1306_UpdateScreen();
        }

        // obsługa przycisku B1 np.:
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)
        {
            printf("Wcisnieto przycisk!\r\n");
            HAL_Delay(200);
        }
    }
}
