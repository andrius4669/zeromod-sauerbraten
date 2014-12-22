#ifndef Z_GENERICSERVERCOMMANDS_H
#define Z_GENERICSERVERCOMMANDS_H 1

#include "z_servcmd.h"

static char z_privcolor(int priv)
{
    if(priv <= PRIV_NONE) return '7';
    else if(priv <= PRIV_AUTH) return '0';
    else return '6';
}

static void z_servcmd_commands(int argc, char **argv, int sender)
{
    vector<char> cbufs[PRIV_ADMIN+1];
    clientinfo *ci = getinfo(sender);
    loopv(z_servcommands)
    {
        z_servcmdinfo &c = z_servcommands[i];
        if(!c.cansee(ci->privilege, ci->local)) continue;
        int j = 0;
        if(c.privilege >= PRIV_ADMIN) j = PRIV_ADMIN;
        else if(c.privilege >= PRIV_AUTH) j = PRIV_AUTH;
        else if(c.privilege >= PRIV_MASTER) j = PRIV_MASTER;
        if(cbufs[j].empty()) { cbufs[j].add('\f'); cbufs[j].add(z_privcolor(j)); }
        else { cbufs[j].add(','); cbufs[j].add(' '); }
        cbufs[j].put(c.name, strlen(c.name));
    }
    sendf(sender, 1, "ris", N_SERVMSG, "\f2avaiable server commands:");
    loopi(sizeof(cbufs)/sizeof(cbufs[0]))
    {
        if(cbufs[i].empty()) continue;
        cbufs[i].add('\0');
        sendf(sender, 1, "ris", N_SERVMSG, cbufs[i].getbuf());
    }
}
SCOMMANDA(commands, PRIV_NONE, z_servcmd_commands, 1);
SCOMMANDAH(help, PRIV_NONE, z_servcmd_commands, 1);

static const struct z_timedivinfo { const char *name; int timediv; } z_timedivinfos[] =
{
    // month is inaccurate 
    { "week", 60*60*24*7 },
    { "day", 60*60*24 },
    { "hour", 60*60 },
    { "minute", 60 },
    { "second", 1 }
};

void z_servcmd_info(int argc, char **argv, int sender)
{
    vector<char> uptimebuf;
    uint secs = totalsecs;
    loopi(sizeof(z_timedivinfos)/sizeof(z_timedivinfos[0]))
    {
        uint t = secs / z_timedivinfos[i].timediv;
        if(!t) continue;
        secs %= z_timedivinfos[i].timediv;
        if(uptimebuf.length()) uptimebuf.add(' ');
        const char *timestr = tempformatstring("%u", t);
        uptimebuf.put(timestr, strlen(timestr));
        uptimebuf.add(' ');
        uptimebuf.put(z_timedivinfos[i].name, strlen(z_timedivinfos[i].name));
        if(t > 1) uptimebuf.add('s');
        if(!secs) break;
    }
    uptimebuf.add('\0');
    sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("server uptime: %s", uptimebuf.getbuf()));
}
SCOMMANDA(info, PRIV_NONE, z_servcmd_info, 1);

