#include "ui_draw.h"
#include "font.h"
#include <math.h>
#include <string.h>
#include "lib/TFT_eSPI/TFT_eSPI.h"

/**
 * @brief 모든 UI 요소는 단일 프레임버퍼(Canvas)에서 그려진 뒤 한 번에 LCD로 전송
 */

typedef struct 
{
  TFT_eSPI* display;
  TFT_eSprite* frambuffer;
} canvas_t;

static canvas_t canvas = {nullptr, nullptr};

// 배터리 충전 잔량
static int8_t   batt_percent;
// 배터리 충전 여부
static bool     batt_charging = false;

static int      uiGetFontIndex(char c);
static void     uiDrawCharScaled(TFT_eSprite* target, char c, int x, int y, float scale);
static void     uiDrawTextScaled(TFT_eSprite* target, const char* str, int x, int y, float scale);
static float    uiClampf(float value, float min_val, float max_val);
static void     uiDrawHud(int point_x, int point_y);
static void     uiDrawXBar(int offset_x);
static void     uiDrawYBar(int offset_y);
static void     uiDrawSensorValuesOnCanvas(float X, float Y);
static uint16_t uiGetBatteryColor(float percent);
static void     uiDrawBattery(float percent, bool is_charging);
/**
 * @brief 프레임버퍼를 생성한다.
 */
static void uiFrameBufferInit(void)
{
  if (canvas.display == nullptr)
  {
    return;
  }

  // 단색 조합으로 UI를 구성할 예정이므로, 8비트로 설정한다.
  canvas.frambuffer->setColorDepth(16);

  // 해상도에 맞는 크기의 프레임 버퍼를 생성한다.
  if (canvas.frambuffer->createSprite(UI_SCREEN_W, UI_SCREEN_H) == nullptr)
  {
    Serial.println("FrameBuffer alloc failed!");
    while (1);
  }
}

void uiInit(TFT_eSPI* disp) 
{
  if (disp == nullptr) 
  {
    return;
  }

  // main에서 생성한 display 객체를 받는다.
  canvas.display = disp;

  // 위 객체를 이용하여 ram에 그림을 그릴 프레임버퍼 객체를 생성한다.
  static TFT_eSprite framebuffer = TFT_eSprite(disp);
  
  // 전역 변수에 연결해준다.
  canvas.frambuffer = &framebuffer;

  // 화면을 가로모드로 돌린다.
  canvas.frambuffer->setRotation(1);

  // LCD 기본 배경색 설정
  canvas.display->fillScreen(BACKGROUND_COLOR);

  // 스프라이트 초기화 (반드시 uiDraw 호출 전에 완료되어야 함)
  uiFrameBufferInit();

  // 최초 렌더링
  uiDraw(0.0f, 0.0f);
}

