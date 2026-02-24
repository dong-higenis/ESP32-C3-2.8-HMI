#ifndef UI_DRAW_H
#define UI_DRAW_H

#include <stdint.h>
#include <stdbool.h>
#include <TFT_eSPI.h>

#include "ui_def.h"

void uiInit(TFT_eSPI* disp);
void uiDraw(float sensor_x, float sensor_y);

// 둥근 모서리 사각박스 그리기
void uiDrawRoundBox(TFT_eSPI* target, int x, int y, int w, int h);

// 점선 그리기
void uiDrawDashedLine
(
    TFT_eSPI* target,
    int start_x,
    int start_y,
    int end_x,
    int end_y,
    int dash_len,
    int gap_len,
    uint16_t color
);


#endif