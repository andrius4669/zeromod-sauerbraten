#ifndef Z_MAPSUCKS_H
#define Z_MAPSUCKS_H

#include "z_servercommands.h"

extern void z_mapsucks_trigger(int);
VARF(mapsucks, 0, 0, 1, z_mapsucks_trigger(0));
VAR(mapsucks_time, 0, 30, 3600);

void z_mapsucks_trigger(int type)
{
    z_enable_command("mapsucks", mapsucks!=0);
}
Z_TRIGGER(z_mapsucks_trigger, Z_TRIGGER_STARTUP);

void z_mapsucks(clientinfo *ci)
{
    vector<uint> msips;
    int players = 0;
    if(interm) return;
    bool changed = ci->mapsucks <= 0;
    ci->mapsucks = 1;
    loopv(clients) if(clients[i]->state.aitype==AI_NONE && clients[i]->state.state!=CS_SPECTATOR)
    {
        players++;
        if(clients[i]->mapsucks > 0)
        {
            uint ip = getclientip(clients[i]->clientnum);
            if(msips.find(ip)<0) msips.add(ip);
        }
    }
    int needvotes;
    if(players > 2) needvotes = (players + 1) / 2;
    else needvotes = (players + 2) / 2;
    if(changed) sendservmsgf("%s thinks this map sucks. current mapsucks votes: [%d/%d]. you can rate this map with #mapsucks",
                                colorname(ci), msips.length(), needvotes);
    if(msips.length() >= needvotes)
    {
        if(m_timed && mapsucks_time)
        {
            if(gamelimit-gamemillis > mapsucks_time*1000)
            {
                gamelimit = gamemillis + mapsucks_time*1000;
                sendservmsgf("mapsucks voting succeeded, staring intermission in %d seconds", mapsucks_time);
            }
        }
        else
        {
            sendservmsg("mapsucks voting succeeded, starting intermission");
            startintermission();
        }
    }
}

void z_servcmd_mapsucks(int argc, char **argv, int sender)
{
    if(!mapsucks) { sendf(sender, 1, "ris", N_SERVMSG, "mapsucks command is disabled"); return; }
    clientinfo *ci = (clientinfo *)getclientinfo(sender);
    if(!ci) return;
    if(ci->state.state==CS_SPECTATOR) { sendf(sender, 1, "ris", N_SERVMSG, "Spectators may not rate map"); return; }
    z_mapsucks(ci);
}
SCOMMANDA(mapsucks, PRIV_NONE, z_servcmd_mapsucks, 1);

#endif // Z_MAPSUCKS_H
