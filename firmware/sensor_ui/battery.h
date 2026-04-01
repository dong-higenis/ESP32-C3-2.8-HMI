#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>
#include "hw_def.h"

// EMA 필터
#define BAT_EMA_ALPHA             0.05f

// 초기화 시 ADC 안정화 샘플 수
#define BAT_INIT_SAMPLES          20

// // 히스테리시스
// #define BAT_HYSTERESIS            1  // 최소 변화량 (%)

// 충전 전압 오프셋 상수
#define BAT_CHARGE_VOLTAGE_OFFSET 0.02f  

// 시스템 작동 최저 전압
#define BAT_SHUTDOWN_VOLTAGE      2.8f

typedef enum
{
  VOLTAGE = 0,
  PERCENTAGE,
  OCV_MAX
} ocv_t;

/**
 * @brief 배터리 모듈 초기화
 */
void batteryInit(void);

/**
 * @brief 배터리 상태 업데이트
 */
void batteryUpdate(void);

/**
 * @brief 현재 배터리 퍼센트 반환
 */
int8_t batteryGetPercent(void);

/**
 * @brief 충전 중 여부 반환
 */
bool batteryIsCharging(void);

/**
 * @brief 배터리 잔량 없음 여부 반환
 */
bool batteryIsDown(void);

#endif