void uiDraw(float sensor_x, float sensor_y) 
{
    if (canvas.frambuffer == nullptr)
    {
      return;
    }

  // 1. 캔버스 초기화
  canvas.frambuffer->fillSprite(BACKGROUND_COLOR);

  // 2. 고정 텍스트 레이블 그리기
  canvas.frambuffer->setTextColor(TEXT_COLOR, BACKGROUND_COLOR);
  canvas.frambuffer->setTextSize(2);
  canvas.frambuffer->setCursor(STRING_X_VALUE_START_X, STRING_X_VALUE_START_Y);
  canvas.frambuffer->print("X AXIS:");
  canvas.frambuffer->setCursor(STRING_Y_VALUE_START_X, STRING_Y_VALUE_START_Y);
  canvas.frambuffer->print("Y AXIS:");

  // 축 이름 데코레이션
  canvas.frambuffer->setFreeFont(&FreeSans12pt7b);
  canvas.frambuffer->setTextSize(1);
  canvas.frambuffer->setCursor(CIRCLE_CENTER_X - (BIG_CIRCLE_RADIUS + 20), CIRCLE_CENTER_Y + 10);
  canvas.frambuffer->print("Y");
  canvas.frambuffer->setCursor(CIRCLE_CENTER_X - 5, CIRCLE_CENTER_Y - (BIG_CIRCLE_RADIUS + 5));
  canvas.frambuffer->print("X");
  canvas.frambuffer->setFreeFont(NULL);

  // 3. 센서 데이터 기반 좌표 매핑 로직
  float target_x = sensor_y;
  float target_y = -sensor_x;
  float hud_vector_len = sqrtf(target_x * target_x + target_y * target_y);

  int draw_x = CIRCLE_CENTER_X;
  int draw_y = CIRCLE_CENTER_Y;

  if (hud_vector_len > 0.0001f) // 센서에서 데이터가 들어온다면
  {
    float dir_x = target_x / hud_vector_len; // 각 방향 벡터를 구한다.
    float dir_y = target_y / hud_vector_len;
    float ui_vector_length = uiClampf(hud_vector_len / 90.0f, 0.0f, 1.0f); // 0 ~ 1.0사이로 매핑한다.

    const float threshold = MIN_MARGIN_OF_ANGLE; // middle_circle에 진입할 조건
    const float middle_touch = (float)MIDDLE_CIRCLE_RADIUS + (float)SMALL_CIRCLE_RADIUS; // 작은 원의 반지름을 고려하여 중간원에 접한 위치를 계산
    const float max_distance = (float)BIG_CIRCLE_RADIUS - (float)SMALL_CIRCLE_RADIUS; // 최대 도달거리도 계산 

    float draw_distance; // 그릴 위치

    if (ui_vector_length <= threshold) // middle_circle에 들어갈 조건
    {
      draw_distance = (ui_vector_length / threshold) * middle_touch; // 매핑된 값을 기준으로 middle_circle내부 그릴 위치를 계산
    } 
    else 
    {
      float outer_ratio = (ui_vector_length - threshold) / (1.0f - threshold); // 매핑값을 기준으로 middle <-> big_circle사이 위치값 계산
      draw_distance = middle_touch + uiClampf(outer_ratio, 0.0f, 1.0f) * (max_distance - middle_touch); // 그릴 위치를 계산 
    }

    draw_x = CIRCLE_CENTER_X + (int)(dir_x * draw_distance); // 절대 좌표값과 방향 정보를 주어 small_circle의 중심을 각 좌표별로 정해준다. 
    draw_y = CIRCLE_CENTER_Y + (int)(dir_y * draw_distance);
  }

  // 4. 위 정보를 토대로 그린다.
  uiDrawHud(draw_x, draw_y);

  // guide bar 내 원의 중심은 offset값만 주어지면 그릴 수 있다.
  uiDrawXBar(draw_x - CIRCLE_CENTER_X);

  uiDrawYBar(draw_y - CIRCLE_CENTER_Y);

  uiDrawSensorValuesOnCanvas(sensor_x, sensor_y);
  
  uiDrawBattery(batt_percent, batt_charging);

  // 5. 최종 그림을 한번에 spi 전송한다.
  canvas.frambuffer->pushSprite(0, 0);
}

void uiSetBattPercent(int8_t percent, bool is_charging)
{
  batt_percent = percent;
  batt_charging = is_charging;
}

/**
 * @brief 센서에서 받은 값을 문자열로써 화면에 그리는 함수
 */
static void uiDrawSensorValuesOnCanvas(float X, float Y) 
{
  char data_buf[12];

  // X 축 값
  sprintf(data_buf, "%.2f", fabs(X));
  canvas.frambuffer->setTextSize(2);
  canvas.frambuffer->setCursor(STRING_X_VALUE_START_X, STRING_X_VALUE_START_Y + 20);
  canvas.frambuffer->print(X >= 0 ? "+" : "-");
  uiDrawTextScaled(canvas.frambuffer, data_buf, STRING_X_VALUE_START_X + 15, STRING_X_VALUE_START_Y + 20, 2.5f);

  // Y 축 값
  sprintf(data_buf, "%.2f", fabs(Y));
  canvas.frambuffer->setCursor(STRING_Y_VALUE_START_X, STRING_Y_VALUE_START_Y + 20);
  canvas.frambuffer->print(Y >= 0 ? "+" : "-");
  uiDrawTextScaled(canvas.frambuffer, data_buf, STRING_Y_VALUE_START_X + 15, STRING_Y_VALUE_START_Y + 20, 2.5f);
}

