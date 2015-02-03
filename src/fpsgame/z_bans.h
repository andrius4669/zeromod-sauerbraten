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

static void z_printbantime(vector<char> &buf, int millisecs)
{
    const char *s;
    int secs = millisecs / 1000;
    int days = secs / 24*60*60;
    secs %= 24*60*60;
    int hours = secs / 60*60;
    secs %= 60*60;
    int mins = secs / 60;
    secs %= 60;
    if(days)
    {
        s = tempformatstring("%d day%s", days, days > 1 ? "s" : "");
        buf.put(s, strlen(s));
    }
    if(hours)
    {
        if(buf.length()) buf.add(' ');
        s = tempformatstring("%d hour%s", hours, hours > 1 ? "s" : "");
        buf.put(s, strlen(s));
    }
    if(mins)
    {
        if(buf.length()) buf.add(' ');
        s = tempformatstring("%d minute%s", mins, mins > 1 ? "s" : "");
        buf.put(s, strlen(s));
    }
    if(secs)
    {
        if(buf.length()) buf.add(' ');
        s = tempformatstring("%d second%s", secs, secs > 1 ? "s" : "");
        buf.put(s, strlen(s));
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
                vector<char> buf;
                z_printbantime(buf, bannedips[i].expire-totalmillis);
                if(buf.length())
                {
                    buf.add(0);
                    const char *r;
                    if(bannedips[i].type==BAN_TEAMKILL)
                    {
                        r = tempformatstring("ip you are connecting from is banned because of teamkills; ban will expire after %s", buf.getbuf());
                    }
                    else
                    {
                        r = bannedips[i].reason
                            ? tempformatstring("ip you are connecting from is banned (%s); ban will expire after %s", bannedips[i].reason, buf.getbuf())
                            : tempformatstring("ip you are connecting from is banned; ban will expire after %s", buf.getbuf());
                    }
                    ci->xi.setdiscreason(r);
                }
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