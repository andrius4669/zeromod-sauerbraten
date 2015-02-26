#ifndef Z_BANS_H
#define Z_BANS_H

#include "z_format.h"
#include "z_servercommands.h"

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

void z_kickteamkillers(uint ip)
{
    if(!teamkillspectate) kickclients(ip);
    else
    {
        loopv(clients) if(clients[i]->state.aitype==AI_NONE && clients[i]->state.state!=CS_SPECTATOR && getclientip(clients[i]->clientnum)==ip)
        {
            if(clients[i]->local || clients[i]->privilege >= PRIV_MASTER) continue;
            extern void forcespectator(clientinfo *);
            forcespectator(clients[i]);
            sendf(clients[i]->clientnum, 1, "ris", N_SERVMSG, "you got spectated because teamkills limit was reached");
        }
    }
}

bool z_checkban(uint ip, clientinfo *ci)
{
    extern int showbanreason;
    loopv(bannedips) if(bannedips[i].ip==ip) switch(bannedips[i].type)
    {
        case BAN_TEAMKILL:
            if(teamkillspectate) continue;
            // FALL THROUGH
        case BAN_KICK:
            if(showbanreason)
            {
                if(bannedips[i].type==BAN_TEAMKILL) ci->xi.setdiscreason("ip you are connecting from is banned because of teamkills");
                else if(bannedips[i].reason) ci->xi.setdiscreason(tempformatstring("ip you are connecting from is banned because: %s", bannedips[i].reason));
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

bool z_applyspecban(clientinfo *ci)
{
    if(ci->local) return false;
    uint ip = getclientip(ci->clientnum);
    loopv(bannedips) if(bannedips[i].ip==ip && (bannedips[i].type==BAN_SPECTATE || (teamkillspectate && bannedips[i].type==BAN_TEAMKILL)))
    {
        ban &b = bannedips[i];
        sendf(ci->clientnum, 1, "ris", N_SERVMSG, tempformatstring("you are spectated because of %s ban", b.type==BAN_TEAMKILL ? "teamkills" : "spectate"));
        return true;
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
        n = sprintf(buf, "\f2id: \f7%d\f2, ip: \f7", i);
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

int z_readbantime(char *str)
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

void z_servcmd_ban(int argc, char **argv, int sender)
{
    extern void z_log_kick(clientinfo *, const char *, const char *, int, clientinfo *, const char *);
    extern void z_log_kickdone();
    extern void z_showban(clientinfo *, const char *, const char *, int, const char *);
    extern bool z_parseclient_verify(const char *str, int *cn, bool allowall, bool allowbot = false, bool allowspy = false);
    extern clientinfo *getinfo(int n);
    extern const char *colorname(clientinfo *ci, const char *name = NULL);
    extern void forcespectator(clientinfo *ci);

    static const char * const banstrings[] = { "ban", "specban", "muteban" };
    clientinfo *target, *ci = (clientinfo *)getclientinfo(sender);
    int bant, cn, time;
    if(argc <= 1) { sendf(sender, 1, "ris", N_SERVMSG, "please specify client number"); return; }
    if(!strcasecmp(argv[0], banstrings[0]))
    {
        bant = 0;
        if(!z_parseclient_verify(argv[1], &cn, false))
        {
            sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("unknown client: %s", argv[1]));
            return;
        }
        target = (clientinfo *)getclientinfo(cn);
        if(target->local || cn == sender || (!ci->local && (target->privilege >= PRIV_ADMIN || target->privilege > ci->privilege)))
        {
            sendf(sender, 1, "ris", N_SERVMSG, "cannot ban specified client");
            return;
        }
    }
    else if(!strcasecmp(argv[0], banstrings[1]))
    {
        bant = 1;
        if(!z_parseclient_verify(argv[1], &cn, false))
        {
            sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("unknown client: %s", argv[1]));
            return;
        }
        target = (clientinfo *)getclientinfo(cn);
        if(target->local || target == ci || target->privilege >= PRIV_MASTER)
        {
            sendf(sender, 1, "ris", N_SERVMSG, "cannot specban specified client");
            return;
        }
    }
    else if(!strcasecmp(argv[0], banstrings[2]))
    {
        bant = 2;
        if(!z_parseclient_verify(argv[1], &cn, false, true))
        {
            sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("unknown client: %s", argv[1]));
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
    else return;

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
    z_showban(ci, banstrings[bant], colorname(target), time, reason);

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
SCOMMANDAH(delban, PRIV_MASTER, z_servcmd_unban, 1);

#endif // Z_BANS_H
