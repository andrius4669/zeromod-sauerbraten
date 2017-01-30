#ifdef Z_MAPLOADED_H
#error "already z_maploaded.h"
#endif
#define Z_MAPLOADED_H

#ifndef Z_PATCHMAP_H
#error "want z_patchmap.h"
#endif

VAR(mapload_debug, 0, 0, 1);

static void z_maploaded(clientinfo *ci, bool val = true)
{
    if(val && !ci->xi.maploaded)
    {
        ci->xi.maploaded = totalmillis ? totalmillis : 1;
        if(mapload_debug) sendservmsgf("%s has loaded map", colorname(ci));
        z_sendallpatches(ci, s_patchreliable >= 0);
    }
    else if(!val) ci->xi.maploaded = 0;

    if(val) race_maploaded(ci);
}
