#ifdef Z_MAPSUCKS_H
#error "already z_mapsucks.h"
#endif
#define Z_MAPSUCKS_H

#ifndef Z_SERVERCOMMANDS_H
#error "want z_servercommands.h"
#endif
#ifndef Z_FORMAT_H
#error "want z_format.h"
#endif


VARF(mapsucks, 0, 0, 2, z_enable_command("mapsucks", mapsucks!=0));
VAR(mapsucks_time, 0, 30, 3600);

SVAR(mapsucks_style_vote, "%C thinks this map sucks. current mapsucks votes: [%z/%l]. you can rate this map with #mapsucks");
SVAR(mapsucks_style_waitsuccess, "mapsucks voting succeeded, staring intermission in %t seconds");
SVAR(mapsucks_style_success, "mapsucks voting succeeded, starting intermission");
SVAR(mapsucks_style_speccantvote, "Spectators may not rate map");
SVAR(mapsucks_style_editmode, "mapsucks is disabled in coop edit mode");

void z_mapsucks(clientinfo *ci)
{
    vector<uint> plips, msips;
    if(interm) return;
    bool changed = ci->xi.mapsucks <= 0;
    ci->xi.mapsucks = 1;
    loopv(clients) if(clients[i]->state.aitype==AI_NONE && clients[i]->state.state!=CS_SPECTATOR)
    {
        uint ip = getclientip(clients[i]->clientnum);
        if(plips.find(ip)<0) plips.add(ip);
        if(clients[i]->xi.mapsucks > 0 && msips.find(ip)<0) msips.add(ip);
    }
    int needvotes;
    if(plips.length() > 2) needvotes = (plips.length() + 1) / 2;
    else needvotes = (plips.length() + 2) / 2;
    if(changed)
    {
        z_formattemplate ft[] =
        {
            { 'C', "%s", (const void *)colorname(ci) },
            { 'c', "%s", (const void *)ci->name },
            { 'n', "%d", (const void *)(intptr_t)ci->clientnum },
            { 'z', "%d", (const void *)(uintptr_t)msips.length() },
            { 'l', "%d", (const void *)(intptr_t)needvotes },
            { 0, NULL, NULL }
        };
        string buf;
        z_format(buf, sizeof(buf), mapsucks_style_vote, ft);
        if(buf[0]) sendservmsg(buf);
    }
    if(msips.length() >= needvotes)
    {
        if(m_timed && mapsucks_time)
        {
            if(gamelimit-gamemillis > mapsucks_time*1000)
            {
                gamelimit = gamemillis + mapsucks_time*1000;

                z_formattemplate ft[] =
                {
                    { 't', "%d", (const void *)(intptr_t)mapsucks_time },
                    { 0, NULL, NULL }
                };
                string buf;
                z_format(buf, sizeof(buf), mapsucks_style_waitsuccess, ft);
                if(*buf) sendservmsg(buf);

                sendf(-1, 1, "ri2", N_TIMEUP, max((gamelimit - gamemillis)/1000, 1));
            }
        }
        else
        {
            if(mapsucks_style_success[0]) sendservmsg(mapsucks_style_success);
            startintermission();
        }
    }
}

void z_servcmd_mapsucks(int argc, char **argv, int sender)
{
    clientinfo *ci = getinfo(sender);
    if(!ci) return;
    if(mapsucks != 1 && m_edit) { sendf(sender, 1, "ris", N_SERVMSG, mapsucks_style_editmode); return; }
    if(ci->state.state==CS_SPECTATOR) { sendf(sender, 1, "ris", N_SERVMSG, mapsucks_style_speccantvote); return; }
    z_mapsucks(ci);
}
SCOMMANDA(mapsucks, ZC_DISABLED | PRIV_NONE, z_servcmd_mapsucks, 1);