static void uiDrawHud(int point_x, int point_y) 
{
  canvas.frambuffer->fillCircle(CIRCLE_CENTER_X, CIRCLE_CENTER_Y, BIG_CIRCLE_RADIUS, OUT_LINE_COLOR_BLUE);
  canvas.frambuffer->fillCircle(CIRCLE_CENTER_X, CIRCLE_CENTER_Y, BIG_CIRCLE_RADIUS - 2, FILL_COLOR);

  // 중간 원 그리기
  canvas.frambuffer->fillCircle(CIRCLE_CENTER_X, CIRCLE_CENTER_Y, MIDDLE_CIRCLE_RADIUS, TEXT_COLOR);
  canvas.frambuffer->fillCircle(CIRCLE_CENTER_X, CIRCLE_CENTER_Y, MIDDLE_CIRCLE_RADIUS - 2, FILL_COLOR);

  // 작은 원 그리기
  canvas.frambuffer->fillCircle(point_x, point_y, SMALL_CIRCLE_RADIUS, OUT_LINE_COLOR_BLUE);
  canvas.frambuffer->fillCircle(point_x, point_y, SMALL_CIRCLE_RADIUS - 2, BACKGROUND_COLOR);

  // MIDDLE 테두리 복원 (두께 2px 예시)
for (int i = 0; i < 2; i++)
{
  canvas.frambuffer->drawCircle
  (
    CIRCLE_CENTER_X,
    CIRCLE_CENTER_Y,
    MIDDLE_CIRCLE_RADIUS - i,
    TEXT_COLOR
  );
}
  // 중간 십자선 그리기
  uiDrawDashedLine(canvas.frambuffer, CIRCLE_CENTER_X - CROSS_LENGTH, CIRCLE_CENTER_Y, CIRCLE_CENTER_X + CROSS_LENGTH, CIRCLE_CENTER_Y, 6, 4, TEXT_COLOR);
  uiDrawDashedLine(canvas.frambuffer, CIRCLE_CENTER_X, CIRCLE_CENTER_Y - CROSS_LENGTH, CIRCLE_CENTER_X, CIRCLE_CENTER_Y + CROSS_LENGTH, 6, 4, TEXT_COLOR);
}

