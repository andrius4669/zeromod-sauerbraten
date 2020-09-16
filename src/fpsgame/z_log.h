#ifdef Z_LOG_H
#error "already z_log.h"
#endif
#define Z_LOG_H

static struct z_log_kickerinfostruct
{
    clientinfo *ci;
    int priv;

    z_log_kickerinfostruct(): ci(NULL), priv(PRIV_NONE) {}

    void set(clientinfo *i, int p) { ci = i; priv = p; }

    void reset()
    {
        ci = NULL;
        priv = PRIV_NONE;
    }
} z_log_kickerinfo;

void z_log_kick(clientinfo *actor, const char *aname, const char *adesc, int priv, clientinfo *victim, const char *reason)
{
    const char *kicker;
    if(!aname) kicker = tempformatstring("%s (%d)", actor->name, actor->clientnum);
    else
    {
        if(adesc && adesc[0]) kicker = tempformatstring("%s (%d) as '%s' [%s] (%s)", actor->name, actor->clientnum, aname, adesc, privname(priv));
        else kicker = tempformatstring("%s (%d) as '%s' (%s)", actor->name, actor->clientnum, aname, privname(priv));
    }
    if(reason && *reason) logoutf("kick: %s kicked %s (%d) because: %s", kicker, victim->name, victim->clientnum, reason);
    else logoutf("kick: %s kicked %s (%d)", kicker, victim->name, victim->clientnum);
    z_log_kickerinfo.set(actor, priv);
}

void z_log_kickdone()
{
    z_log_kickerinfo.reset();
}

static void z_sendmsgpacks(packetbuf &adminpack, packetbuf &normalpack, clientinfo *actor)
{
    recordpacket(1, normalpack.packet->data, normalpack.packet->dataLength);
    loopv(clients) if(clients[i]->state.aitype==AI_NONE)
    {
        clientinfo *ci = clients[i];
        if(actor && (ci==actor || ci->local || ci->privilege>=PRIV_ADMIN)) sendpacket(ci->clientnum, 1, adminpack.packet);
        else sendpacket(ci->clientnum, 1, normalpack.packet);
    }
}

SVAR(kick_style_normal,        "%k kicked %v");
SVAR(kick_style_normal_reason, "%k kicked %v because: %r");
SVAR(kick_style_spy,           "%v was kicked");
SVAR(kick_style_spy_reason,    "%v was kicked because: %r");

void z_showkick(const char *kicker, clientinfo *actor, clientinfo *vinfo, const char *reason)
{
    string kickstr;

    z_formattemplate ft[] =
    {
        { 'k', "%s", kicker },
        { 'v', "%s", colorname(vinfo) },
        { 'r', "%s", reason },
        { 0, NULL, NULL }
    };

    if(actor)
    {
        z_format(kickstr, sizeof(kickstr), reason && *reason ? kick_style_normal_reason : kick_style_normal, ft);
        if(!actor->spy)
        {
            sendservmsg(kickstr);
            return;
        }
    }

    packetbuf normalpack(MAXTRANS, ENET_PACKET_FLAG_RELIABLE), spypack(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);

    if(actor)
    {
        putint(normalpack, N_SERVMSG);
        sendstring(kickstr, normalpack);
        normalpack.finalize();
    }

    z_format(kickstr, sizeof(kickstr), reason && *reason ? kick_style_spy_reason : kick_style_spy, ft);
    putint(spypack, N_SERVMSG);
    sendstring(kickstr, spypack);
    spypack.finalize();

    z_sendmsgpacks(normalpack, spypack, actor);
}

SVAR(bans_style_normal,        "%C %bned %v for %t");
SVAR(bans_style_normal_reason, "%C %bned %v for %t because: %r");
SVAR(bans_style_spy,           "%v was %bned for %t");
SVAR(bans_style_spy_reason,    "%v was %bned for %t because: %r");

