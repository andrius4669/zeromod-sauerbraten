#ifdef Z_BANS_H
#error "already z_bans.h"
#endif
#define Z_BANS_H

#ifndef Z_FORMAT_H
#error "want z_format.h"
#endif
#ifndef Z_SERVERCOMMANDS_H
#error "want z_servercommands.h"
#endif

VAR(serverautounban, -1, 1, 1); // -1 = depends whether authed mode, 0 - off, 1 - on

void z_clearbans(bool force)
{
    force = force || (serverautounban >= 0 ? serverautounban!=0 : mastermask&MM_AUTOAPPROVE);
    if(force) bannedips.shrink(0);
    else
    {
        loopvrev(bannedips) if(bannedips[i].type==BAN_TEAMKILL) bannedips.remove(i);
    }
}

VARF(teamkillspectate, 0, 0, 1,
{
    if(!teamkillspectate) loopv(bannedips) if(bannedips[i].type==BAN_TEAMKILL)
    {
        uint ip = bannedips[i].ip;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.aitype != AI_NONE || ci->privilege >= PRIV_MASTER || ci->local) continue;
            if(getclientip(ci->clientnum) == ip) disconnect_client(ci->clientnum, DISC_IPBAN);
        }
    }
});

SVAR(ban_message_teamkillspectate, "you got spectated because teamkills limit was reached");

void z_kickteamkillers(uint ip)
{
    if(!teamkillspectate) kickclients(ip);
    else
    {
        loopv(clients) if(clients[i]->state.aitype==AI_NONE && clients[i]->state.state!=CS_SPECTATOR && getclientip(clients[i]->clientnum)==ip)
        {
            if(clients[i]->local || clients[i]->privilege >= PRIV_MASTER) continue;
            forcespectator(clients[i]);
            sendf(clients[i]->clientnum, 1, "ris", N_SERVMSG, ban_message_teamkillspectate);
        }
    }
}

SVAR(ban_message_teamkillban, "ip you are connecting from is banned because of teamkills");
SVAR(ban_message_kickban, "");
SVAR(ban_message_kickbanreason, "ip you are connecting from is banned because: %r");

bool z_checkban(uint ip, clientinfo *ci)
{
    loopv(bannedips) if(bannedips[i].ip==ip) switch(bannedips[i].type)
    {
        case BAN_TEAMKILL:
            if(teamkillspectate) continue;
            // FALL THROUGH
        case BAN_KICK:
            if(showbanreason)
            {
                if(bannedips[i].type==BAN_TEAMKILL) ci->xi.setdiscreason(ban_message_teamkillban);
                else if(bannedips[i].reason)
                {
                    string buf;
                    z_formattemplate ft[] =
                    {
                        { 'r', "%s", bannedips[i].reason },
                        { 0, 0, 0 }
                    };
                    z_format(buf, sizeof(buf), ban_message_kickbanreason, ft);
                    if(*buf) ci->xi.setdiscreason(buf);
                }
                else if(*ban_message_kickban) ci->xi.setdiscreason(ban_message_kickban);
            }
            return true;

        case BAN_MUTE:
            ci->xi.chatmute = 2;
            continue;

        default:
            continue;
    }
    return false;
}

static inline bool z_checkmuteban(uint ip)
{
    loopv(bannedips) if(bannedips[i].ip==ip && bannedips[i].type == BAN_MUTE) return true;
    return false;
}

static bool z_checkchatmute(clientinfo *ci, clientinfo *cq = NULL)
{
    if(!ci->xi.chatmute && (!cq || !cq->xi.chatmute)) return false;         // not chatmuted at all
    if(ci->xi.chatmute == 1 || (cq && cq->xi.chatmute == 1)) return true;   // chatmuted
    // 2 = possible muteban
    if(z_checkmuteban(getclientip(ci->clientnum))) return true;             // yep, it is still there
    // nope, mark client as entirelly unmuted
    ci->xi.chatmute = 0;
    if(cq) cq->xi.chatmute = 0;
    return false;
}

SVAR(ban_message_specreason, "you are spectated because of %t ban");