static void uiDrawXBar(int offset_x) 
{
  uiDrawRoundBox(canvas.frambuffer, X_GUIDE_BAR_X, X_GUIDE_BAR_Y, X_GUIDE_BAR_W, X_GUIDE_BAR_H);
  int point_x = X_GUIDE_BAR_CENTER_X_VAL + offset_x;
  int point_y = X_GUIDE_BAR_CENTER_Y_VAL;

  // 타원형 포인터
  int rx = GUIDE_CIRCLE_RADIUS;
  int ry = ((GUIDE_BAR_THICKNESS - 2) / 2) - 1;
  
  canvas.frambuffer->fillEllipse(point_x, point_y, rx, ry, OUT_LINE_COLOR_BLUE);
  canvas.frambuffer->fillEllipse(point_x, point_y, rx - 2, ry - 2, BACKGROUND_COLOR);

  // 가이드 라인 그리기
  int inner = GUIDE_CIRCLE_RADIUS;
  int outer = MIDDLE_CIRCLE_RADIUS;
  int mid   = (inner + outer) / 2;

  int inner_sections[] = { -inner, inner};
  int outer_sections[] = { -outer, outer, -mid, mid };

  int inner_half = (GUIDE_BAR_THICKNESS - 6) / 2;  // 내곽은 길게 만든다.
  int outer_half = (GUIDE_BAR_THICKNESS - 14) / 2; // 외곽은 짧게 만든다.

  // 내곽 가이드라인(작은원에 딱맞는 사이즈로)
  for (int i = 0; i < 2; i++) 
  {
    int x = X_GUIDE_BAR_CENTER_X_VAL + inner_sections[i];
    int y1 = X_GUIDE_BAR_CENTER_Y_VAL - inner_half;
    int y2 = X_GUIDE_BAR_CENTER_Y_VAL + inner_half;

    canvas.frambuffer->drawLine(x, y1, x, y2, TEXT_COLOR);
    canvas.frambuffer->drawLine(x + 1, y1, x + 1, y2, TEXT_COLOR);
  }

  // 외곽 가이드라인과, 내 외각 사이 센터 지점 수선 하나를 긋는다.
  for (int i = 0; i < 4; i++) 
  {
    int x = X_GUIDE_BAR_CENTER_X_VAL + outer_sections[i];
    int y1 = X_GUIDE_BAR_CENTER_Y_VAL - outer_half;
    int y2 = X_GUIDE_BAR_CENTER_Y_VAL + outer_half;

    canvas.frambuffer->drawLine(x, y1, x, y2, TEXT_COLOR);
    canvas.frambuffer->drawLine(x + 1, y1, x + 1, y2, TEXT_COLOR);
  }

  // inner 구간을 6등분 → 지점 0~6
  for (int i = 1; i <= 5; i++)
  {
    if (!(i == 1 || i == 2 || i == 4 || i == 5)) // 그려야 할 지점이 아니면 건너뛴다.
    {
      continue; 
    }

    float temp = (float)i / 6.0f;

    int x1 = X_GUIDE_BAR_CENTER_X_VAL - inner
          + (int)(2 * inner * temp);
    int x2 = x1 + 1;

    int y1 = X_GUIDE_BAR_CENTER_Y_VAL - outer_half;
    int y2 = X_GUIDE_BAR_CENTER_Y_VAL + outer_half;

    // 수선을 긋는다.
    canvas.frambuffer->drawLine(x1, y1, x1, y2, TEXT_COLOR);
    canvas.frambuffer->drawLine(x2, y1, x2, y2, TEXT_COLOR);
  }
}

static void uiDrawYBar(int offset_y) 
{
  uiDrawRoundBox(canvas.frambuffer, Y_GUIDE_BAR_X, Y_GUIDE_BAR_Y, Y_GUIDE_BAR_W, Y_GUIDE_BAR_H);

  int point_x = Y_GUIDE_BAR_CENTER_X_VAL;
  int point_y = Y_GUIDE_BAR_CENTER_Y_VAL + offset_y;

  int rx = ((GUIDE_BAR_THICKNESS - 2) / 2) - 1;
  int ry = GUIDE_CIRCLE_RADIUS;

  canvas.frambuffer->fillEllipse(point_x, point_y, rx, ry, OUT_LINE_COLOR_BLUE);
  canvas.frambuffer->fillEllipse(point_x, point_y, rx - 2, ry - 2, BACKGROUND_COLOR);

  int inner = GUIDE_CIRCLE_RADIUS;
  int outer = MIDDLE_CIRCLE_RADIUS;
  int mid   = (inner + outer) / 2;

  int inner_sections[] = { -inner, inner };
  int outer_sections[] = { -outer, outer, -mid, mid };

  int inner_half = (GUIDE_BAR_THICKNESS - 6) / 2;
  int outer_half = (GUIDE_BAR_THICKNESS - 14) / 2;

  for (int i = 0; i < 2; i++)
  {
    int y = Y_GUIDE_BAR_CENTER_Y_VAL + inner_sections[i];
    int x1 = Y_GUIDE_BAR_CENTER_X_VAL - inner_half;
    int x2 = Y_GUIDE_BAR_CENTER_X_VAL + inner_half;

    canvas.frambuffer->drawLine(x1, y, x2, y, TEXT_COLOR);
    canvas.frambuffer->drawLine(x1, y + 1, x2, y + 1, TEXT_COLOR);
  }

  for (int i = 0; i < 4; i++)
  {
    int y = Y_GUIDE_BAR_CENTER_Y_VAL + outer_sections[i];
    int x1 = Y_GUIDE_BAR_CENTER_X_VAL - outer_half;
    int x2 = Y_GUIDE_BAR_CENTER_X_VAL + outer_half;

    canvas.frambuffer->drawLine(x1, y, x2, y, TEXT_COLOR);
    canvas.frambuffer->drawLine(x1, y + 1, x2, y + 1, TEXT_COLOR);
  }

  for (int i = 1; i <= 5; i++)
  {
    if (!(i == 1 || i == 2 || i == 4 || i == 5))
      continue;

    float t = (float)i / 6.0f;

    int y = Y_GUIDE_BAR_CENTER_Y_VAL - inner
          + (int)(2 * inner * t);

    int x1 = Y_GUIDE_BAR_CENTER_X_VAL - outer_half;
    int x2 = Y_GUIDE_BAR_CENTER_X_VAL + outer_half;

    canvas.frambuffer->drawLine(x1, y, x2, y, TEXT_COLOR);
    canvas.frambuffer->drawLine(x1, y + 1, x2, y + 1, TEXT_COLOR);
  }
}