void z_servcmd_stats(int argc, char **argv, int sender)
{
    int cn, i;
    clientinfo *ci = NULL, *senderci = getinfo(sender);
    vector<clientinfo *> cis;
    for(i = 1; i < argc; i++)
    {
        if(!z_parseclient_verify(argv[i], &cn, true, true, senderci->local || senderci->privilege>=PRIV_ADMIN))
        {
            sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("unknown client: %s", argv[i]));
            return;
        }
        if(cn < 0)
        {
            cis.shrink(0);
            loopvj(clients) if(!clients[j]->spy || !senderci || senderci->local || senderci->privilege>=PRIV_ADMIN) cis.add(clients[j]);
            break;
        }
        ci = getinfo(cn);
        if(cis.find(ci)<0) cis.add(ci);
    }

    if(cis.empty() && senderci) cis.add(senderci);

    for(i = 0; i < cis.length(); i++)
    {
        ci = cis[i];
        string buf;
        if(m_ctf) formatstring(buf)(
            "\f6stats: \f7%s: \f2frags: \f7%d\f2, flags: \f7%d\f2, deaths: \f7%d\f2, teamkills: \f7%d\f2, accuracy(%%): \f7%d\f2, kpd: \f7%.2f",
            colorname(ci), ci->state.frags, ci->state.flags, ci->state.deaths, ci->state.teamkills,
            ci->state.damage*100/max(ci->state.shotdamage,1), float(ci->state.frags)/max(ci->state.deaths,1));
        else formatstring(buf)(
            "\f6stats: \f7%s: \f2frags: \f7%d\f2, deaths: \f7%d\f2, teamkills: \f7%d\f2, accuracy(%%): \f7%d\f2, kpd: \f7%.2f",
            colorname(ci), ci->state.frags, ci->state.deaths, ci->state.teamkills,
            ci->state.damage*100/max(ci->state.shotdamage,1), float(ci->state.frags)/max(ci->state.deaths,1));
        sendf(sender, 1, "ris", N_SERVMSG, buf);
    }
}
SCOMMAND(stats, PRIV_NONE, z_servcmd_stats);

VAR(servcmd_pm_comfirmation, 0, 1, 1);
void z_servcmd_pm(int argc, char **argv, int sender)
{
    if(argc <= 2) { sendf(sender, 1, "ris", N_SERVMSG, "please specify client and message"); return; }
    int cn;
    clientinfo *ci;
    if(!z_parseclient_verify(argv[1], &cn, false, true, true))
    {
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("unknown client: %s", argv[1]));
        return;
    }
    ci = getinfo(cn);
    if(ci->state.aitype!=AI_NONE) { sendf(sender, 1, "ris", N_SERVMSG, "you can not send private message to bot"); return; }
    ci = getinfo(sender);
    sendf(cn, 1, "ris", N_SERVMSG, tempformatstring("\f6pm: \f7%s \f5(%d)\f7: \f0%s", ci->name, ci->clientnum, argv[2]));
    if(servcmd_pm_comfirmation)
    {
        ci = getinfo(cn);
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("your private message successfully sent to %s \f5(%d)", ci->name, ci->clientnum));
    }
}
SCOMMANDA(pm, PRIV_NONE, z_servcmd_pm, 2);

void z_servcmd_interm(int argc, char **argv, int sender)
{
    startintermission();
}
SCOMMANDAH(interm, PRIV_MASTER, z_servcmd_interm, 1);
SCOMMANDA(intermission, PRIV_MASTER, z_servcmd_interm, 1);

void z_servcmd_wall(int argc, char **argv, int sender)
{
    if(argc <= 1) { sendf(sender, 1, "ris", N_SERVMSG, "please specify message"); return; }
    sendservmsg(argv[1]);
}
SCOMMANDA(wall, PRIV_ADMIN, z_servcmd_wall, 1);

void z_servcmd_reqauth(int argc, char **argv, int sender)
{
    if(argc < 2)
    {
        if(!serverauth[0]) sendf(sender, 1, "ris", N_SERVMSG, "please specify client and authdesc");
        else sendf(sender, 1, "ris", N_SERVMSG, "please specify client");
        return;
    }
    if(argc < 3 && !serverauth[0]) { sendf(sender, 1, "ris", N_SERVMSG, "please specify authdesc"); return; }

    int cn;
    if(!z_parseclient_verify(argv[1], &cn, true, false, true))
    {
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("unknown client: %s", argv[1]));
        return;
    }

    const char *authdesc = argc > 2 ? argv[2] : serverauth;

    if(cn >= 0) sendf(cn, 1, "ris", N_REQAUTH, authdesc);
    else loopv(clients) if(clients[i]->state.aitype==AI_NONE) sendf(clients[i]->clientnum, 1, "ris", N_REQAUTH, authdesc);
}
SCOMMANDA(reqauth, PRIV_ADMIN, z_servcmd_reqauth, 2);

#include "z_mutes.h"

#include "z_loadmap.h"
#include "z_savemap.h"

#include "z_rename.h"
#include "z_spy.h"
#include "z_mapsucks.h"
#include "z_slay.h"

#endif //Z_GENERICSERVERCOMMANDS_H