bool z_applyspecban(clientinfo *ci, bool connecting = false, bool quiet = false)
{
    if(ci->local) return false;
    uint ip = getclientip(ci->clientnum);
    // check against regular bans
    loopv(bannedips) if(bannedips[i].ip==ip && (bannedips[i].type==BAN_SPECTATE || (teamkillspectate && bannedips[i].type==BAN_TEAMKILL)))
    {
        if(!quiet)
        {
            ban &b = bannedips[i];
            string tmp;
            z_formattemplate ft[] =
            {
                { 't', "%s", b.type==BAN_TEAMKILL ? "teamkills" : "spectate" },
                { 0, 0, 0 }
            };
            z_format(tmp, sizeof(tmp), ban_message_specreason, ft);
            if(*tmp) sendf(ci->clientnum, 1, "ris", N_SERVMSG, tmp);
        }
        return true;
    }
    // check against global bans
    gbaninfo *p;
    uint hip = ENET_NET_TO_HOST_32(ip);
    loopv(gbans)
    {
        const char *ident, *wlauth, *banmsg;
        int mode;
        if(!getmasterbaninfo(i, ident, mode, wlauth, banmsg) || mode != 2) continue;
        if(wlauth && ci->xi.ident.isset() && !strcmp(ci->xi.ident.desc, wlauth)) continue;
        if((p = gbans[i].check(hip)))
        {
            if(!quiet)
            {
                // message gonna be cleared by N_WELCOME anyway
                if(!connecting && banmsg && banmsg[0]) sendf(ci->clientnum, 1, "ris", N_SERVMSG, banmsg);
                if(wlauth && ci->authmaster < 0 && !ci->authchallenge)
                {
                    // we haven't yet sent N_WELCOME at this point
                    if(connecting) sendf(ci->clientnum, 1, "ri", N_WELCOME);
                    sendf(ci->clientnum, 1, "ris", N_REQAUTH, wlauth);
                }
            }
            return true;
        }
    }
    return false;
}

static const char *z_bantypestr(int type)
{
    switch(type)
    {
        case BAN_KICK: return "kick";
        case BAN_TEAMKILL: return "teamkill";
        case BAN_MUTE: return "mute";
        case BAN_SPECTATE: return "spectate";
        default: return "unknown";
    }
}

static void z_sendbanlist(int cn)
{
    string buf;
    vector<char> msgbuf;
    ipmask im;
    im.mask = ~0;
    int n;
    sendf(cn, 1, "ris", N_SERVMSG, bannedips.empty() ? "banlist is empty" : "banlist:");
    loopv(bannedips)
    {
        msgbuf.setsize(0);
        im.ip = bannedips[i].ip;
        n = sprintf(buf, "\f2id: \f7%2d\f2, ip: \f7", i);
        msgbuf.put(buf, n);
        n = im.print(buf);
        msgbuf.put(buf, n);
        n = sprintf(buf, "\f2, type: \f7%s\f2, expires in: \f7", z_bantypestr(bannedips[i].type));
        msgbuf.put(buf, n);
        z_formatsecs(msgbuf, (uint)((bannedips[i].expire-totalmillis)/1000));
        if(bannedips[i].reason)
        {
            n = snprintf(buf, sizeof(buf), "\f2, reason: \f7%s", bannedips[i].reason);
            msgbuf.put(buf, clamp(n, 0, int(sizeof(buf)-1)));
        }
        msgbuf.add(0);
        sendf(cn, 1, "ris", N_SERVMSG, msgbuf.getbuf());
    }
}

void z_servcmd_listbans(int argc, char **argv, int sender)
{
    z_sendbanlist(sender);
}
SCOMMANDA(listbans, PRIV_MASTER, z_servcmd_listbans, 1);

static int z_readbantime(char *str)
{
    char *end = str;
    int i, s = 0;
    while(*str)
    {
        i = strtol(str, &end, 10);
        if(end <= str) i = 1;
        if(*end)
        {
            if(*end == '-')
            {
                i = -1;
                if(!*++end) return s;
            }
            switch(*end++)
            {
                case 's': case 'S': i *= 1000; break;
                case 'm': case 'M': i *= 60000; break;
                case 'h': case 'H': i *= 60*60000; break;
                case 'd': case 'D': i *= 24*60*60000; break;
                case 'w': case 'W': i *= 7*24*60*60000; break;
                default: return -1;
            }
        }
        else i *= 60000;    // minutes by default
        s += i;
        str = end;
    }
    return s;
}

static const char * const z_banstrings[] = { "ban", "specban", "muteban" };