void uiDrawRoundBox(TFT_eSPI* target, int x, int y, int w, int h) 
{
  const uint8_t r = GUIDE_BAR_EDGE_RADIUS;
  target->fillRoundRect(x, y, w, h, r, OUT_LINE_COLOR_BLUE);
  target->fillRoundRect(x + 2, y + 2, w - 4, h - 4, r - 2, FILL_COLOR);
}


void uiDrawDashedLine(TFT_eSPI* target, int start_x, int start_y, int end_x, int end_y, int dash_len, int gap_len, uint16_t color) 
{
  float vector_x = (float)(end_x - start_x);
  float vector_y = (float)(end_y - start_y);
  float vector_length = sqrtf(vector_x * vector_x + vector_y * vector_y);
  if (vector_length <= 0.0f) 
  {
    return;
  }

  float delta_x = vector_x / vector_length;
  float delta_y = vector_y / vector_length;

  float pos = 0.0f;

  bool is_time_to_draw = true;

  while (pos < vector_length) 
  {
    float line_length = is_time_to_draw ? (float)dash_len : (float)gap_len;
    if (pos + line_length > vector_length) 
    {
      line_length = vector_length - pos;
    }

    if (is_time_to_draw) 
    {
      target->drawLine((int)(start_x + delta_x * pos), (int)(start_y + delta_y * pos), (int)(start_x + delta_x * (pos + line_length)), (int)(start_y + delta_y * (pos + line_length)), color);
    }

    pos += line_length;
    is_time_to_draw = !is_time_to_draw;
  }
}

static uint16_t uiGetBatteryColor(float percent)
{
  if (percent <= BATT_COLOR_CRITICAL)  
  {
    return TFT_RED;
  }
  else if (percent <= BATT_COLOR_WARNING)
  {
    return TFT_ORANGE;
  }
  return TFT_GREEN;
}

