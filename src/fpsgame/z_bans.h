#ifndef Z_BANS_H
#define Z_BANS_H

#include "z_format.h"
#include "z_servercommands.h"

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
            ci->xi.chatmute = true;
            continue;

        default:
            continue;
    }
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

#endif // Z_BANS_H
