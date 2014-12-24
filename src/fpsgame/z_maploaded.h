#ifndef Z_MAPLOADED_H
#define Z_MAPLOADED_H

#include "z_patchmap.h"

VAR(mapload_debug, 0, 0, 1);

static void z_maploaded(clientinfo *ci, bool val = true)
{
    if(val && !ci->maploaded)
    {
        ci->maploaded = totalmillis ? totalmillis : 1;
        if(mapload_debug) sendservmsgf("%s has loaded map", colorname(ci));
        z_sendallpatches(ci);
    }
    else if(!val) ci->maploaded = 0;
}

#endif // Z_MAPLOADED_H