void z_servcmd_ban(int argc, char **argv, int sender)
{
    clientinfo *target, *ci = (clientinfo *)getclientinfo(sender);
    int bant, cn, time;
    if(argc <= 1) { z_servcmd_pleasespecifyclient(sender); return; }
    if(!strcasecmp(argv[0], z_banstrings[0]))
    {
        bant = 0;
        if(!z_parseclient_verify(argv[1], cn, false))
        {
            z_servcmd_unknownclient(argv[1], sender);
            return;
        }
        target = (clientinfo *)getclientinfo(cn);
        if(target->local || cn == sender || (!ci->local && (target->privilege >= PRIV_ADMIN || target->privilege > ci->privilege)))
        {
            sendf(sender, 1, "ris", N_SERVMSG, "cannot ban specified client");
            return;
        }
    }
    else if(!strcasecmp(argv[0], z_banstrings[1]))
    {
        bant = 1;
        if(!z_parseclient_verify(argv[1], cn, false))
        {
            z_servcmd_unknownclient(argv[1], sender);
            return;
        }
        target = (clientinfo *)getclientinfo(cn);
        if(target->local || target == ci || target->privilege >= PRIV_MASTER)
        {
            sendf(sender, 1, "ris", N_SERVMSG, "cannot specban specified client");
            return;
        }
    }
    else if(!strcasecmp(argv[0], z_banstrings[2]))
    {
        bant = 2;
        if(!z_parseclient_verify(argv[1], cn, false, true))
        {
            z_servcmd_unknownclient(argv[1], sender);
            return;
        }
        target = getinfo(cn);
        if(target->state.aitype != AI_NONE)
        {
            cn = target->ownernum;
            target = (clientinfo *)getclientinfo(cn);
        }
        if(!target || target->local)
        {
            sendf(sender, 1, "ris", N_SERVMSG, "cannot muteban specified client");
            return;
        }
    }
    else abort();

    if(argc > 2)
    {
        time = z_readbantime(argv[2]);
        if(time <= 0)
        {
            sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("incorrect ban length: %s", argv[2]));
            return;
        }
    }
    else time = 4*60*60000;

    const char *reason = argc > 3 ? argv[3] : NULL;
    z_showban(ci, z_banstrings[bant], colorname(target), time, reason);

    uint ip = getclientip(cn);
    switch(bant)
    {
        case 0:
            z_log_kick(ci, NULL, NULL, ci->privilege, target, reason);
            addban(ip, time, BAN_KICK, reason);
            kickclients(ip, ci, ci->privilege);
            z_log_kickdone();
            break;

        case 1:
            addban(ip, time, BAN_SPECTATE, reason);
            loopv(clients)
            {
                clientinfo &c = *clients[i];
                if(c.local || c.state.aitype != AI_NONE) continue;
                if(ip == getclientip(c.clientnum))
                {
                    if(c.state.state != CS_SPECTATOR && c.privilege < PRIV_MASTER && c.clientnum != sender) forcespectator(&c);
                }
            }
            break;

        case 2:
            addban(ip, time, BAN_MUTE, reason);
            loopv(clients)
            {
                clientinfo &c = *clients[i];
                if(c.local || c.state.aitype != AI_NONE) continue;
                if(ip == getclientip(c.clientnum) && c.xi.chatmute < 1) c.xi.chatmute = 2;
            }
            break;
    }
}
SCOMMANDA(ban, PRIV_AUTH, z_servcmd_ban, 3);
SCOMMANDA(specban, PRIV_AUTH, z_servcmd_ban, 3);
SCOMMANDA(muteban, PRIV_AUTH, z_servcmd_ban, 3);

void z_servcmd_kick(int argc, char **argv, int sender)
{
    clientinfo *target, *ci = (clientinfo *)getclientinfo(sender);
    int cn;
    if(argc <= 1) { z_servcmd_pleasespecifyclient(sender); return; }

    target = z_parseclient_return(argv[1]);
    if(!target)
    {
        z_servcmd_unknownclient(argv[1], sender);
        return;
    }
    cn = target->clientnum;

    if(target->local || cn == sender || (!ci->local && (target->privilege >= PRIV_ADMIN || target->privilege > ci->privilege)))
    {
        sendf(sender, 1, "ris", N_SERVMSG, "cannot kick specified client");
        return;
    }

    const char *reason = argc > 2 ? argv[2] : NULL;
    z_showkick(colorname(ci), ci, target, reason);
    z_log_kick(ci, NULL, NULL, ci->privilege, target, reason);
    disconnect_client(cn, DISC_KICK);
    z_log_kickdone();
}
SCOMMANDA(kick, PRIV_AUTH, z_servcmd_kick, 2);
SCOMMANDAH(disc, PRIV_AUTH, z_servcmd_kick, 2);

