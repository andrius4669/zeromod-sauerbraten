#ifndef Z_BANS_H
#define Z_BANS_H

void z_teamkillspectate(uint ip)
{
    loopv(clients) if(clients[i]->state.aitype==AI_NONE && clients[i]->state.state!=CS_SPECTATOR && getclientip(clients[i]->clientnum)==ip)
    {
        if(clients[i]->local || clients[i]->privilege>=PRIV_MASTER) continue;
        extern void forcespectator(clientinfo *);
        forcespectator(clients[i]);
        sendf(clients[i]->clientnum, 1, "ris", N_SERVMSG, "you got spectated because teamkills limit was reached");
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

#endif // Z_BANS_H