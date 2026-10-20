#pragma once
#include "save_game.h"
enum {
    SAVE_PLACE_UNKNOWN, SAVE_PLACE_OPENING, SAVE_PLACE_RECEPTION,
    SAVE_PLACE_HALL, SAVE_PLACE_CHIEF, SAVE_PLACE_ENGINEERING,
    SAVE_PLACE_DETECTIVE, SAVE_PLACE_COMPUTER, SAVE_PLACE_RANGE,
    SAVE_PLACE_VEHICLE, SAVE_PLACE_FACTORY_FRONT, SAVE_PLACE_FACTORY,
    SAVE_PLACE_COUNT
};
typedef struct { unsigned act,place; } SaveSummary;
/* Inspect a validated checkpoint without importing or changing live VM state. */
SaveSummary save_summary(const GameCheckpoint *checkpoint);
