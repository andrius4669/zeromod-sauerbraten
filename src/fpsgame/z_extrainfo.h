#ifndef Z_EXTRAINFO_H
#define Z_EXTRAINFO_H

#include "z_queue.h"
#include "z_geoipstate.h"

struct z_posrecord
{
    vec o;
    int tmillis;
    int gmillis;
};

struct z_posqueue: z_queue<z_posrecord>
{
    // total x and y values
    float tx, ty;
    int gametime, totaltime;

    z_posqueue(): z_queue(200), tx(0.0f), ty(0.0f), gametime(0), totaltime(0) {}

    void removeold(int millis)
    {
        while(length() && first().tmillis-millis<0)
        {
            z_posrecord &v = remove();
            if(length())
            {
                z_posrecord &n = first();
                tx -= fabs(n.o.x-v.o.x);
                ty -= fabs(n.o.y-v.o.y);
                gametime = last().gmillis - n.gmillis;
                totaltime = last().tmillis - n.tmillis;
            }
            else
            {
                tx = ty = 0.0f;
                gametime = 0;
                totaltime = 0;
            }
        }
    }

    bool addpos(const vec &o, int tmillis, int gmillis)
    {
        if(!full())
        {
            if(length())
            {
                z_posrecord &l = last();
                tx += fabs(o.x-l.o.x);
                ty += fabs(o.y-l.o.y);
                gametime = gmillis - first().gmillis;
                totaltime = tmillis - first().tmillis;
            }
            else
            {
                tx = ty = 0.0f;
                gametime = 0;
                totaltime = 0;
            }
            z_posrecord &n = add();
            n.o = o;
            n.tmillis = tmillis;
            n.gmillis = gmillis;
            return true;
        }
        return false;
    }
};

struct z_extrainfo
{
    geoipstate geoip;
    z_queue<int> af_text, af_rename, af_team;
    char *disc_reason;
    int nodamage;
    bool slay, invpriv;
    char *wlauth;
    bool chatmute, specmute, editmute, namemute;
    int lastchat, lastedit;
    int maploaded;
    int mapsucks;
    //z_posqueue postrack;

    z_extrainfo(): disc_reason(NULL), wlauth(NULL) {}
    ~z_extrainfo() { delete[] disc_reason; delete[] wlauth; }

    void mapchange()
    {
        maploaded = 0;
        mapsucks = 0;
    }

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
        chatmute = specmute = editmute = namemute = false;
        lastchat = lastedit = 0;
    }

    void setdiscreason(const char *s) { delete[] disc_reason; disc_reason = s && *s ? newstring(s) : NULL; }

    void setwlauth(const char *s) { delete[] wlauth; wlauth = s && *s ? newstring(s) : NULL; }
    void clearwlauth() { DELETEA(wlauth); }
};

#endif // Z_EXTRAINFO_H
