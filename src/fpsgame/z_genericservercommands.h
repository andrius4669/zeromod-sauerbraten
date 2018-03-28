#ifdef Z_GENERICSERVERCOMMANDS_H
#error "already z_genericservercommands.h"
#endif
#define Z_GENERICSERVERCOMMANDS_H 1

#ifndef Z_SERVCMD_H
#error "want z_servcmd.h"
#endif
#ifndef Z_FORMAT_H
#error "want z_format.h"
#endif
#ifndef Z_INVPRIV_H
#error "want z_invpriv.h"
#endif


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
    sendf(sender, 1, "ris", N_SERVMSG, "\f2available server commands:");
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

void z_servcmd_ignore(int argc, char **argv, int sender)
{
    if(argc <= 1) { z_servcmd_pleasespecifyclient(sender); return; }

    clientinfo *sci = getinfo(sender);
    if(!sci) return;

    bool val = strcasecmp(argv[0], "unignore")!=0;

    loopi(argc-1)
    {
        int cn;
        if(!z_parseclient_verify(argv[i+1], cn, false, true))
        {
            z_servcmd_unknownclient(argv[i+1], sender);
            continue;
        }
        if(val)
        {
            sci->xi.ignores.addunique((uchar)cn);
            sci->xi.allows.removeobj((uchar)cn);
        }
        else sci->xi.ignores.removeobj((uchar)cn);
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("%s server messages from %s", val ? "ignoring" : "unignoring", colorname(getinfo(cn))));
    }
}
SCOMMAND(ignore, PRIV_NONE, z_servcmd_ignore);
SCOMMAND(unignore, PRIV_NONE, z_servcmd_ignore);

VAR(servcmd_pm_comfirmation, 0, 1, 1);
void z_servcmd_pm(int argc, char **argv, int sender)
{
    if(argc <= 1) { z_servcmd_pleasespecifyclient(sender); return; }
    if(argc <= 2) { z_servcmd_pleasespecifymessage(sender); return; }
    int cn;
    clientinfo *sci, *ci;

    sci = getinfo(sender);
    if(!sci) return;
    if(z_checkchatmute(sci))
    {
        sendf(sender, 1, "ris", N_SERVMSG, "your pms are muted");
        return;
    }

    ci = z_parseclient_return(argv[1], false, true);
    if(!ci)
    {
        z_servcmd_unknownclient(argv[1], sender);
        return;
    }
    cn = ci->clientnum;

    bool foundallows = ci->xi.allows.find((uchar)sender) >= 0;
    if(!foundallows && ci->spy && sci->privilege < PRIV_ADMIN)
    {
        z_servcmd_unknownclient(argv[1], sender);
        return;
    }

    sci->xi.allows.addunique((uchar)cn);

    if(ci->xi.ignores.find((uchar)sender) >= 0)
    {
        sendf(sender, 1, "ris", N_SERVMSG, "your pms are ignored");
        return;
    }

    if(!foundallows) ci->xi.allows.add((uchar)sender);

    sendf(cn, 1, "ris", N_SERVMSG, tempformatstring("\f6pm: \f7%s \f5(%d)\f7: \f0%s", sci->name, sci->clientnum, argv[2]));

    if(servcmd_pm_comfirmation && strcasecmp(argv[0], "qpm") != 0)
    {
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("your pm was successfully sent to %s \f5(%d)", ci->name, ci->clientnum));
    }
}
SCOMMANDA(pm, PRIV_NONE, z_servcmd_pm, 2);
SCOMMANDA(qpm, PRIV_NONE, z_servcmd_pm, 2);

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
    z_servcmdinfo *c = z_servcmd_find(argv[0]);
    int priv = c ? c->privilege : PRIV_ADMIN;
    loopv(clients) if(clients[i]->state.aitype==AI_NONE && (clients[i]->local || clients[i]->privilege >= priv))
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

static void z_quitwhenempty_trigger(int type)
{
    if(quitwhenempty) quitserver = true;
}
Z_TRIGGER(z_quitwhenempty_trigger, Z_TRIGGER_NOCLIENTS);