void z_showban(clientinfo *actor, const char *banstr, const char *victim, int bantime, const char *reason)
{
    string banmsg;
    vector<char> timebuf;
    z_formatsecs(timebuf, (uint)(bantime/1000));
    timebuf.add(0);

    z_formattemplate ft[] =
    {
        { 'C', "%s", actor ? colorname(actor) : "" },
        { 'b', "%s", banstr },
        { 'v', "%s", victim },
        { 't', "%s", timebuf.getbuf() },
        { 'r', "%s", reason },
        { 0, NULL, NULL }
    };

    if(actor)
    {
        z_format(banmsg, sizeof(banmsg), reason && *reason ? bans_style_normal_reason : bans_style_normal, ft);
        if(!actor->spy)
        {
            sendservmsg(banmsg);
            return;
        }
    }

    packetbuf normalpack(MAXTRANS, ENET_PACKET_FLAG_RELIABLE), spypack(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);

    if(actor)
    {
        putint(normalpack, N_SERVMSG);
        sendstring(banmsg, normalpack);
        normalpack.finalize();
    }

    z_format(banmsg, sizeof(banmsg), reason && *reason ? bans_style_spy_reason : bans_style_spy, ft);
    // packetbuf for normal clients without kicker shown (in case kicker is spy, it'd disclose him/her)
    putint(spypack, N_SERVMSG);
    sendstring(banmsg, spypack);
    spypack.finalize();

    z_sendmsgpacks(normalpack, spypack, actor);
}

static inline void z_log_setmaster(clientinfo *master, bool val, bool pass, const char *aname, const char *adesc, const char *priv, clientinfo *by)
{
    const char *mode;
    if(by) mode = tempformatstring("%s by %s (%d)", val ? "passed" : "taken", by->name, by->clientnum); // someone else used /setmaster
    else if(aname) mode = "auth";       // auth system
    else if(pass) mode = "password";    // password claim
    else mode = NULL;
    if(aname)
    {
        if(adesc && adesc[0]) logoutf("master: %s (%d) claimed %s as '%s' [%s] (%s)", master->name, master->clientnum, priv, aname, adesc, mode);
        else logoutf("master: %s (%d) claimed %s as '%s' (%s)", master->name, master->clientnum, priv, aname, mode);
    }
    else
    {
        if(mode) logoutf("master: %s (%d) %s %s (%s)", master->name, master->clientnum, val ? "claimed" : "relinquished", priv, mode);
        else logoutf("master: %s (%d) %s %s", master->name, master->clientnum, val ? "claimed" : "relinquished", priv);
    }
}

static inline void z_log_ident(clientinfo *ci, const char *name, const char *desc)
{
    if(desc && *desc) logoutf("ident: %s (%d) identified as '%s' [%s]", ci->name, ci->clientnum, name, desc);
    else logoutf("ident: %s (%d) identified as '%s'", ci->name, ci->clientnum, name);
}

static inline void z_log_say(clientinfo *cq, const char *tp)
{
    if(!isdedicatedserver()) return;
    if(cq->state.aitype==AI_NONE) logoutf("chat: %s (%d): %s", cq->name, cq->clientnum, tp);
    else logoutf("chat: %s [%d:%d]: %s", cq->name, cq->ownernum, cq->clientnum, tp);
}

static inline void z_log_sayteam(clientinfo *cq, const char *text, const char *team)
{
    if(!isdedicatedserver()) return;
    if(cq->state.aitype==AI_NONE) logoutf("chat: %s (%d) <%s>: %s", cq->name, cq->clientnum, team, text);
    else logoutf("chat: %s [%d:%d] <%s>: %s", cq->name, cq->ownernum, cq->clientnum, team, text);
}

static void z_log_rename(clientinfo *ci, const char *name, clientinfo *actor = NULL)
{
    if(!isdedicatedserver() || ci->state.aitype!=AI_NONE) return;
    if(actor) logoutf("rename: %s (%d) is now known as %s by %s (%d)", ci->name, ci->clientnum, name, actor->name, actor->clientnum);
    else logoutf("rename: %s (%d) is now known as %s", ci->name, ci->clientnum, name);
}

static inline void z_log_servcmd(clientinfo *ci, const char *cmd)
{
    logoutf("servcmd: %s (%d): %s", ci->name, ci->clientnum, cmd);
}

VAR(discmsg_privacy, 0, 1, 1);          // avoid broadcasting clients' ips to unauthorized clients
VAR(discmsg_verbose, 0, 0, 2);          // (only applies for not connected clients) whether show disc msg (2 - force)
VAR(discmsg_showip_admin, -1, -1, 1);   // whether we should treat master or admin as authorized client (-1 - depending on auth mode)
VAR(discmsg_showip_kicker, 0, 0, 1);    // whether we should always treat kicker as authorized to see regardless of priv

