#include "apMain.h"
#include "myGame.h"

void apInit(void)
{
    gameInit();
}

void apMain(void)
{
    while (1)
    {
        gameUpdate();
    }
}
