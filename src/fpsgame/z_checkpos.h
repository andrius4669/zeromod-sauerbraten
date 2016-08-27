#ifdef Z_CHECKPOS_H
#error "already z_checkpos.h"
#endif
#define Z_CHECKPOS_H

#ifndef Z_GHOST_H
#error "want z_ghost.h"
#endif

static int z_nextexceeded = 0;

VAR(servertrackpos, 0, 0, 1);
VAR(servertrackpos_autotune, 0, 0, 1);
FVAR(servertrackpos_maxvel, 0.0, 0.0, 10000.0);
VAR(servertrackpos_maxlag, 0, 0, INT_MAX);

static void z_clientdied(clientinfo *ci)
{
    ci->xi.postrack.reset();
}

static void z_calcnextexceeded(clientinfo *ci)
{
    z_posqueue &pt = ci->xi.postrack;
    if(pt.exceededrate) z_nextexceeded = totalmillis ? totalmillis : 1;
}

static void z_processpos(clientinfo *ci, clientinfo *cp)
{
    if(z_shouldhidepos(cp)) nullifyclientpos(*cp);
    z_posqueue &pt = cp->xi.postrack;
    if(!servertrackpos || m_edit || /*ci->local ||*/ cp->state.state != CS_ALIVE)
    {
        if(pt.size) pt.reset();
        return;
    }
    if(pt.size < 125) pt.reset(125);
    pt.removeold(totalmillis-4000);
    // client sends messages in 33ms intervals. 4000/33 = 121.212121212... if we get messages at higher rate, its lame kind of speedhack
    bool res = pt.addpos(cp->state.o, totalmillis, gamemillis);
    if(!res && pt.length())
    {
        pt.exceededrate = true;
        pt.removefirst();
        pt.addpos(cp->state.o, totalmillis, gamemillis);
        z_calcnextexceeded(cp);
    }
}

static inline void z_checkexceeded()
{
    if(z_nextexceeded && z_nextexceeded-totalmillis <= 0)
    {
        z_nextexceeded = 0;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.state != CS_ALIVE) continue;
            z_posqueue &pt = ci->xi.postrack;
            if(pt.exceededrate)
            {
                clientinfo *oi = getinfo(ci->ownernum);
                logoutf("checkpos: possible speedhack: %s (%d) - %d pos messages in %d ms", oi->name, oi->clientnum, pt.size, pt.totaltime);
            }
        }
    }
}

// unfinished
#if 0
// check whether client can reach certain object
static bool z_is_reachable(clientinfo &ci, const vec &o, float radius)
{
    int allowedlag = ci.calcpushrange();        /* TODO: determine this value from ping/round trip time more preciselly */
    int timepassed, gtimepassed;
    float distance;
    z_posqueue &pt = ci.xi.postrack;
    loopv(pt)
    {
        z_posrecord &pr = pt.last(i);
        timepassed = totalmillis - pr.tmillis;
        gtimepassed = gamemillis - pr.gmillis;
        distance = ci.state.o.dist2(o);
        if(i && timepassed > allowedlag) break;
        float couldgo = 0;
    }
}
#endif

void z_test_totalx(int *cn)
{
    clientinfo *ci = getinfo(*cn);
    floatret(ci ? ci->xi.postrack.tx : 0.0f);
}
COMMAND(z_test_totalx, "i");

void z_test_totaly(int *cn)
{
    clientinfo *ci = getinfo(*cn);
    floatret(ci ? ci->xi.postrack.ty : 0.0f);
}
COMMAND(z_test_totaly, "i");

void z_test_totalmillis(int *cn)
{
    clientinfo *ci = getinfo(*cn);
    intret(ci ? ci->xi.postrack.gametime : 0);
}
COMMAND(z_test_totalmillis, "i");

void z_test_gamemillis(int *cn)
{
    clientinfo *ci = getinfo(*cn);
    intret(ci ? ci->xi.postrack.totaltime : 0);
}
COMMAND(z_test_gamemillis, "i");

void z_test_poslen(int *cn)
{
    clientinfo *ci = getinfo(*cn);
    intret(ci ? ci->xi.postrack.length() : 0);
}
COMMAND(z_test_poslen, "i");
