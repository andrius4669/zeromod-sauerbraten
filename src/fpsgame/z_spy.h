#ifdef Z_SPY_H
#error "already z_spy.h"
#endif
#define Z_SPY_H

#ifndef Z_SERVCMD_H
#error "want z_servcmd.h"
#endif
#ifndef Z_GEOIPSERVER_H
#error "want z_geoipserver.h"
#endif


void z_setspy(clientinfo *ci, bool val)
{
    if(ci->spy ? val : !val) return;
    if(val)
    {
        flushserver(true);
        // spectate client first (show only to client itself)
        if(ci->state.state==CS_ALIVE) suicide(ci);
        if(smode) smode->leavegame(ci);
        ci->state.state = CS_SPECTATOR;
        ci->state.timeplayed += lastmillis - ci->state.lasttimeplayed;
        aiman::removeai(ci);
        sendf(ci->clientnum, 1, "ri3", N_SPECTATOR, ci->clientnum, 1);
        // send out fake relinquish messages (for other clients only)
        if(ci->privilege >= PRIV_MASTER)
        {
            defformatstring(msg, "%s %s %s", colorname(ci), "relinquished", privname(ci->privilege));
            for(int i = demorecord ? -1 : 0; i < clients.length(); i++)
            {
                clientinfo *cx = i >= 0 ? clients[i] : NULL;
                if(!z_canseemypriv(ci, cx) || (cx && (cx->clientnum == ci->clientnum || cx->state.aitype != AI_NONE))) continue;
                packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
                putint(p, N_SERVMSG);
                sendstring(msg, p);
                putint(p, N_CURRENTMASTER);
                putint(p, mastermode);
                loopvj(clients) if(clients[j]->privilege >= PRIV_MASTER && z_canseemypriv(clients[j], cx))
                {
                    putint(p, clients[j]->clientnum);
                    putint(p, clients[j]->privilege);
                }
                putint(p, -1);
                if(cx) sendpacket(cx->clientnum, 1, p.finalize());
                else { p.finalize(); recordpacket(1, p.packet->data, p.packet->dataLength); }
            }
        }
        // send fake disconnect message (for other clients only)
        sendf(-1, 1, "rxi2", ci->clientnum, N_CDIS, ci->clientnum);
        // done
        ci->spy = true;
    }
    else
    {
        ci->spy = false;
        // client should appear unspectated
        if(mastermode < MM_LOCKED && !shouldspectate(ci))
        {
            // dead
            if(smode && !smode->canspawn(ci, true))
            {
                ci->state.state = CS_DEAD;
                ci->state.respawn();
                ci->state.lasttimeplayed = lastmillis;
                aiman::addclient(ci);
                sendf(ci->clientnum, 1, "ri3", N_SPECTATOR, ci->clientnum, 0);
                sendf(-1, 1, "ri2x", N_FORCEDEATH, ci->clientnum, ci->clientnum);
            }
            // alive
            else
            {
                ci->state.state = CS_DEAD;
                ci->state.respawn();
                ci->state.lasttimeplayed = lastmillis;
                aiman::addclient(ci);
                sendspawn(ci);
            }
        }
        // client should appear spectated
        else
        {
            sendf(-1, 1, "ri3x", N_SPECTATOR, ci->clientnum, 1, ci->clientnum);
        }
        // send resume
        sendresume(ci);
        // send join notification
        sendinitclient(ci);
        // fake geoip message
        z_geoip_show(ci);
        // privilege evaluation message
        if(ci->privilege >= PRIV_MASTER)
        {
            defformatstring(msg, "%s %s %s", colorname(ci), "claimed", privname(ci->privilege));
            for(int i = demorecord ? -1 : 0; i < clients.length(); i++)
            {
                clientinfo *cx = i >= 0 ? clients[i] : NULL;
                if(!z_canseemypriv(ci, cx) || (cx && (cx->clientnum == ci->clientnum || cx->state.aitype != AI_NONE))) continue;
                packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
                putint(p, N_SERVMSG);
                sendstring(msg, p);
                putint(p, N_CURRENTMASTER);
                putint(p, mastermode);
                loopvj(clients) if(clients[j]->privilege >= PRIV_MASTER && z_canseemypriv(clients[j], cx))
                {
                    putint(p, clients[j]->clientnum);
                    putint(p, clients[j]->privilege);
                }
                putint(p, -1);
                if(cx) sendpacket(cx->clientnum, 1, p.finalize());
                else { p.finalize(); recordpacket(1, p.packet->data, p.packet->dataLength); }
            }
        }
    }
    loopv(clients)
        if(clients[i]->state.aitype==AI_NONE && (clients[i]->privilege >= PRIV_ADMIN || clients[i]->local || clients[i]->clientnum == ci->clientnum))
            sendf(clients[i]->clientnum, 1, "ris", N_SERVMSG, tempformatstring("%s %s spy mode", colorname(ci), val ? "entered" : "left"));
}

void z_servcmd_spy(int argc, char **argv, int sender)
{
    clientinfo *ci = (clientinfo *)getclientinfo(sender);
    if(!ci) return;
    z_setspy(ci, !ci->spy);
}
SCOMMANDA(spy, PRIV_ADMIN, z_servcmd_spy, 1);

void z_servcmd_listspy(int argc, char **argv, int sender)
{
    vector<char> spybuf;
    loopv(clients) if(clients[i]->spy)
    {
        if(spybuf.length()) spybuf.add(' ');
        const char *n = colorname(clients[i]);
        spybuf.put(n, strlen(n));
    }
    if(spybuf.empty()) sendf(sender, 1, "ris", N_SERVMSG, "no spies in this server");
    else
    {
        spybuf.add('\0');
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("spies list: %s", spybuf.getbuf()));
    }
}
SCOMMANDA(listspy, PRIV_ADMIN, z_servcmd_listspy, 1);
