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
    int lastpos;
    bool exceededrate;

    z_posqueue(): tx(0.0f), ty(0.0f), gametime(0), totaltime(0), lastpos(0), exceededrate(false) {}

    void removefirst()
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
            gametime = totaltime = 0;
        }
    }

    void removeold(int millis)
    {
        while(length() && first().tmillis-millis<0) removefirst();
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
                gametime = totaltime = 0;
            }
            z_posrecord &n = add();
            n.o = o;
            n.tmillis = tmillis;
            n.gmillis = gmillis;
            lastpos = tmillis;
            return true;
        }
        return false;
    }

    void reset(int s = 0)
    {
        resize(s);
        tx = ty = 0.0f;
        gametime = totaltime = 0;
        lastpos = 0;
    }
};

struct z_identinfo
{
    char *name, *desc;

    z_identinfo(): name(NULL), desc(NULL) {}
    ~z_identinfo() { clear(); }

    void clear() { DELETEA(name); desc = NULL; }

    void set(const char *n, const char *d)
    {
        size_t nl = strlen(n), dl = d ? strlen(d) : 0;
        clear();
        name = new char[nl + dl + 2];
        memcpy(name, n, nl+1);
        desc = &name[nl+1];
        if(dl) memcpy(desc, d, dl+1);
        else *desc = 0;
    }

    bool isset() const { return name!=NULL; }
};

struct z_extrainfo
{
    geoipstate geoip;
    z_queue<int> af_text, af_rename, af_team;
    char *disc_reason;
    int nodamage;
    bool slay, invpriv;
    char *wlauth;
    bool specmute, editmute, namemute;
    int chatmute;
    int lastchat, lastedit;
    int maploaded;
    int mapsucks;
    z_identinfo ident;
    z_identinfo claim;
    bool authident;
    z_posqueue postrack;
    bool ghost;

    z_extrainfo(): disc_reason(NULL), wlauth(NULL) {}
    ~z_extrainfo() { delete[] disc_reason; delete[] wlauth; }

    void mapchange()
    {
        maploaded = 0;
        mapsucks = 0;
        postrack.reset();
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
        specmute = editmute = namemute = false;
        chatmute = 0;
        lastchat = lastedit = 0;
        authident = false;
        ghost = false;
    }

    void setdiscreason(const char *s) { delete[] disc_reason; disc_reason = s && *s ? newstring(s) : NULL; }

    void setwlauth(const char *s) { delete[] wlauth; wlauth = s && *s ? newstring(s) : NULL; }
    void clearwlauth() { DELETEA(wlauth); }
};

#endif // Z_EXTRAINFO_H
