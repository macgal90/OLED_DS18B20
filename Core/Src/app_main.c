#include "app_main.h"
#include "main.h"
#include <math.h>
#include <stdio.h>
#include "oled/ssd1306.h"
#include "oled/ssd1306_fonts.h"
#include "ds18b20.h"

// ===== Sceny =====
typedef enum { SC_VIEW = 0, SC_SETPOINT = 1, SC_HYST = 2 } Scene;
static volatile Scene g_scene = SC_VIEW;

// ===== Parametry regulatora (na razie tylko do wyświetlania) =====
static float g_setpoint = 22.0f;  // °C
static float g_hyst     = 0.6f;   // °C

// ===== Debounce przycisku B1 (PC13) =====
static uint32_t g_lastSceneMs = 0;
static const uint32_t SCENE_DEBOUNCE_MS = 200;

// ===== Flaga natychmiastowego odświeżenia UI =====
static volatile uint8_t g_uiDirty = 1;

// ===== Filtr temperatury (EMA) =====
static float g_tempFilt = NAN;
static const float alpha = 0.25f;

// ===== Bufor na linie tekstu =====
static char line[24];

static const char* scene_name(Scene s) __attribute__((unused));
static const char* scene_name(Scene s) {
  switch (s) {
    case SC_VIEW:     return "1: Odczyt";
    case SC_SETPOINT: return "2: Zadana";
    case SC_HYST:     return "3: Histereza";
    default:          return "?";
  }
}

// ====== Rysowanie ekranu (bez odczytu czujnika!) ======
static void draw_screen(void) {
  ssd1306_Fill(Black);

#if (SSD1306_HEIGHT <= 32)
  /* ======= 128x32 – układ kompaktowy ======= */
  switch (g_scene) {
    case SC_VIEW: {
      if (isnan(g_tempFilt)) snprintf(line, sizeof(line), "T: --.- C");
      else                   snprintf(line, sizeof(line), "T: %.1f C", g_tempFilt);
      ssd1306_SetCursor(0, 0);                 // 0..17 (11x18)
      ssd1306_WriteString(line, Font_11x18, White);

      snprintf(line, sizeof(line), "SP: %.1f H: %.1f", g_setpoint, g_hyst);
      ssd1306_SetCursor(0, 22);                // 22..31 (7x10)
      ssd1306_WriteString(line, Font_7x10, White);
    } break;

    case SC_SETPOINT: {
      ssd1306_SetCursor(0, 0);                 // 0..9
      ssd1306_WriteString("2: Zadana", Font_7x10, White);

      snprintf(line, sizeof(line), "SP: %.1f C", g_setpoint);
      ssd1306_SetCursor(0, 12);                // 12..29
      ssd1306_WriteString(line, Font_11x18, White);
    } break;

    case SC_HYST: {
      ssd1306_SetCursor(0, 0);
      ssd1306_WriteString("3: Histereza", Font_7x10, White);

      snprintf(line, sizeof(line), "H: %.1f C", g_hyst);
      ssd1306_SetCursor(0, 12);
      ssd1306_WriteString(line, Font_11x18, White);
    } break;
  }

#else
  /* ======= 128x64 – wygodny układ ======= */

  // nagłówek
  ssd1306_SetCursor(0, 0);
  ssd1306_WriteString(scene_name(g_scene), Font_7x10, White);

  switch (g_scene) {
    case SC_VIEW: {
      if (isnan(g_tempFilt)) snprintf(line, sizeof(line), "T: --.- C");
      else                   snprintf(line, sizeof(line), "T: %.2f C", g_tempFilt);
      ssd1306_SetCursor(0, 14);                // 14..39 (16x26)
      ssd1306_WriteString(line, Font_16x26, White);

      snprintf(line, sizeof(line), "SP: %.1f  H: %.1f", g_setpoint, g_hyst);
      ssd1306_SetCursor(0, 46);
      ssd1306_WriteString(line, Font_7x10, White);
    } break;

    case SC_SETPOINT: {
      snprintf(line, sizeof(line), "SP: %.1f C", g_setpoint);
      ssd1306_SetCursor(0, 18);
      ssd1306_WriteString(line, Font_16x26, White);

      ssd1306_SetCursor(0, 48);
      ssd1306_WriteString("UP/DOWN: zmiana", Font_7x10, White);
    } break;

    case SC_HYST: {
      snprintf(line, sizeof(line), "H: %.1f C", g_hyst);
      ssd1306_SetCursor(0, 18);
      ssd1306_WriteString(line, Font_16x26, White);

      ssd1306_SetCursor(0, 48);
      ssd1306_WriteString("UP/DOWN: zmiana", Font_7x10, White);
    } break;
  }
#endif

  ssd1306_UpdateScreen();
}

// ===== Główna pętla aplikacji =====
void App_Main(void) {
  uint32_t lastUi = 0;
  uint32_t lastT  = 0;

  const uint32_t uiPeriodMs = 200;  // odświeżanie ekranu
  const uint32_t tPeriodMs  = 1000; // odświeżanie pomiaru

  for (;;) {
    uint32_t now = HAL_GetTick();

    // Odczyt DS18B20 rzadziej – mniej zakłóceń I2C i stabilniejszy ekran
    if (now - lastT >= tPeriodMs) {
      lastT = now;
      float t;
      if (DS18B20_ReadTemperature(&t)) {
        if (isnan(g_tempFilt)) g_tempFilt = t;
        else                   g_tempFilt = alpha * t + (1.0f - alpha) * g_tempFilt;
        g_uiDirty = 1; // mamy nowe dane – przerysuj
      }
    }

    // natychmiastowe odświeżenie po zmianie sceny/danych
    if (g_uiDirty) {
      g_uiDirty = 0;
      draw_screen();
    }

    // cykliczne odświeżanie (na wypadek, gdyby coś się zmieniło w tle)
    if (now - lastUi >= uiPeriodMs) {
      lastUi = now;
      draw_screen();
    }

    // (tu później dorzucisz UP/DOWN dla zmiany SP/H)
  }
}

// ===== Callback EXTI – B1 (PC13): zmiana sceny + odświeżenie =====
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_13)  // przycisk B1
  {
    uint32_t now = HAL_GetTick();
    if (now - g_lastSceneMs >= SCENE_DEBOUNCE_MS) {  // debounce 200ms
      Scene next = (Scene)((g_scene + 1) % 3);
      // krótki log – nie spamuj w przerwaniu
      printf("B1->scena:%d\r\n", (int)next);
      g_scene = next;
      g_lastSceneMs = now;
      g_uiDirty = 1; // wymuś natychmiastowe rysowanie nowej sceny
    }
  }
}
