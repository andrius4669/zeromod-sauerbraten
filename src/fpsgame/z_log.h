#ifndef Z_LOG_H
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
        if(ci==actor || ci->local || ci->privilege>=PRIV_ADMIN) sendpacket(ci->clientnum, 1, adminpack.packet);
        else sendpacket(ci->clientnum, 1, normalpack.packet);
    }
}

static inline void z_showkick(const char *kicker, clientinfo *actor, clientinfo *vinfo, const char *reason)
{
    const char *kickstr = reason && reason[0]
        ? tempformatstring("%s kicked %s because: %s", kicker, colorname(vinfo), reason)
        : tempformatstring("%s kicked %s", kicker, colorname(vinfo));
    if(!actor->spy) { sendservmsg(kickstr); return; }
    // if kicker is spy, don't show his name to normal clients
    const char *spykickstr = reason && reason[0]
        ? tempformatstring("%s was kicked because: %s", colorname(vinfo), reason)
        : tempformatstring("%s was kicked", colorname(vinfo));
    // allocate packetbufs for messages
    packetbuf kickpack(MAXTRANS, ENET_PACKET_FLAG_RELIABLE), spykickpack(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    // fill in packetbuf for admins
    putint(kickpack, N_SERVMSG);
    sendstring(kickstr, kickpack);
    kickpack.finalize();
    // packetbuf for normal clients (don't show who kicked)
    putint(spykickpack, N_SERVMSG);
    sendstring(spykickstr, spykickpack);
    spykickpack.finalize();
    // distribute messages
    z_sendmsgpacks(kickpack, spykickpack, actor);
}

void z_showban(clientinfo *actor, const char *banstr, const char *victim, int bantime, const char *reason)
{
    const char *actorname = colorname(actor);
    vector<char> timebuf;
    z_formatsecs(timebuf, (uint)(bantime/1000));
    timebuf.add(0);
    const char *adminstr = reason && reason[0]
        ? tempformatstring("%s %sned %s for %s because: %s", actorname, banstr, victim, timebuf.getbuf(), reason)
        : tempformatstring("%s %sned %s for %s", actorname, banstr, victim, timebuf.getbuf());
    if(!actor->spy)
    {
        sendservmsg(adminstr);
        return;
    }
    const char *normalstr = reason && reason[0]
        ? tempformatstring("%s was %sned for %s because: %s", victim, banstr, timebuf.getbuf(), reason)
        : tempformatstring("%s was %sned for %s", victim, banstr, timebuf.getbuf());
    // allocate packetbufs for messages
    packetbuf adminpack(MAXTRANS, ENET_PACKET_FLAG_RELIABLE), normalpack(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    // fill in packetbuf for admins
    putint(adminpack, N_SERVMSG);
    sendstring(adminstr, adminpack);
    adminpack.finalize();
    // packetbuf for normal clients (don't show who kicked)
    putint(normalpack, N_SERVMSG);
    sendstring(normalstr, normalpack);
    normalpack.finalize();
    // distribute messages
    z_sendmsgpacks(adminpack, normalpack, actor);
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

VAR(discmsg_privacy, 0, 0, 1);          // avoid broadcasting clients' ips to unauthorized clients
VAR(discmsg_verbose, 0, 1, 2);          // (only applies for not connected clients) whether show disc msg (2 - force)
VAR(discmsg_showip_admin, 0, 1, 1);     // whether we should treat master or admin as authorized client
VAR(discmsg_showip_kicker, 0, 0, 1);    // whether we should treat kicker specially

static void z_discmsg_print(char (&s)[MAXSTRLEN], clientinfo *ci, int n, const char *msg, bool hideip)
{
    if(ci) {
        if(hideip) {
            if(msg) formatstring(s)("client %s disconnected because: %s", colorname(ci), msg);
            else formatstring(s)("client %s disconnected", colorname(ci));
        } else {
            if(msg) formatstring(s)("client %s (%s) disconnected because: %s", colorname(ci), getclienthostname(n), msg);
            else formatstring(s)("client %s (%s) disconnected", colorname(ci), getclienthostname(n));
        }
    } else {
        if(hideip) {
            if(msg) formatstring(s)("client disconnected because: %s", msg);
            else copystring(s, "client disconnected");
        } else {
            if(msg) formatstring(s)("client (%s) disconnected because: %s", getclienthostname(n), msg);
            else formatstring(s)("client (%s) disconnected", getclienthostname(n));
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

static bool z_discmsg_canseeip(clientinfo *ci)
{
    if(ci == z_log_kickerinfo.ci) return discmsg_showip_kicker || ci->local || z_log_kickerinfo.priv>=(discmsg_showip_admin ? PRIV_ADMIN : PRIV_MASTER);
    return ci->local || ci->privilege>=(discmsg_showip_admin ? PRIV_ADMIN : PRIV_MASTER);
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

#endif // Z_LOG_H
