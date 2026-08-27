#pragma once

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#define RC522_UID_MAX_SIZE 10U

typedef struct
{
    uint8_t bytes[RC522_UID_MAX_SIZE];
    uint8_t size;
    uint8_t sak;
} rc522Uid_t;

bool rc522Init(void);
bool rc522ReadUid(rc522Uid_t *uid);
uint8_t rc522GetVersion(void);

