#ifndef Z_TALKBOT_H
#define Z_TALKBOT_H

#define MAXTALKBOTS (0x100 - MAXCLIENTS - MAXBOTS)

vector<clientinfo *> talkbots;

void deltalkbot(int i)
{
    if(!talkbots.inrange(i) || !talkbots[i]) return;
    sendf(-1, 1, "ri2", N_CDIS, talkbots[i]->clientnum);
    clients.removeobj(talkbots[i]);
    delete talkbots[i];
    talkbots[i] = NULL;
}

void cleartalkbots()
{
    loopv(talkbots) if(talkbots[i]) deltalkbot(i);
    talkbots.setsize(0);
}
COMMAND(cleartalkbots, "");

void puttalkbotinit(packetbuf &p, clientinfo *ci, const char *altname = NULL)
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

bool addtalkbot(int wantcn, const char *name)
{
    int cn = -1;
    clientinfo *ci;
    if(wantcn >= 0)
    {
        if(wantcn >= MAXTALKBOTS) return false;
        cn = wantcn;
        while(cn >= talkbots.length()) talkbots.add(NULL);
    }
    else
    {
        loopv(talkbots) if(!talkbots[i]) { cn = i; break; }
        if(cn < 0)
        {
            cn = talkbots.length();
            if(cn >= MAXTALKBOTS) return false;
            talkbots.add(NULL);
        }
    }

    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);

    if(!talkbots[cn])
    {
        talkbots[cn] = new clientinfo;
        talkbots[cn]->clientnum = MAXCLIENTS + MAXBOTS + cn;

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
    clients.add(ci);

    puttalkbotinit(p, ci);

    sendpacket(-1, 1, p.finalize());

    return true;
}
ICOMMAND(talkbot, "is", (int *cn, char *name), addtalkbot(*cn, name));

void talkbot_putsay(packetbuf &p, int cn, const char *msg)
{
    vector<uchar> b;
    putint(b, N_TEXT);
    sendstring(msg, b);

    putint(p, N_CLIENT);
    putint(p, cn);
    putuint(p, b.length());
    p.put(b.getbuf(), b.length());
}

void talkbot_putsayteam(packetbuf &p, int cn, const char *msg)
{
    putint(p, N_SAYTEAM);
    putint(p, cn);
    sendstring(msg, p);
}

void talkbot_say(int cn, const char *msg)
{
    if(!talkbots.inrange(cn)) return;
    clientinfo *ci = talkbots[cn];
    if(!ci) return;

    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    talkbot_putsay(p, ci->clientnum, msg);
    sendpacket(-1, 1, p.finalize());
}

void talkbot_fakesay(int cn, const char *fakename, const char *msg)
{
    if(!talkbots.inrange(cn)) return;
    clientinfo *ci = talkbots[cn];
    if(!ci) return;

    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    puttalkbotinit(p, ci, fakename);
    talkbot_putsay(p, ci->clientnum, msg);
    puttalkbotinit(p, ci);
    sendpacket(-1, 1, p.finalize());
}

void talkbot_sayteam(int cn, int tcn, const char *msg)
{
    if(!talkbots.inrange(cn) || (tcn >= 0 && !getclientinfo(tcn))) return;
    clientinfo *ci = talkbots[cn];
    if(!ci) return;

    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    talkbot_putsayteam(p, ci->clientnum, msg);
    sendpacket(tcn, 1, p.finalize());
}

void talkbot_fakesayteam(int cn, const char *fakename, int tcn, const char *msg)
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
    filtertext(fakename, fakename, false, false, strlen(msg));
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
    filtertext(fakename, fakename, false, false, strlen(msg));
    talkbot_fakesayteam(*bn, fakename, *tn, msg);
});

#endif // Z_TALKBOT_H
