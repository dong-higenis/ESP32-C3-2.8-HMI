#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>
#include "hw_def.h"

// ── OCV table column indices ──
enum { VOLTAGE = 0, PERCENTAGE = 1, OCV_MAX = 2 };

// ── ADC / voltage calibration ──
#define BAT_CHARGE_VOLTAGE_OFFSET  0.05f
#define BAT_INIT_SAMPLES           8

// ── Shutdown threshold ──
#define BAT_SHUTDOWN_VOLTAGE       3.0f

// ── Voltage filter ──
// ADC 전압을 평활화하는 EMA 계수
// 작을수록 느리고 부드럽다 (0.02 = 차이의 2%씩 반영)
#define BAT_VOLTAGE_EMA_ALPHA      0.03f

// ── SOC filter ──
// 평활화된 전압 퍼센트를 SOC에 반영하는 EMA 계수
// 전압 EMA 위에 이중으로 걸리므로 충분히 작아야 한다
// 0.005 = 차이의 0.5%씩 반영, 100ms 주기 기준 초당 약 5%
#define BAT_SOC_EMA_ALPHA          0.002f

// ── Display filter ──
#define BAT_DISPLAY_EMA_ALPHA      0.05f

// ── Charge pin debounce ──
#define BAT_CHARGE_DEBOUNCE_MS     50

// ── Init charge pin retry ──
#define BAT_INIT_CHARGE_RETRY_COUNT  5
#define BAT_INIT_CHARGE_RETRY_MS     10

#define BAT_INIT_VALUE               -1.0f

// ── API ──
void   batteryInit(void);
void   batteryUpdate(void);
int8_t batteryGetPercent(void);
bool   batteryIsCharging(void);
bool   batteryIsChargeDone(void);
bool   batteryIsDown(void);

#endif // BATTERY_H