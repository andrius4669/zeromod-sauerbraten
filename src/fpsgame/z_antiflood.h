#ifdef Z_ANTIFLOOD_H
#error "already z_antoflood.h"
#endif
#define Z_ANTIFLOOD_H

#ifndef Z_QUEUE_H
#error "want z_queue.h"
#endif

VAR(antiflood_mode, 0, 2, 2);   // 0 - disconnect, 1 - rate limit, 2 - block
VAR(antiflood_text, 0, 10, 8192);
VAR(antiflood_text_time, 1, 5000, 2*60000);
VAR(antiflood_rename, 0, 4, 8192);
VAR(antiflood_rename_time, 0, 4000, 2*60000);
VAR(antiflood_team, 0, 4, 8192);
VAR(antiflood_team_time, 1, 4000, 2*60000);

SVAR(antiflood_style, "\f6antiflood: %T was blocked, wait %w ms before resending");

static int z_ratelimit(z_queue<int> &q, int time, int limit, bool overwrite)
{
    if(!q.capacity()) return 0;
    while(q.length() && time-q.first() >= limit) q.remove();
    if(!q.full())
    {
        q.add(time);
        return 0;
    }
    if(overwrite) q.add(time);
    return q.first() - time + limit;
}

static int z_warnantiflood(int afwait, int cn, const char *typestr)
{
    if(afwait <= 0) return 0;
    if(!antiflood_mode)
    {
        disconnect_client(cn, DISC_OVERFLOW);
        return -1;
    }
    z_formattemplate ft[] =
    {
        { 'T', "%s", typestr },
        { 'w', "%d", (const void *)(intptr_t)afwait },
        { 0,   NULL, NULL }
    };
    string buf;
    z_format(buf, sizeof(buf), antiflood_style, ft);
    if(*buf) sendf(cn, 1, "ris", N_SERVMSG, buf);
    return 1;
}

static int z_doantiflood(z_queue<int> &q, int num, int time, int cn, const char *ts)
{
    if(q.capacity() != num) q.resize(num);
    switch(antiflood_mode)
    {
        case 0: case 1: default:
            return z_warnantiflood(z_ratelimit(q, totalmillis, time, false), cn, ts);
        case 2:
            return z_warnantiflood(z_ratelimit(q, totalmillis, time, true), cn, ts);
    }
}

static int z_antiflood(clientinfo *ci, int type, const char *typestr)
{
    if(!ci) return 0;
    switch(type)
    {
        case N_TEXT:
        case N_SAYTEAM:
            return z_doantiflood(ci->xi.af_text, antiflood_text, antiflood_text_time, ci->clientnum, typestr);

        case N_SWITCHNAME:
            return z_doantiflood(ci->xi.af_rename, antiflood_rename, antiflood_rename_time, ci->clientnum, typestr);

        case N_SWITCHTEAM:
            return z_doantiflood(ci->xi.af_team, antiflood_team, antiflood_team_time, ci->clientnum, typestr);
    }
    return 0;
}

#define Z_ANTIFLOOD(client, type) \
{ \
    int _rr = z_antiflood(client, type, #type); \
    if(_rr > 0) break; \
    if(_rr < 0) return; \
}