void z_servcmd_ipban(int argc, char **argv, int sender)
{
    if(argc <= 1)
    {
        sendf(sender, 1, "ris", N_SERVMSG, "please specify IP address");
        return;
    }
    ipmask im;
    im.parse(argv[1]);
    if(im.mask != 0xFFffFFff)
    {
        sendf(sender, 1, "ris", N_SERVMSG, "invalid IP address");
        return;
    }

    int bant = -1;
    string banstr;
    copystring(banstr, argv[0]);
    banstr[strlen(banstr)-2] = '\0'; // cut 'ip' ending
    loopi(sizeof(z_banstrings)/sizeof(z_banstrings[0]))
    {
        if(!strcasecmp(banstr, z_banstrings[i])) { bant = i; break; }
    }
    if(bant < 0 || bant > 2) abort();

    int time;
    if(argc > 2)
    {
        time = z_readbantime(argv[2]);
        if(time <= 0)
        {
            sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("incorrect ban length: %s", argv[2]));
            return;
        }
    }
    else time = 4*60*60000;

    const char *reason = argc > 3 ? argv[3] : NULL;

    switch(bant)
    {
        case 0:
        {
            addban(im.ip, time, BAN_KICK, reason);
            clientinfo *ci = (clientinfo *)getclientinfo(sender);
            kickclients(im.ip, ci, ci ? ci->privilege : PRIV_NONE);
            break;
        }

        case 1:
            addban(im.ip, time, BAN_SPECTATE, reason);
            loopv(clients)
            {
                clientinfo &c = *clients[i];
                if(c.local || c.state.aitype != AI_NONE) continue;
                if(im.ip == getclientip(c.clientnum))
                {
                    if(c.state.state != CS_SPECTATOR && c.privilege < PRIV_MASTER && c.clientnum != sender) forcespectator(&c);
                }
            }
            break;

        case 2:
            addban(im.ip, time, BAN_MUTE, reason);
            loopv(clients)
            {
                clientinfo &c = *clients[i];
                if(c.local || c.state.aitype != AI_NONE) continue;
                if(im.ip == getclientip(c.clientnum) && c.xi.chatmute < 1) c.xi.chatmute = 2;
            }
            break;
    }
}
SCOMMANDA(banip, PRIV_ADMIN, z_servcmd_ipban, 3);
SCOMMANDA(specbanip, PRIV_ADMIN, z_servcmd_ipban, 3);
SCOMMANDA(mutebanip, PRIV_ADMIN, z_servcmd_ipban, 3);

void z_servcmd_unban(int argc, char **argv, int sender)
{
    if(argc <= 1)
    {
        sendf(sender, 1, "ris", N_SERVMSG, "please specify ban id");
        return;
    }
    char *end = NULL;
    int id = (int)strtol(argv[1], &end, 10);
    if(end <= argv[1] || *end || (id >= 0 && !bannedips.inrange(id)))
    {
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("bad ban id: %s", argv[1]));
        return;
    }
    if(id >= 0)
    {
        bannedips.remove(id);
        sendf(sender, 1, "ris", N_SERVMSG, "ban removed");
    }
    else
    {
        bannedips.shrink(0);
        sendservmsg("cleared all bans");
    }
}
SCOMMANDA(unban, PRIV_MASTER, z_servcmd_unban, 1);

void z_servcmd_unbanlast(int argc, char **argv, int sender)
{
    if(bannedips.empty()) { sendf(sender, 1, "ris", N_SERVMSG, "banlist is empty"); return; }
    int last = -1, lastelapsed = 0;
    loopvrev(bannedips)
    {
        int elapsed = totalmillis - bannedips[i].time;
        if(last < 0 || elapsed < lastelapsed)
        {
            last = i;
            lastelapsed = elapsed;
        }
    }
    bannedips.remove(last);
    sendf(sender, 1, "ris", N_SERVMSG, "last ban removed");
}
SCOMMANDA(unbanlast, PRIV_MASTER, z_servcmd_unbanlast, 1);
