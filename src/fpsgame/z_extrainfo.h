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
    char *wlauth;

    z_extrainfo(): disc_reason(NULL), wlauth(NULL) {}
    ~z_extrainfo() { delete[] disc_reason; delete[] wlauth; }

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

    void setdiscreason(const char *s) { delete[] disc_reason; disc_reason = s && *s ? newstring(s) : NULL; }

    void setwlauth(const char *s) { delete[] wlauth; wlauth = s && *s ? newstring(s) : NULL; }
    void clearwlauth() { DELETEA(wlauth); }
};

#endif // Z_EXTRAINFO_H
