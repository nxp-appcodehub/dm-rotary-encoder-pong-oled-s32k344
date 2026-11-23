/*
 * Copyright 2024-2026 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PONG_H
#define PONG_H

#include <stdint.h>
#include <stdbool.h>

/** @brief  Points needed to win a game. */
#define PONG_WIN_SCORE   (3U)

/** @brief  Maximum number of game steps per frame (speed level). */
#define PONG_SPEED_MAX   (5U)

/** @brief  Game states. */
typedef enum {
	PONG_STATE_SERVE = 0, /**< ball sits on the player paddle, waiting for a serve */
	PONG_STATE_PLAYING,   /**< ball in play */
	PONG_STATE_WIN,       /**< player reached PONG_WIN_SCORE */
	PONG_STATE_LOSE       /**< computer reached PONG_WIN_SCORE */
} pong_state_t;

/**
 * @brief   Starts a new game (scores 0, paddles centered, waiting for serve).
 */
void Pong_Init(void);

/**
 * @brief   Moves the player paddle (left side).
 * @param[in] clicks  Encoder clicks; positive moves the paddle down.
 */
void Pong_MovePaddle(int32_t clicks);

/**
 * @brief   Serves the ball, or starts a new game after a win / loss.
 */
void Pong_Serve(void);

/**
 * @brief   Advances the game by one tick (ball, computer paddle, scoring).
 */
void Pong_Step(void);

/**
 * @brief   Increases the game speed by one level (reset by Pong_Init).
 */
void Pong_SpeedUp(void);

/**
 * @brief   Draws the game into the SSD1306 frame buffer (does not flush it).
 */
void Pong_Render(void);

/**
 * @brief   Returns the current game state.
 */
pong_state_t Pong_GetState(void);

/**
 * @brief   Returns the LED index (0..23) that follows the ball. The ball goes
 *          LED18 -> 23 -> 0 -> LED5 (over the top) and back the same way in
 *          reverse (LED5 -> 0 -> 23 -> LED18).
 */
uint8_t Pong_GetLedPos(void);

#endif /* PONG_H */
