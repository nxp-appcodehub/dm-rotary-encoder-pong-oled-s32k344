/*
 * Copyright 2024-2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pong.h"
#include "ssd1306.h"
#include <stdio.h>

/* ---- Geometry (display is SCREEN_WIDTH x SCREEN_HEIGHT = 96 x 39 px) ---- */
#define PADDLE_W        (2)
#define PADDLE_H        (10)
#define BALL_SIZE       (2)
#define LEFT_X          (2)
#define RIGHT_X         (SCREEN_WIDTH - 2 - PADDLE_W)
#define CLICK_PX        (3)  /* paddle movement per encoder click */
#define OPP_SPEED_PX    (1)  /* computer paddle speed per tick */
#define PADDLE_DIR      (-1) /* set to 1 or -1 to invert the encoder direction */

/* ---- Ball motion, fixed point with 4 fractional bits (1/16 px) ---------- */
#define FP_SHIFT        (4)
#define VX_START        (24) /* 1.5 px per tick */
#define VX_MAX          (44)
#define VX_STEP         (2)  /* speed increase per paddle hit */
#define VY_PER_PX       (4)  /* vertical speed per px away from paddle center */

static int32_t s_ballX;     /* 1/16 px, top-left of the ball */
static int32_t s_ballY;
static int32_t s_vx;        /* 1/16 px per tick */
static int32_t s_vy;
static int32_t s_paddleY;   /* px, top edge of the player paddle */
static int32_t s_oppY;      /* px, top edge of the computer paddle */
static uint32_t s_scorePlayer;
static uint32_t s_scoreOpp;
static uint32_t s_serveCount;
static pong_state_t s_state = PONG_STATE_SERVE;
static uint32_t s_speed = 1U; /* game steps per frame (1..PONG_SPEED_MAX) */

static int32_t Clamp(int32_t v, int32_t lo, int32_t hi) {
	if (v < lo) {
		return lo;
	}
	if (v > hi) {
		return hi;
	}
	return v;
}

/* Puts the ball on the player paddle, waiting for the serve. */
static void AttachBall(void) {
	s_ballX = (int32_t) (LEFT_X + PADDLE_W) << FP_SHIFT;
	s_ballY = (s_paddleY + (PADDLE_H / 2) - (BALL_SIZE / 2)) << FP_SHIFT;
	s_vx = 0;
	s_vy = 0;
	s_state = PONG_STATE_SERVE;
}

void Pong_Init(void) {
	s_paddleY = (SCREEN_HEIGHT - PADDLE_H) / 2;
	s_oppY = (SCREEN_HEIGHT - PADDLE_H) / 2;
	s_scorePlayer = 0U;
	s_scoreOpp = 0U;
	s_serveCount = 0U;
	s_speed = 1U;
	AttachBall();
}

void Pong_MovePaddle(int32_t clicks) {
	s_paddleY = Clamp(s_paddleY + (clicks * CLICK_PX * PADDLE_DIR), 0,
			SCREEN_HEIGHT - PADDLE_H);
	if (s_state == PONG_STATE_SERVE) {
		AttachBall(); /* the ball follows the paddle until it is served */
	}
}

void Pong_Serve(void) {
	if ((s_state == PONG_STATE_WIN) || (s_state == PONG_STATE_LOSE)) {
		Pong_Init();
		return;
	}
	if (s_state == PONG_STATE_SERVE) {
		/* Vertical speed alternates: -0.5, 0, +0.5 px per tick */
		s_vx = VX_START;
		s_vy = ((int32_t) (s_serveCount % 3U) - 1) * 8;
		s_serveCount++;
		s_state = PONG_STATE_PLAYING;
	}
}

static void PointScored(bool player) {
	if (player) {
		s_scorePlayer++;
	} else {
		s_scoreOpp++;
	}
	if (s_scorePlayer >= PONG_WIN_SCORE) {
		s_state = PONG_STATE_WIN;
	} else if (s_scoreOpp >= PONG_WIN_SCORE) {
		s_state = PONG_STATE_LOSE;
	} else {
		AttachBall();
	}
}

void Pong_SpeedUp(void) {
	if (s_speed < PONG_SPEED_MAX) {
		s_speed++;
	}
}

static void Pong_StepOnce(void);

void Pong_Step(void) {
	uint32_t i;

	for (i = 0U; i < s_speed; i++) {
		Pong_StepOnce();
	}
}

