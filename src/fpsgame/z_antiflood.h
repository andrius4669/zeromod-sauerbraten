#ifndef Z_ANTIFLOOD_H
#define Z_ANTIFLOOD_H

#include "z_queue.h"

VAR(antiflood_disconnect, 0, 0, 1);
VAR(antiflood_text, 0, 10, 8192);
VAR(antiflood_text_time, 1, 5000, 2*60000);
VAR(antiflood_rename, 0, 4, 8192);
VAR(antiflood_rename_time, 0, 4000, 2*60000);
VAR(antiflood_team, 0, 4, 8192);
VAR(antiflood_team_time, 1, 4000, 2*60000);

static int z_ratelimit(z_queue<int> &q, int time, int limit)
{
    while(q.length() && time-q.first() >= limit) q.remove();
    if(q.full()) return q.capacity() ? limit + q.first() - time : 0;
    q.add(time);
    return 0;
}

static inline void z_fixantifloodsize(z_queue<int> &q, int size) { if(q.capacity() != size) q.resize(size); }

static int z_warnantiflood(int afwait, int cn, const char *typestr)
{
    if(afwait <= 0) return 0;
    if(antiflood_disconnect)
    {
        disconnect_client(cn, DISC_OVERFLOW);
        return -1;
    }
    sendf(cn, 1, "ris", N_SERVMSG, tempformatstring("antiflood: %s message was blocked. wait %d milliseconds before resending.", typestr, afwait));
    return 1;
}

static int z_antiflood(clientinfo *ci, int type, const char *typestr)
{
    if(!ci) return 0;
    switch(type)
    {
        case N_TEXT:
        case N_SAYTEAM:
            z_fixantifloodsize(ci->xi.af_text, antiflood_text);
            return z_warnantiflood(z_ratelimit(ci->xi.af_text, totalmillis, antiflood_text_time), ci->clientnum, typestr);
        case N_SWITCHNAME:
            z_fixantifloodsize(ci->xi.af_rename, antiflood_rename);
            return z_warnantiflood(z_ratelimit(ci->xi.af_rename, totalmillis, antiflood_rename_time), ci->clientnum, typestr);
        case N_SWITCHTEAM:
            z_fixantifloodsize(ci->xi.af_team, antiflood_team);
            return z_warnantiflood(z_ratelimit(ci->xi.af_team, totalmillis, antiflood_team_time), ci->clientnum, typestr);
    }
    return 0;
}

#define Z_ANTIFLOOD(client, type) \
{ \
    int _rr = z_antiflood(client, type, #type); \
    if(_rr > 0) break; \
    if(_rr < 0) return; \
}

#endif // Z_ANTIFLOOD_H
