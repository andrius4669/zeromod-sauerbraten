#ifdef Z_PERSISTTEAMS_H
#error "already z_persistteams.h"
#endif
#define Z_PERSISTTEAMS_H

#ifndef Z_SERVERCOMMANDS_H
#error "want z_servercommands.h"
#endif

int z_persistteams = 0;
VARFN(persistteams, defaultpersistteams, 0, 0, 2, { if(clients.empty()) z_persistteams = defaultpersistteams; });
static void z_persistteams_trigger(int type) { z_persistteams = defaultpersistteams; }
Z_TRIGGER(z_persistteams_trigger, Z_TRIGGER_STARTUP);
Z_TRIGGER(z_persistteams_trigger, Z_TRIGGER_NOCLIENTS);

void z_servcmd_persistteams(int argc, char **argv, int sender)
{
    if(argc < 2)
    {
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("persistent teams are currently %s",
                z_persistteams == 1 ? "enabled" : z_persistteams == 2 ? "enabled for custom teams" : "disabled"));
        return;
    }
    z_persistteams = clamp(atoi(argv[1]), 0, 2);
    sendservmsgf("persistent teams %s", z_persistteams == 1 ? "enabled" : z_persistteams == 2 ? "enabled for custom teams" : "disabled");
}
SCOMMANDAH(persistteams, PRIV_MASTER, z_servcmd_persistteams, 1);
SCOMMANDA(persist, PRIV_MASTER, z_servcmd_persistteams, 1);

static int z_checkstandardteam(const char *team, const char * const *teamnames)
{
    loopi(2) if(!strcmp(team, teamnames[i])) return i;
    return -1;
}

// calculates rank and sets timeplayed to -1 to disable later selection
static float z_rankplayer(clientinfo *ci)
{
    float rank = ci->state.state!=CS_SPECTATOR ? ci->state.effectiveness/max(ci->state.timeplayed, 1) : -1;
    if(smode && smode->hidefrags()) rank = 1;
    ci->state.timeplayed = -1;
    return rank;
}

static void autoteam()
{
    if(smode && smode->autoteam()) return;

    static const char * const teamnames[2] = {"good", "evil"};
    vector<clientinfo *> team[2];
    float teamrank[2] = {0, 0};
    int remaining = clients.length();

    if(z_persistteams == 1)
    {
        // XXX use smode->canchangeteam()?
        if(m_ctf || m_collect)
        {
            // need some special magic with ctf/collect modes, as they don't allow non-standard teams
            string sttnamebufs[2] = { {0}, {0} };
            char * const sttnames[2] = { sttnamebufs[0], sttnamebufs[1] };
            // pre-fill masks with standard names, if possible
            loopi(2) loopvj(clients) if(!strcmp(clients[j]->team, teamnames[i]))
            {
                copystring(sttnames[i], teamnames[i], MAXTEAMLEN+1);
                break;
            }
            loopv(clients) if(clients[i]->team[0])
            {
                clientinfo *ci = clients[i];
                int s = z_checkstandardteam(ci->team, sttnames);
                if(s < 0)
                {
                    loopi(2) if(!sttnames[i][0])
                    {
                        copystring(sttnames[i], ci->team, MAXTEAMLEN+1);
                        s = i;
                        break;
                    }
                    if(s < 0) continue;
                }
                float rank = z_rankplayer(ci);
                team[s].add(ci);
                if(rank > 0) teamrank[s] += rank;
                remaining--;
            }
        }
        else
        {
            // for everyone who have teams, just use these
            loopv(clients) if(clients[i]->team[0])
            {
                clientinfo *ci = clients[i];
                int s = z_checkstandardteam(ci->team, teamnames);
                float rank = z_rankplayer(ci);
                if(s >= 0)
                {
                    team[s].add(ci);
                    if(rank > 0) teamrank[s] += rank;
                }
                else addteaminfo(ci->team);
                remaining--;
            }
        }
    }

    // makes no sense in ctf/collect modes, as non-standard teams are not allowed
    if(z_persistteams == 2 && !m_ctf && !m_collect)
    {
        loopv(clients) if(clients[i]->team[0])
        {
            clientinfo *ci = clients[i];
            int s = z_checkstandardteam(ci->team, teamnames);
            if(s >= 0) continue;
            addteaminfo(ci->team);
            ci->state.timeplayed = -1;
            remaining--;
        }
    }

    // default/fallback shuffler for clients we didn't touch above
    for(int round = 0; remaining>=0; round++)
    {
        int first = round&1, second = (round+1)&1, selected = 0;
        while(teamrank[first] <= teamrank[second])
        {
            float rank;
            clientinfo *ci = choosebestclient(rank);
            if(!ci) break;
            if(smode && smode->hidefrags()) rank = 1;
            else if(selected && rank<=0) break;
            ci->state.timeplayed = -1;
            team[first].add(ci);
            if(rank>0) teamrank[first] += rank;
            selected++;
            if(rank<=0) break;
        }
        if(!selected) break;
        remaining -= selected;
    }

    loopi(sizeof(team)/sizeof(team[0]))
    {
        addteaminfo(teamnames[i]);
        loopvj(team[i])
        {
            clientinfo *ci = team[i][j];
            if(!strcmp(ci->team, teamnames[i])) continue;
            copystring(ci->team, teamnames[i], MAXTEAMLEN+1);
            sendf(-1, 1, "riisi", N_SETTEAM, ci->clientnum, teamnames[i], -1);
        }
    }
}
