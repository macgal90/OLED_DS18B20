#include "app_main.h"
#include "main.h"
#include <math.h>
#include <stdio.h>

#include "oled/ssd1306.h"
#include "oled/ssd1306_fonts.h"
#include "ds18b20.h"

// ===== Stałe wysokości czcionek =====
#define FH_7x10   10
#define FH_11x18  18
#define XOF 4   // mały margines w poziomie

// ===== Sceny =====
typedef enum { SC_VIEW = 0, SC_SETPOINT = 1, SC_HYST = 2 } Scene;
static volatile Scene g_scene = SC_VIEW;

// ===== Parametry regulatora =====
static float g_setpoint = 25.0f;  // °C
static float g_hyst     = 0.6f;   // °C

// ===== Debounce przycisku B1 (PC13) =====
static uint32_t g_lastSceneMs = 0;
static const uint32_t SCENE_DEBOUNCE_MS = 200;

// ===== Flaga odświeżenia UI =====
static volatile uint8_t g_uiDirty = 1;

// ===== Filtr temperatury (EMA) =====
static float g_tempFilt = NAN;
static const float alpha = 0.25f;

// ===== Bufor na tekst =====
static char line[24];

// ===== Piny przycisków i wyjścia =====
#define BTN_UP_Port    GPIOC
#define BTN_UP_Pin     GPIO_PIN_1
#define BTN_DOWN_Port  GPIOC
#define BTN_DOWN_Pin   GPIO_PIN_0

#define RELAY_Port     GPIOA
#define RELAY_Pin      GPIO_PIN_5   // LED LD2 lub przekaźnik

// ===== Polaryzacja przekaźnika =====
#define RELAY_ACTIVE_LOW  0   // 1 jeśli moduł aktywny stanem niskim

static volatile uint8_t g_relay_state = 0; // 0=OFF, 1=ON

// ===== Funkcja pomocnicza =====
static void relay_apply(void) {
  GPIO_PinState level;
#if RELAY_ACTIVE_LOW
  level = g_relay_state ? GPIO_PIN_RESET : GPIO_PIN_SET;
#else
  level = g_relay_state ? GPIO_PIN_SET : GPIO_PIN_RESET;
#endif
  HAL_GPIO_WritePin(RELAY_Port, RELAY_Pin, level);
}

// ====== Rysowanie ekranu ======
static void draw_screen(void) {
  ssd1306_Fill(Black);

#if (SSD1306_HEIGHT <= 32)
  switch (g_scene) {

    // ===== SCENA 0: PODGLĄD =====
    case SC_VIEW: {
      // Linia 1 – temperatura
      if (isnan(g_tempFilt))
        snprintf(line, sizeof(line), "T: --.-C");
      else
        snprintf(line, sizeof(line), "T:%4.1fC", g_tempFilt);

      ssd1306_SetCursor(XOF, 0);
      ssd1306_WriteString(line, Font_11x18, White);

      // Linia 2 – COOLING lub SP/H
      int y2 = SSD1306_HEIGHT - FH_7x10 - 1;
      ssd1306_SetCursor(XOF, y2);
      if (g_relay_state) {
        ssd1306_WriteString("COOLING", Font_7x10, White);
      } else {
        snprintf(line, sizeof(line), "SP:%4.1f H:%3.1f", g_setpoint, g_hyst);
        ssd1306_WriteString(line, Font_7x10, White);
      }
    } break;

    // ===== SCENA 1: ZADANA =====
    case SC_SETPOINT: {
      ssd1306_SetCursor(XOF, 0);
      ssd1306_WriteString("Zadana", Font_7x10, White);
      snprintf(line, sizeof(line), "%4.1f C", g_setpoint);
      ssd1306_SetCursor(XOF, 12);
      ssd1306_WriteString(line, Font_11x18, White);
    } break;

    // ===== SCENA 2: HISTER. =====
    case SC_HYST: {
      ssd1306_SetCursor(XOF, 0);
      ssd1306_WriteString("Histereza", Font_7x10, White);
      snprintf(line, sizeof(line), "%3.1f C", g_hyst);
      ssd1306_SetCursor(XOF, 12);
      ssd1306_WriteString(line, Font_11x18, White);
    } break;
  }

#else
  // --- OLED 128x64 ---
  ssd1306_SetCursor(0, 0);
  switch (g_scene) {
    case SC_VIEW: {
      if (isnan(g_tempFilt))
        snprintf(line, sizeof(line), "T: --.- C");
      else
        snprintf(line, sizeof(line), "T: %.2f C", g_tempFilt);
      ssd1306_SetCursor(0, 14);
      ssd1306_WriteString(line, Font_16x26, White);

      ssd1306_SetCursor(0, 46);
      if (g_relay_state)
        ssd1306_WriteString("COOLING", Font_7x10, White);
      else {
        snprintf(line, sizeof(line), "SP: %.1f  H: %.1f", g_setpoint, g_hyst);
        ssd1306_WriteString(line, Font_7x10, White);
      }
    } break;

    case SC_SETPOINT: {
      snprintf(line, sizeof(line), "SP: %.1f C", g_setpoint);
      ssd1306_SetCursor(0, 18);
      ssd1306_WriteString(line, Font_16x26, White);
    } break;

    case SC_HYST: {
      snprintf(line, sizeof(line), "H: %.1f C", g_hyst);
      ssd1306_SetCursor(0, 18);
      ssd1306_WriteString(line, Font_16x26, White);
    } break;
  }
#endif

  ssd1306_UpdateScreen();
}

