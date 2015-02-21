#ifndef Z_CHECKPOS_H
#define Z_CHECKPOS_H

VAR(servertrackpos, 0, 0, 1);

void z_processpos(clientinfo *ci, clientinfo *cp)
{
    if(!servertrackpos || m_edit) return;
    if(cp->xi.postrack.size < 200) cp->xi.postrack.reset(200);
    cp->xi.postrack.removeold(totalmillis-5000);
    cp->xi.postrack.addpos(cp->state.o, totalmillis, gamemillis);
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