static void Pong_StepOnce(void) {
	int32_t bx;
	int32_t by;
	int32_t cy;
	int32_t target;

	if (s_state != PONG_STATE_PLAYING) {
		return;
	}

	s_ballX += s_vx;
	s_ballY += s_vy;

	/* Top and bottom walls */
	if (s_ballY < 0) {
		s_ballY = -s_ballY;
		s_vy = -s_vy;
	} else if (s_ballY > ((int32_t) (SCREEN_HEIGHT - BALL_SIZE) << FP_SHIFT)) {
		s_ballY = (2 * ((int32_t) (SCREEN_HEIGHT - BALL_SIZE) << FP_SHIFT))
				- s_ballY;
		s_vy = -s_vy;
	}

	bx = s_ballX >> FP_SHIFT;
	by = s_ballY >> FP_SHIFT;
	cy = by + (BALL_SIZE / 2); /* vertical center of the ball */

	/* Player paddle (left) */
	if ((s_vx < 0) && (bx <= (LEFT_X + PADDLE_W))
			&& (bx >= (LEFT_X - BALL_SIZE))) {
		if ((cy >= (s_paddleY - 1)) && (cy <= (s_paddleY + PADDLE_H))) {
			s_ballX = (int32_t) (LEFT_X + PADDLE_W) << FP_SHIFT;
			s_vx = -s_vx;
			if (s_vx < VX_MAX) {
				s_vx += VX_STEP;
			}
			s_vy = (cy - (s_paddleY + (PADDLE_H / 2))) * VY_PER_PX;
		}
	}

	/* Computer paddle (right) */
	if ((s_vx > 0) && ((bx + BALL_SIZE) >= RIGHT_X)
			&& (bx <= (RIGHT_X + PADDLE_W))) {
		if ((cy >= (s_oppY - 1)) && (cy <= (s_oppY + PADDLE_H))) {
			s_ballX = (int32_t) (RIGHT_X - BALL_SIZE) << FP_SHIFT;
			s_vx = -s_vx;
			s_vy = (cy - (s_oppY + (PADDLE_H / 2))) * VY_PER_PX;
		}
	}

	/* Scoring: the ball left the screen */
	if ((s_ballX >> FP_SHIFT) < 0) {
		PointScored(false);
	} else if ((s_ballX >> FP_SHIFT) > SCREEN_WIDTH) {
		PointScored(true);
	}

	/* Computer paddle: follows the ball when it comes towards it, otherwise
	 * returns to the center. Limited speed, so the player can win. */
	if (s_vx > 0) {
		target = cy - (PADDLE_H / 2);
	} else {
		target = (SCREEN_HEIGHT - PADDLE_H) / 2;
	}
	if (target > (s_oppY + 1)) {
		s_oppY += OPP_SPEED_PX;
	} else if (target < (s_oppY - 1)) {
		s_oppY -= OPP_SPEED_PX;
	}
	s_oppY = Clamp(s_oppY, 0, SCREEN_HEIGHT - PADDLE_H);
}

void Pong_Render(void) {
	char text[16];
	int y;

	SSD1306_ClearBuffer();

	/* Dotted center line */
	for (y = 0; y < SCREEN_HEIGHT; y += 4) {
		SSD1306_DrawPixel(SCREEN_WIDTH / 2, y, true);
	}

	/* Score: one digit on each side of the dotted center line (5 px glyphs,
	 * 3 px gap to the line) */
	snprintf(text, sizeof(text), "%lu", (unsigned long) s_scorePlayer);
	SSD1306_DrawString((SCREEN_WIDTH / 2) - 3 - 5, 0, text, 1, true);
	snprintf(text, sizeof(text), "%lu", (unsigned long) s_scoreOpp);
	SSD1306_DrawString((SCREEN_WIDTH / 2) + 4, 0, text, 1, true);

	/* Paddles and ball */
	SSD1306_FillRect(LEFT_X, (int) s_paddleY, PADDLE_W, PADDLE_H, true);
	SSD1306_FillRect(RIGHT_X, (int) s_oppY, PADDLE_W, PADDLE_H, true);
	SSD1306_FillRect((int) (s_ballX >> FP_SHIFT), (int) (s_ballY >> FP_SHIFT),
			BALL_SIZE, BALL_SIZE, true);

	if (s_state == PONG_STATE_SERVE) {
		SSD1306_DrawString(16, 30, "PRESS KNOB", 1, true);
	} else if (s_state == PONG_STATE_WIN) {
		SSD1306_FillRect(20, 14, 56, 11, false);
		SSD1306_DrawString(24, 16, "YOU WIN", 1, true);
	} else if (s_state == PONG_STATE_LOSE) {
		SSD1306_FillRect(16, 14, 64, 11, false);
		SSD1306_DrawString(18, 16, "YOU LOSE", 1, true);
	}
}

pong_state_t Pong_GetState(void) {
	return s_state;
}

uint8_t Pong_GetLedPos(void) {
	/* The ball runs over the top of the ring in both directions:
	 * LED 18 -> 17 -> ... -> 11 -> 10 -> ... -> 6 and back in reverse. */
	static const uint8_t path[] = { 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6 };
	int32_t cx = (s_ballX >> FP_SHIFT) + (BALL_SIZE / 2);
	int32_t n = (int32_t) (sizeof(path) / sizeof(path[0]));
	int32_t idx = Clamp((cx * n) / SCREEN_WIDTH, 0, n - 1);

	return path[idx];
}
