/* Windows stub for i_net: single-player only; no network. */
#ifdef _WIN32

#include <stdlib.h>
#include <string.h>
#include "doomdef.h"
#include "doomstat.h"
#include "d_net.h"
#include "m_argv.h"
#include "i_net.h"

extern doomcom_t *doomcom;
extern boolean netgame;

void I_InitNetwork(void)
{
    doomcom = (doomcom_t *)malloc(sizeof(doomcom_t));
    memset(doomcom, 0, sizeof(doomcom_t));
    netgame = false;
    doomcom->id = DOOMCOM_ID;
    doomcom->numplayers = doomcom->numnodes = 1;
    doomcom->deathmatch = 0;
    doomcom->consoleplayer = 0;
    doomcom->ticdup = 1;
    doomcom->extratics = 0;
}

void I_NetCmd(void)
{
    (void)doomcom;
    /* Single player: no send/get. */
}

#endif
