#ifndef Z_GENERICSERVERCOMMANDS_H
#define Z_GENERICSERVERCOMMANDS_H 1

#include "z_servcmd.h"
#include "z_format.h"
#include "z_invpriv.h"

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

void z_servcmd_info(int argc, char **argv, int sender)
{
    vector<char> uptimebuf;
    z_formatsecs(uptimebuf, totalsecs);
    uptimebuf.add('\0');
    sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("server uptime: %s", uptimebuf.getbuf()));
}
SCOMMANDAH(info, PRIV_NONE, z_servcmd_info, 1);
SCOMMANDA(uptime, PRIV_NONE, z_servcmd_info, 1);

SVAR(stats_style_normal, "\f6stats: \f7%C: \f2frags: \f7%f\f2, deaths: \f7%l\f2, suicides: \f7%s\f2, accuracy(%%): \f7%a\f2, kpd: \f7%p");
SVAR(stats_style_teamplay, "\f6stats: \f7%C: \f2frags: \f7%f\f2, deaths: \f7%l\f2, suicides: \f7%s\f2, teamkills: \f7%t\f2, accuracy(%%): \f7%a\f2, kpd: \f7%p");
SVAR(stats_style_ctf, "\f6stats: \f7%C: \f2frags: \f7%f\f2, flags: \f7%g\f2, deaths: \f7%l\f2, suicides: \f7%s\f2, teamkills: \f7%t\f2, accuracy(%%): \f7%a\f2, kpd: \f7%p");

static inline void z_putstats(char (&msg)[MAXSTRLEN], clientinfo *ci)
{
    gamestate &gs = ci->state;
    int p = gs.frags*1000/max(gs.deaths, 1);
    string ps;
    formatstring(ps)("%d.%03d", p/1000, p%1000);
    z_formattemplate ft[] =
    {
        { 'C', "%s", colorname(ci) },
        { 'f', "%d", (const void *)(long)gs.frags },
        { 'g', "%d", (const void *)(long)gs.flags },
        { 'p', "%s", ps },
        { 'a', "%d", (const void *)(long)(gs.damage*100/max(gs.shotdamage,1)) },
        { 'd', "%d", (const void *)(long)gs.damage },
        { 't', "%d", (const void *)(long)gs.teamkills },
        { 'l', "%d", (const void *)(long)gs.deaths },
        { 's', "%d", (const void *)(long)gs.suicides },
        { 'w', "%d", (const void *)(long)(gs.shotdamage-gs.damage) },
        { 'r', "%d", (const void *)(long)gs.maxsteak },
        { 0, 0, 0 }
    };
    if(m_ctf) z_format(msg, sizeof(msg), stats_style_ctf, ft);
    else if(m_teammode) z_format(msg, sizeof(msg), stats_style_teamplay, ft);
    else z_format(msg, sizeof(msg), stats_style_normal, ft);
}

void z_servcmd_stats(int argc, char **argv, int sender)
{
    int cn, i;
    clientinfo *ci = NULL, *senderci = getinfo(sender);
    vector<clientinfo *> cis;
    for(i = 1; i < argc; i++)
    {
        if(!z_parseclient_verify(argv[i], cn, true, true, senderci->local || senderci->privilege>=PRIV_ADMIN))
        {
            z_servcmd_unknownclient(argv[i], sender);
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

    string buf;
    for(i = 0; i < cis.length(); i++)
    {
        z_putstats(buf, cis[i]);
        if(*buf) sendf(sender, 1, "ris", N_SERVMSG, buf);
    }
}
SCOMMAND(stats, PRIV_NONE, z_servcmd_stats);

VAR(servcmd_pm_comfirmation, 0, 1, 1);
void z_servcmd_pm(int argc, char **argv, int sender)
{
    if(argc <= 1) { z_servcmd_pleasespecifyclient(sender); return; }
    if(argc <= 2) { z_servcmd_pleasespecifymessage(sender); return; }
    int cn;
    clientinfo *ci;
    if(!z_parseclient_verify(argv[1], cn, false, false, true))
    {
        z_servcmd_unknownclient(argv[1], sender);
        return;
    }
    ci = getinfo(sender);
    if(z_checkchatmute(ci)) { sendf(sender, 1, "ris", N_SERVMSG, "your pms are muted"); return; }
    sendf(cn, 1, "ris", N_SERVMSG, tempformatstring("\f6pm: \f7%s \f5(%d)\f7: \f0%s", ci->name, ci->clientnum, argv[2]));
    if(servcmd_pm_comfirmation)
    {
        ci = getinfo(cn);
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("your private message was successfully sent to %s \f5(%d)", ci->name, ci->clientnum));
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
    if(argc <= 1) { z_servcmd_pleasespecifymessage(sender); return; }
    sendservmsg(argv[1]);
}
SCOMMANDA(wall, PRIV_ADMIN, z_servcmd_wall, 1);

void z_servcmd_achat(int argc, char **argv, int sender)
{
    if(argc <= 1) { z_servcmd_pleasespecifymessage(sender); return; }
    clientinfo *ci = getinfo(sender);
    loopv(clients) if(clients[i]->state.aitype==AI_NONE && (clients[i]->local || clients[i]->privilege >= PRIV_ADMIN))
    {
        sendf(clients[i]->clientnum, 1, "ris", N_SERVMSG, tempformatstring("\f6achat: \f7%s: \f0%s", colorname(ci), argv[1]));
    }
}
SCOMMANDA(achat, PRIV_ADMIN, z_servcmd_achat, 1);

void z_servcmd_reqauth(int argc, char **argv, int sender)
{
    if(argc <= 1) { z_servcmd_pleasespecifyclient(sender); return; }
    if(argc <= 2 && !*serverauth) { sendf(sender, 1, "ris", N_SERVMSG, "please specify authdesc"); return; }

    int cn;
    if(!z_parseclient_verify(argv[1], cn, true, false, true))
    {
        z_servcmd_unknownclient(argv[1], sender);
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
