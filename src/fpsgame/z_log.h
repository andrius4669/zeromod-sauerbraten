#ifndef Z_LOG_H
#define Z_LOG_H

static inline
void z_log_kick(clientinfo *actor, const char *aname, const char *adesc, int apriv, clientinfo *victim, const char *reason)
{
    const char *kicker;
    if(!aname) kicker = tempformatstring("%s (%d)", actor->name, actor->clientnum);
    else
    {
        if(adesc && adesc[0]) kicker = tempformatstring("%s (%d) as '%s' [%s] (%s)", actor->name, actor->clientnum, aname, adesc, privname(apriv));
        else kicker = tempformatstring("%s (%d) as '%s' (%s)", actor->name, actor->clientnum, aname, privname(apriv));
    }
    if(reason && *reason) logoutf("kick: %s kicked %s (%d) because: %s", kicker, victim->name, victim->clientnum, reason);
    else logoutf("kick: %s kicked %s (%d)", kicker, victim->name, victim->clientnum);
}

static inline
void z_log_setmaster(clientinfo *master, bool val, bool pass, const char *aname, const char *adesc, const char *priv, clientinfo *by)
{
    const char *mode;
    if(by) mode = tempformatstring("%s by %s (%d)", val ? "passed" : "taken", by->name, by->clientnum); // someone else used /setmaster
    else if(aname) mode = "auth";                   // auth system
    else if(pass) mode = "password";     // password claim
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

VAR(discmsg_privacy, 0, 0, 1);  // avoid broadcasting clients' ips
VAR(discmsg_verbose, 0, 1, 2);

template<size_t N>
static void z_discmsg_print(char (&s)[N], clientinfo *ci, int n, const char *msg, bool hideip)
{
    if(ci)
    {
        if(hideip)
        {
            if(msg) formatstring(s)("client %s disconnected because: %s", colorname(ci), msg);
            else formatstring(s)("client %s disconnected", colorname(ci));
        }
        else
        {
            if(msg) formatstring(s)("client %s (%s) disconnected because: %s", colorname(ci), getclienthostname(n), msg);
            else formatstring(s)("client %s (%s) disconnected", colorname(ci), getclienthostname(n));
        }
    }
    else
    {
        if(hideip)
        {
            if(msg) formatstring(s)("client disconnected because: %s", msg);
            else copystring(s, "client disconnected");
        }
        else
        {
            if(msg) formatstring(s)("client (%s) disconnected because: %s", getclienthostname(n), msg);
            else formatstring(s)("client (%s) disconnected", getclienthostname(n));
        }
    }
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
                if(clients[i]->local || clients[i]->privilege>=PRIV_ADMIN)
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
                if(clients[i]->local || clients[i]->privilege>=PRIV_ADMIN)
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
        }
    }
}

#endif // Z_LOG_H
