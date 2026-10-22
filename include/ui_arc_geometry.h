/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EVIST_UI_ARC_GEOMETRY_H
#define EVIST_UI_ARC_GEOMETRY_H

#define ARC_DIGIT_X 72u
#define ARC_DIGIT_W 47u
#define ARC_DOT_W 11u
#define ARC_TENTH_W 30u
#define ARC_SPEED_BASE 119
#define ARC_CELL_TOP 65u
#define ARC_CELL_BOTTOM 121u
#define ARC_UNIT_RIGHT (ARC_DIGIT_X+2u*ARC_DIGIT_W+ARC_DOT_W+ARC_TENTH_W)
#define ARC_UNIT_BASE 60

/* Outer speed frame and its inward stroke. */
#define ARC_FRAME_LEFT 60u
#define ARC_FRAME_RIGHT 218u
#define ARC_FRAME_TIP 230u
#define ARC_FRAME_TOP 30u
#define ARC_FRAME_MID 81u
#define ARC_FRAME_BOTTOM 132u
#define ARC_FRAME_W 3u
#define ARC_FRAME_LEGAL_COL 0xBDF7u
#define ARC_FRAME_OFFROAD_COL 0xFD20u

#endif
