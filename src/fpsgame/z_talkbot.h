#ifdef Z_TALKBOT_H
#error "already z_talkbot.h"
#endif
#define Z_TALKBOT_H


#define MAXTALKBOTS (0x100 - MAXCLIENTS - MAXBOTS)

static vector<clientinfo *> talkbots;

static void deltalkbot(int i)
{
    if(!talkbots.inrange(i) || !talkbots[i]) return;
    sendf(-1, 1, "ri2", N_CDIS, talkbots[i]->clientnum);
    clients.removeobj(talkbots[i]);
    delete talkbots[i];
    talkbots[i] = NULL;
}

void cleartalkbots(int *n, int *numargs)
{
    if(*numargs <= 0)
    {
        loopv(talkbots) if(talkbots[i]) deltalkbot(i);
        talkbots.setsize(0);
    }
    else deltalkbot(*n);
}
COMMAND(cleartalkbots, "iN");

static void puttalkbotinit(packetbuf &p, clientinfo *ci, const char *altname = NULL)
{
    putint(p, N_INITAI);
    putint(p, ci->clientnum);
    putint(p, ci->ownernum);
    putint(p, ci->state.aitype);
    putint(p, ci->state.skill);
    putint(p, ci->playermodel);
    sendstring(altname ? altname : ci->name, p);
    sendstring(ci->team, p);
}

static int addtalkbot(int wantcn, const char *name)
{
    int cn = -1;
    clientinfo *ci;
    if(wantcn >= 0)
    {
        if(wantcn >= MAXTALKBOTS) return -1;
        cn = wantcn;
        while(cn >= talkbots.length()) talkbots.add(NULL);
    }
    else
    {
        loopv(talkbots) if(!talkbots[i]) { cn = i; break; }
        if(cn < 0)
        {
            cn = talkbots.length();
            if(cn >= MAXTALKBOTS) return -1;
            talkbots.add(NULL);
        }
    }

    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);

    if(!talkbots[cn])
    {
        talkbots[cn] = new clientinfo;
        talkbots[cn]->clientnum = MAXCLIENTS + MAXBOTS + cn;
        clients.add(talkbots[cn]);

        putint(p, N_SPECTATOR);
        putint(p, talkbots[cn]->clientnum);
        putint(p, 1);
    }
    ci = talkbots[cn];
    ci->ownernum = ci->clientnum;   // not owned by anyone else, still must provide owner cn
    ci->connected = true;
    ci->connectmillis = 0;  // since this is fake
    ci->state.aitype = AI_BOT;
    ci->state.skill = 0;
    ci->state.state = CS_SPECTATOR; // this aint real bot
    ci->playermodel = 0;    // dunno what would be best there
    copystring(ci->name, name);
    copystring(ci->team, "");   // teamless
    ci->state.lasttimeplayed = lastmillis;
    ci->aireinit = 0;
    ci->reassign();

    puttalkbotinit(p, ci);

    sendpacket(-1, 1, p.finalize());

    return cn;
}
ICOMMAND(talkbot, "is", (int *cn, char *name), intret(addtalkbot(*cn, name)));

void talkbotpriv(int *bn, int *priv, int *inv)
{
    if(!talkbots.inrange(*bn)) return;
    clientinfo *ci = talkbots[*bn];
    if(!ci) return;
    bool changed = false;
    changed = changed || (ci->privilege != *priv);
    if(*inv >= 0) changed = changed || (ci->xi.invpriv != (*inv > 0));
    if(!changed) return;

    ci->privilege = clamp(*priv, int(PRIV_NONE), int(PRIV_ADMIN));
    if(*inv >= 0) ci->xi.invpriv = *inv > 0;

    for(int i = demorecord ? -1 : 0; i < clients.length(); i++)
    {
        clientinfo *cx = i >= 0 ? clients[i] : NULL;
        if(cx && cx->state.aitype != AI_NONE) continue;
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
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
COMMAND(talkbotpriv, "iib");

static void talkbot_putsay(packetbuf &p, int cn, const char *msg)
{
    vector<uchar> b;
    putint(b, N_TEXT);
    sendstring(msg, b);

    putint(p, N_CLIENT);
    putint(p, cn);
    putuint(p, b.length());
    p.put(b.getbuf(), b.length());
}

static void talkbot_putsayteam(packetbuf &p, int cn, const char *msg)
{
    putint(p, N_SAYTEAM);
    putint(p, cn);
    sendstring(msg, p);
}

static void talkbot_say(int cn, const char *msg)
{
    if(!talkbots.inrange(cn)) return;
    clientinfo *ci = talkbots[cn];
    if(!ci) return;

    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    talkbot_putsay(p, ci->clientnum, msg);
    sendpacket(-1, 1, p.finalize());
}

static void talkbot_fakesay(int cn, const char *fakename, const char *msg)
{
    if(!talkbots.inrange(cn)) return;
    clientinfo *ci = talkbots[cn];
    if(!ci) return;

    // pack all in one packet, so there is no chance for client to notice name change
    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    puttalkbotinit(p, ci, fakename);
    talkbot_putsay(p, ci->clientnum, msg);
    puttalkbotinit(p, ci);
    sendpacket(-1, 1, p.finalize());
}

static void talkbot_sayteam(int cn, int tcn, const char *msg)
{
    if(!talkbots.inrange(cn) || (tcn >= 0 && !getclientinfo(tcn))) return;
    clientinfo *ci = talkbots[cn];
    if(!ci) return;

    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    talkbot_putsayteam(p, ci->clientnum, msg);
    sendpacket(tcn, 1, p.finalize());
}

static void talkbot_fakesayteam(int cn, const char *fakename, int tcn, const char *msg)
{
    if(!talkbots.inrange(cn) || (tcn >= 0 && !getclientinfo(tcn))) return;
    clientinfo *ci = talkbots[cn];
    if(!ci) return;

    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    puttalkbotinit(p, ci, fakename);
    talkbot_putsayteam(p, ci->clientnum, msg);
    puttalkbotinit(p, ci);
    sendpacket(tcn, 1, p.finalize());
}

ICOMMAND(s_talkbot_say, "is", (int *bn, char *msg),
{
    filtertext(msg, msg, true, true, strlen(msg));
    talkbot_say(*bn, msg);
});

ICOMMAND(s_talkbot_fakesay, "iss", (int *bn, char *fakename, char *msg),
{
    filtertext(msg, msg, true, true, strlen(msg));
    filtertext(fakename, fakename, false, false, strlen(fakename));
    talkbot_fakesay(*bn, fakename, msg);
});

ICOMMAND(s_talkbot_sayteam, "iis", (int *bn, int *tn, char *msg),
{
    filtertext(msg, msg, true, true, strlen(msg));
    talkbot_sayteam(*bn, *tn, msg);
});

ICOMMAND(s_talkbot_fakesayteam, "isis", (int *bn, char *fakename, int *tn, char *msg),
{
    filtertext(msg, msg, true, true, strlen(msg));
    filtertext(fakename, fakename, false, false, strlen(fakename));
    talkbot_fakesayteam(*bn, fakename, *tn, msg);
});

ICOMMAND(listtalkbots, "s", (char *name), {
    string n;
    vector<char> buf;
    loopv(talkbots) if(talkbots[i] && (!name[0] || !strcmp(talkbots[i]->name, name)))
    {
        if(buf.length()) buf.add(' ');
        formatstring(n, "%d", i);
        buf.put(n, strlen(n));
    }
    buf.add('\0');
    result(buf.getbuf());
});
