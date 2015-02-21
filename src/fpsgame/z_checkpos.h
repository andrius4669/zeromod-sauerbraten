#ifndef Z_CHECKPOS_H
#define Z_CHECKPOS_H

VAR(servertrackpos, 0, 0, 1);

void z_processpos(clientinfo *ci, clientinfo *cp)
{
    if(!servertrackpos || m_edit) return;
    z_posqueue &pt = cp->xi.postrack;
    if(pt.size < 125) pt.reset(125);
    pt.removeold(totalmillis-4000);
    // client sends messages in 33ms intervals. 4000/33 = 121.212121212... if we get messages at higher rate, its speedhack
    bool res = pt.addpos(cp->state.o, totalmillis, gamemillis);
    if(!res)
    {
        if(pt.length()) pt.removeold();
        pt.addpos(cp->state.o, totalmillis, gamemillis);
        logoutf("checkpos: possible speedhack: %s (%d) - %d pos messages in %d ms", ci->name, ci->clientnum, pt.size, pt.totaltime);
    }
}

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

#endif // Z_CHECKPOS_H
