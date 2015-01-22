#ifndef Z_EXTRAINFO_H
#define Z_EXTRAINFO_H

#include "z_queue.h"
#include "z_geoipstate.h"

struct z_extrainfo
{
    geoipstate geoip;
    z_queue<int> af_text, af_rename, af_team;
    char *disc_reason;
    int nodamage;
    bool slay, invpriv;

    z_extrainfo(): disc_reason(NULL) {}
    ~z_extrainfo() { DELETEA(disc_reason); }

    void reset()
    {
        geoip.cleanup();
        af_text.resize(0);
        af_rename.resize(0);
        af_team.resize(0);
        DELETEA(disc_reason);
        nodamage = 0;
        slay = false;
        invpriv = false;
    }
};

#endif // Z_EXTRAINFO_H
