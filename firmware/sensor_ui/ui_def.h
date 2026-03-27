#ifndef UI_DEF_H
#define UI_DEF_H

#include "hw_def.h"

#define UI_SCREEN_W LCD_WIDTH
#define UI_SCREEN_H LCD_HEIGHT

// ui
#define MIN_MARGIN_OF_ANGLE     0.0011f                             // 0 ~ 90도 사이, 몇도부터 평행하다고 정할지
#define CIRCLE_CENTER_X         190                                 // X : 원 중심 좌표
#define CIRCLE_CENTER_Y         110                                 // Y : 원 중심 좌표
#define BIG_CIRCLE_RADIUS       80                                  // 큰 원 반지름
#define MIDDLE_CIRCLE_RADIUS    30                                  // 중간 원 반지름
#define SMALL_CIRCLE_RADIUS     ((MIDDLE_CIRCLE_RADIUS*3) / 5)      // 작은 원 반지름
#define GUIDE_CIRCLE_RADIUS     SMALL_CIRCLE_RADIUS                 // 사이드 바 원 반지름
#define CROSS_LENGTH            BIG_CIRCLE_RADIUS                   // 중앙 십자가 길이

#define GUIDE_BAR_THICKNESS     MIDDLE_CIRCLE_RADIUS - 4            // 바의 두께
#define GUIDE_BAR_EDGE_RADIUS   ((GUIDE_BAR_THICKNESS) - 2 ) / 2

#define STRING_X_VALUE_START_X  10                     // 센서 X값에 대한 문자열 출력의 시작 X좌표
#define STRING_X_VALUE_START_Y  140                    // 센서 x값에 대한 문자열 출력의 시작 y좌표
#define STRING_Y_VALUE_START_X  10                     // 센서 y값에 대한 문자열 출력의 시작 x좌표
#define STRING_Y_VALUE_START_Y  190                    // 센서 y값에 대한 문자열 출력의 시작 y좌표

#define X_GUIDE_BAR_X   (CIRCLE_CENTER_X - BIG_CIRCLE_RADIUS)
#define X_GUIDE_BAR_Y   (CIRCLE_CENTER_Y + BIG_CIRCLE_RADIUS + 10)
#define X_GUIDE_BAR_W   BIG_CIRCLE_RADIUS * 2
#define X_GUIDE_BAR_H   GUIDE_BAR_THICKNESS

#define Y_GUIDE_BAR_X   (CIRCLE_CENTER_X + BIG_CIRCLE_RADIUS + 10)
#define Y_GUIDE_BAR_Y   (CIRCLE_CENTER_Y - BIG_CIRCLE_RADIUS)
#define Y_GUIDE_BAR_W   GUIDE_BAR_THICKNESS
#define Y_GUIDE_BAR_H   BIG_CIRCLE_RADIUS * 2

#define X_GUIDE_BAR_CENTER_X_VAL CIRCLE_CENTER_X                                                       // X축 가이드 바 중심 좌표(x)  
#define X_GUIDE_BAR_CENTER_Y_VAL CIRCLE_CENTER_Y + BIG_CIRCLE_RADIUS + 10 +((GUIDE_BAR_THICKNESS) / 2) // X축 가이드 바 중심 좌표(y)  

#define Y_GUIDE_BAR_CENTER_X_VAL CIRCLE_CENTER_X + BIG_CIRCLE_RADIUS + 10 +((GUIDE_BAR_THICKNESS) / 2) // Y축 가이드 바 중심 좌표(x) 
#define Y_GUIDE_BAR_CENTER_Y_VAL CIRCLE_CENTER_Y                                                       // X축 가이드 바 중심 좌표(y) 

#define BATT_ICON_X          4      // 좌측 상단 X
#define BATT_ICON_Y          4      // 좌측 상단 Y
#define BATT_ICON_W          36     // 본체 가로 (외곽 포함)
#define BATT_ICON_H          18     // 본체 세로 (외곽 포함)
#define BATT_ICON_BORDER     2      // 외곽선 두께
#define BATT_ICON_RADIUS     3      // 모서리 라운드
#define BATT_CAP_W           4      // 양극 돌기 가로
#define BATT_CAP_H           8      // 양극 돌기 세로
#define BATT_BAR_PAD         3      // 내부 바와 외곽 사이 여백
#define BATT_TEXT_OFFSET_X   (BATT_ICON_X + BATT_ICON_W + BATT_CAP_W + 4)  // % 텍스트 X
#define BATT_TEXT_OFFSET_Y   (BATT_ICON_Y + 2)                            // % 텍스트 Y
 
#define BATT_COLOR_CRITICAL  20     // 이 이하면 빨간색
#define BATT_COLOR_WARNING   50     // 이 이하면 주황색

// color (RG565)
#define BACKGROUND_COLOR        TFT_WHITE
#define TEXT_COLOR              TFT_BLACK
#define OUT_LINE_COLOR_BLUE     0x092B
#define FILL_COLOR              0x2B55

#endif