static void uiDrawBattery(float percent, bool is_charging)
{
  if (canvas.frambuffer == nullptr)
  {
    return;
  }

  float pct = uiClampf(percent, 0.0f, 100.0f);
  TFT_eSprite* fb = canvas.frambuffer;

  // 1. 본체 외곽
  fb->fillRoundRect(BATT_ICON_X, BATT_ICON_Y, BATT_ICON_W, BATT_ICON_H,
                    BATT_ICON_RADIUS, TEXT_COLOR);
  fb->fillRoundRect(BATT_ICON_X + BATT_ICON_BORDER, BATT_ICON_Y + BATT_ICON_BORDER,
                    BATT_ICON_W - BATT_ICON_BORDER * 2, BATT_ICON_H - BATT_ICON_BORDER * 2,
                    BATT_ICON_RADIUS - 1, BACKGROUND_COLOR);

  // 2. 양극 돌기
  int cap_x = BATT_ICON_X + BATT_ICON_W;
  int cap_y = BATT_ICON_Y + (BATT_ICON_H - BATT_CAP_H) / 2;
  fb->fillRoundRect(cap_x, cap_y, BATT_CAP_W, BATT_CAP_H, 1, TEXT_COLOR);

  // 3. 내부 충전 바
  int bar_x     = BATT_ICON_X + BATT_ICON_BORDER + BATT_BAR_PAD;
  int bar_y     = BATT_ICON_Y + BATT_ICON_BORDER + BATT_BAR_PAD;
  int bar_max_w = BATT_ICON_W - (BATT_ICON_BORDER + BATT_BAR_PAD) * 2;
  int bar_h     = BATT_ICON_H - (BATT_ICON_BORDER + BATT_BAR_PAD) * 2;
  int bar_w     = (int)((float)bar_max_w * pct / 100.0f);

  uint16_t bar_color = uiGetBatteryColor(pct);

  if (bar_w > 0)
  {
    fb->fillRoundRect(bar_x, bar_y, bar_w, bar_h, 1, bar_color);
  }

  // 4. 충전 중이면 번개 아이콘
  if (is_charging)
  {
    // 배터리 아이콘 중심 기준
    int cx = BATT_ICON_X + BATT_ICON_W / 2;
    int ty = BATT_ICON_Y + BATT_ICON_BORDER + 1;          // top
    int by = BATT_ICON_Y + BATT_ICON_H - BATT_ICON_BORDER - 1; // bottom
    int my = (ty + by) / 2;  // middle

    uint16_t bolt_color = TEXT_COLOR;

    // 위 삼각: 상단 → 중간좌 → 중간우
    fb->fillTriangle(cx + 1, ty, cx - 3, my, cx + 1, my, bolt_color);
    // 아래 삼각: 중간좌 → 중간우 → 하단
    fb->fillTriangle(cx - 1, my, cx + 3, my, cx - 1, by, bolt_color);
  }

  // 5. 퍼센트 텍스트
if (!is_charging)
{
  char text_buf[8];
  sprintf(text_buf, "%d%%", (int)pct);

  fb->setFreeFont(NULL);
  fb->setTextSize(1);
  fb->setTextColor(TEXT_COLOR, BACKGROUND_COLOR);
  fb->setCursor(BATT_TEXT_OFFSET_X, BATT_TEXT_OFFSET_Y + 4);
  fb->print(text_buf);
}
}


static int uiGetFontIndex(char c) 
{
  if (c == '+') return 0;
  if (c == '-') return 1;
  if (c == '.') return 2;
  if (c == ',') return 3;
  if (c >= '0' && c <= '9') return 4 + (c - '0');
  if (c == 'X') return 14;
  if (c == 'Y') return 15;
  return -1;
}

static void uiDrawCharScaled(TFT_eSprite* target, char c, int x, int y, float scale) 
{
  int idx = uiGetFontIndex(c);

  if (idx < 0) 
  {
    return; 
  }

  const uint8_t* glyph = font_num5x7[idx];
  for (int col = 0; col < 5; col++) 
  {
    uint8_t bits = pgm_read_byte(&glyph[col]);
    for (int row = 0; row < 7; row++) 
    {
      if (bits & (1 << row)) 
      {
        target->fillRect(x + (int)(col * scale), y + (int)(row * scale), (int)(scale + 0.5f), (int)(scale + 0.5f), TEXT_COLOR);
      }
    }
  }
}

static void uiDrawTextScaled(TFT_eSprite* target, const char* str, int x, int y, float scale) 
{
  int cx = x;
  while (*str) 
  {
    uiDrawCharScaled(target, *str, cx, y, scale);
    cx += (int)(6 * scale);
    str++;
  }
}

static float uiClampf(float value, float min_val, float max_val) 
{
  if (value < min_val) return min_val;
  if (value > max_val) return max_val;
  return value;
}