// ===== Główna pętla aplikacji =====
void App_Main(void) {
  uint32_t lastUi = 0;
  uint32_t lastT  = 0;
  const uint32_t uiPeriodMs = 200;
  const uint32_t tPeriodMs  = 1000;

  GPIO_PinState prevUp   = HAL_GPIO_ReadPin(BTN_UP_Port,   BTN_UP_Pin);
  GPIO_PinState prevDown = HAL_GPIO_ReadPin(BTN_DOWN_Port, BTN_DOWN_Pin);

  g_relay_state = 0;
  relay_apply();

  for (;;) {
    uint32_t now = HAL_GetTick();

    // --- przyciski ---
    GPIO_PinState curUp   = HAL_GPIO_ReadPin(BTN_UP_Port,   BTN_UP_Pin);
    GPIO_PinState curDown = HAL_GPIO_ReadPin(BTN_DOWN_Port, BTN_DOWN_Pin);

    if (prevUp == GPIO_PIN_SET && curUp == GPIO_PIN_RESET) {
      if (g_scene == SC_SETPOINT) {
        g_setpoint += 0.1f;
        if (g_setpoint > 35.0f) g_setpoint = 35.0f;
      } else if (g_scene == SC_HYST) {
        g_hyst += 0.1f;
        if (g_hyst > 5.0f) g_hyst = 5.0f;
      }
      g_uiDirty = 1;
    }

    if (prevDown == GPIO_PIN_SET && curDown == GPIO_PIN_RESET) {
      if (g_scene == SC_SETPOINT) {
        g_setpoint -= 0.1f;
        if (g_setpoint < 5.0f) g_setpoint = 5.0f;
      } else if (g_scene == SC_HYST) {
        g_hyst -= 0.1f;
        if (g_hyst < 0.1f) g_hyst = 0.1f;
      }
      g_uiDirty = 1;
    }

    prevUp = curUp;
    prevDown = curDown;

    // --- odczyt temperatury co 1 s ---
    if (now - lastT >= tPeriodMs) {
      lastT = now;
      float t;
      if (DS18B20_ReadTemperature(&t)) {
        if (isnan(g_tempFilt)) g_tempFilt = t;
        else g_tempFilt = alpha * t + (1.0f - alpha) * g_tempFilt;
        g_uiDirty = 1;
      }

      // --- logika chłodzenia (z histerezą) ---
      if (!isnan(g_tempFilt)) {
        uint8_t prevRelay = g_relay_state;

        if (g_tempFilt > (g_setpoint + g_hyst / 2.0f))
          g_relay_state = 1;
        else if (g_tempFilt < (g_setpoint - g_hyst / 2.0f))
          g_relay_state = 0;

        if (g_relay_state != prevRelay) {
          relay_apply();
          g_uiDirty = 1; // przerysuj COOLING/SP-H
        }
      }
    }

    // --- odświeżanie ekranu ---
    if (g_uiDirty) {
      g_uiDirty = 0;
      draw_screen();
    }

    if (now - lastUi >= uiPeriodMs) {
      lastUi = now;
      draw_screen();
    }
  }
}

// ===== Przycisk B1 (PC13): zmiana sceny =====
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == GPIO_PIN_13) {
    uint32_t now = HAL_GetTick();
    if (now - g_lastSceneMs >= SCENE_DEBOUNCE_MS) {
      g_scene = (Scene)((g_scene + 1) % 3);
      g_lastSceneMs = now;
      g_uiDirty = 1;
    }
  }
}