static void z_discmsg_print(char (&s)[MAXSTRLEN], clientinfo *ci, int n, const char *msg, bool hideip)
{
    if(ci) {
        if(hideip) {
            if(msg) formatstring(s, "client %s disconnected because: %s", colorname(ci), msg);
            else formatstring(s, "client %s disconnected", colorname(ci));
        } else {
            if(msg) formatstring(s, "client %s (%s) disconnected because: %s", colorname(ci), getclienthostname(n), msg);
            else formatstring(s, "client %s (%s) disconnected", colorname(ci), getclienthostname(n));
        }
    } else {
        if(hideip) {
            if(msg) formatstring(s, "client disconnected because: %s", msg);
            else copystring(s, "client disconnected");
        } else {
            if(msg) formatstring(s, "client (%s) disconnected because: %s", getclienthostname(n), msg);
            else formatstring(s, "client (%s) disconnected", getclienthostname(n));
        }
    }
}

static void z_discmsg_recordmsg(const char *msg)
{
    vector<uchar> buf;
    putint(buf, N_SERVMSG);
    sendstring(msg, buf);
    recordpacket(1, buf.getbuf(), buf.length());
}

static int z_discmsg_showip_priv()
{
    switch (discmsg_showip_admin)
    {
        case -1: return mastermask&MM_AUTOAPPROVE ? PRIV_ADMIN : PRIV_MASTER;
        case 0: return PRIV_MASTER;
        case 1: return PRIV_ADMIN;
    }
    assert(0 && "shouldn't be reached");
}

static bool z_discmsg_canseeip(clientinfo *ci)
{
    if(ci == z_log_kickerinfo.ci) return discmsg_showip_kicker || ci->local || z_log_kickerinfo.priv>=(discmsg_showip_admin ? PRIV_ADMIN : PRIV_MASTER);
    return ci->local || ci->privilege >= z_discmsg_showip_priv();
}

static void z_discmsg(clientinfo *ci, int n, const char *msg, bool forced)
{
    string s;
    if(!forced) return;
    if(ci->connected)   // always display disconnect messages if client was connected
    {
        if(!discmsg_privacy)
        {
            z_discmsg_print(s, ci, n, msg, false);
            sendservmsg(s);
        }
        else
        {
            string sp;
            *s = *sp = 0;
            loopv(clients) if(clients[i]->state.aitype==AI_NONE)
            {
                if(z_discmsg_canseeip(clients[i]))
                {
                    if(!*s) z_discmsg_print(s, ci, n, msg, false);
                    sendf(clients[i]->clientnum, 1, "ris", N_SERVMSG, s);
                }
                else
                {
                    if(!*sp) z_discmsg_print(sp, ci, n, msg, true);
                    sendf(clients[i]->clientnum, 1, "ris", N_SERVMSG, sp);
                }
            }
            if(demorecord)
            {
                if(!*sp) z_discmsg_print(sp, ci, n, msg, true);
                z_discmsg_recordmsg(sp);
            }
        }
    }
    else if(discmsg_verbose)
    {
        if(!discmsg_privacy)
        {
            z_discmsg_print(s, NULL, n, msg, false);
            sendservmsg(s);
        }
        else
        {
            string sp;
            *s = *sp = 0;
            loopv(clients) if(clients[i]->state.aitype==AI_NONE)
            {
                if(z_discmsg_canseeip(clients[i]))
                {
                    if(!*s) z_discmsg_print(s, NULL, n, msg, false);
                    sendf(clients[i]->clientnum, 1, "ris", N_SERVMSG, s);
                }
                else if(discmsg_verbose>=2)
                {
                    if(!*sp) z_discmsg_print(sp, NULL, n, msg, true);
                    sendf(clients[i]->clientnum, 1, "ris", N_SERVMSG, sp);
                }
            }
            if(demorecord && discmsg_verbose>=2)
            {
                if(!*sp) z_discmsg_print(sp, NULL, n, msg, true);
                z_discmsg_recordmsg(sp);
            }
        }
    }
}

static void z_log_spectate(clientinfo *actor, clientinfo *wi, bool val)
{
    logoutf("spectator: %s (%d) %s %s (%d)", actor->name, actor->clientnum, val ? "spectated" : "unspectated", wi->name, wi->clientnum);
}

static void z_log_setteam(clientinfo *actor, clientinfo *wi, const char *newteam)
{
    logoutf("setteam: %s (%d) forced %s (%d) to team %s", actor->name, actor->clientnum, wi->name, wi->clientnum, newteam);
}
