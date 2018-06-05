
#define EXT_ACK                         -1
#define EXT_VERSION                     105
#define EXT_NO_ERROR                    0
#define EXT_ERROR                       1
#define EXT_PLAYERSTATS_RESP_IDS        -10
#define EXT_PLAYERSTATS_RESP_STATS      -11
#define EXT_UPTIME                      0
#define EXT_PLAYERSTATS                 1
#define EXT_TEAMSCORE                   2
#define EXT_SERVERMOD                   -8

/*
    Client:
    -----
    A: 0 EXT_UPTIME
    B: 0 EXT_PLAYERSTATS cn #a client number or -1 for all players#
    C: 0 EXT_TEAMSCORE

    Server:  
    --------
    A: 0 EXT_UPTIME EXT_ACK EXT_VERSION uptime #in seconds#
    B: 0 EXT_PLAYERSTATS cn #send by client# EXT_ACK EXT_VERSION 0 or 1 #error, if cn was > -1 and client does not exist# ...
         EXT_PLAYERSTATS_RESP_IDS pid(s) #1 packet#
         EXT_PLAYERSTATS_RESP_STATS pid playerdata #1 packet for each player#
    C: 0 EXT_TEAMSCORE EXT_ACK EXT_VERSION 0 or 1 #error, no teammode# remaining_time gamemode loop(teamdata [numbases bases] or -1)

    Errors:
    --------------
    B:C:default: 0 command EXT_ACK EXT_VERSION EXT_ERROR
*/

    VAR(extinfoip, 0, 0, 1);

    VAR(extinfo_enable, 0, 1, 1);           // enable extinfo functionality
    VAR(extinfo_showname, 0, 1, 1);         // show names of clients
    VAR(extinfo_showpriv, 0, 1, 2);         // show privileges of clients
    VAR(extinfo_showspy, 0, 0, 1);          // show spy clients
    VAR(extinfo_ident, 0, 0, 1);            // enable mod identification
    VAR(extinfo_extendedstats, 0, 1, 1);    // enable extended player stats
    VAR(extinfo_firewall, 0, 1, 1);         // only show info if client could see it by joining

    void extinfoplayer(ucharbuf &p, clientinfo *ci, bool z_extended)
    {
        ucharbuf q = p;
        putint(q, EXT_PLAYERSTATS_RESP_STATS); // send player stats following
        putint(q, ci->clientnum); //add player id
        putint(q, ci->ping);
        sendstring(extinfo_showname ? ci->name : "", q);
        sendstring(ci->team, q);
        putint(q, ci->state.frags);
        putint(q, (m_ctf || m_collect) ? ci->state.flags : 0);
        putint(q, ci->state.deaths);
        putint(q, ci->state.teamkills);
        putint(q, ci->state.damage*100/max(ci->state.shotdamage,1));
        putint(q, ci->state.health);
        putint(q, ci->state.armour);
        putint(q, ci->state.gunselect);
        putint(q, (extinfo_showpriv && (extinfo_showpriv > 1 || z_canseemypriv(ci, NULL))) ? ci->privilege : PRIV_NONE);
        putint(q, ci->state.state);
        uint ip = extinfoip ? getclientip(ci->clientnum) : 0;
        q.put((uchar*)&ip, 3);
        if(z_extended)
        {
            putint(q, -1); // for hopmod compatibility
            putint(q, EXT_SERVERMOD);
            putint(q, ci->state.suicides);
            putint(q, ci->state.shotdamage);
            putint(q, ci->state.damage);
            putint(q, ci->state.explosivedamage);
            putint(q, ci->state.hits);
            putint(q, ci->state.misses);
            putint(q, ci->state.shots);
            short gi = z_geoip_getextinfo(ci->xi.geoip);
            q.put((uchar*)&gi, 2);
            putint(q, ci->connectmillis); // player session identifier (in combination with cn), sessionid would be too sensitive
        }
        sendserverinforeply(q);
    }

    static inline void extinfoteamscore(ucharbuf &p, const char *team, int score)
    {
        sendstring(team, p);
        putint(p, score);
        if(!smode || !smode->extinfoteam(team, p))
            putint(p,-1); //no bases follow
    }

    void extinfoteams(ucharbuf &p)
    {
        putint(p, m_teammode ? 0 : 1);
        putint(p, gamemode);
        putint(p, max((gamelimit - gamemillis)/1000, 0));
        if(!m_teammode || (extinfo_firewall && !ipinfoallowed(getserverinfoip()))) return;

        vector<teamscore> scores;
        if(smode && smode->hidefrags()) smode->getteamscores(scores);
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.state!=CS_SPECTATOR && ci->team[0] && scores.htfind(ci->team) < 0)
            {
                if(smode && smode->hidefrags()) scores.add(teamscore(ci->team, 0));
                else { teaminfo *ti = teaminfos.access(ci->team); scores.add(teamscore(ci->team, ti ? ti->frags : 0)); }
            }
        }
        loopv(scores) extinfoteamscore(p, scores[i].team, scores[i].score);
    }

    void extserverinforeply(ucharbuf &req, ucharbuf &p)
    {
        if(!extinfo_enable) return;
        int extcmd = getint(req); // extended commands  

        //Build a new packet
        putint(p, EXT_ACK); //send ack
        putint(p, EXT_VERSION); //send version of extended info

        switch(extcmd)
        {
            case EXT_UPTIME:
            {
                putint(p, totalsecs); //in seconds
                if(extinfo_ident && req.remaining() && req.get() > 0) putint(p, EXT_SERVERMOD); //server mod identification
                break;
            }

            case EXT_PLAYERSTATS:
            {
                int cn = getint(req); //a special player, -1 for all
                bool z_extended = extinfo_ident && extinfo_extendedstats && req.remaining() && getint(req) > 0;
                bool z_allowed = !extinfo_firewall || ipinfoallowed(getserverinfoip());

                clientinfo *ci = NULL;
                if(cn >= 0)
                {
                    if(!z_allowed ||
                        !(ci = getinfo(cn)) ||
                        !ci->connected ||
                        (ci->spy && !extinfo_showspy))
                    {
                        putint(p, EXT_ERROR); //client requested by id was not found
                        sendserverinforeply(p);
                        return;
                    }
                }

                putint(p, EXT_NO_ERROR); //so far no error can happen anymore

                ucharbuf q = p; //remember buffer position
                putint(q, EXT_PLAYERSTATS_RESP_IDS); //send player ids following
                if(ci) putint(q, ci->clientnum);
                else if(z_allowed) loopv(clients)
                {
                    if((clients[i]->state.aitype == AI_NONE || clients[i]->state.state != CS_SPECTATOR) &&
                        (!clients[i]->spy || extinfo_showspy))
                    {
                        putint(q, clients[i]->clientnum);
                    }
                }
                sendserverinforeply(q);

                if(ci) extinfoplayer(p, ci, z_extended);
                else if(z_allowed) loopv(clients)
                {
                    if((clients[i]->state.aitype == AI_NONE || clients[i]->state.state != CS_SPECTATOR) &&
                        (!clients[i]->spy || extinfo_showspy))
                    {
                        extinfoplayer(p, clients[i], z_extended);
                    }
                }
                return;
            }

            case EXT_TEAMSCORE:
            {
                extinfoteams(p);
                break;
            }

            default:
            {
                putint(p, EXT_ERROR);
                break;
            }
        }
        sendserverinforeply(p);
    }

