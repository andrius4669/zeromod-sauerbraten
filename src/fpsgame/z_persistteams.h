#ifndef Z_PERSISTTEAMS_H
#define Z_PERSISTTEAMS_H

#include "z_servercommands.h"

int z_persistteams = 0;
VARFN(persistteams, defaultpersistteams, 0, 0, 2, { if(clients.empty()) z_persistteams = defaultpersistteams; });
static void z_persistteams_trigger(int type) { z_persistteams = defaultpersistteams; }
Z_TRIGGER(z_persistteams_trigger, Z_TRIGGER_STARTUP);
Z_TRIGGER(z_persistteams_trigger, Z_TRIGGER_NOCLIENTS);

void z_servcmd_persistteams(int argc, char **argv, int sender)
{
    if(argc < 2)
    {
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("persistent teams are %s",
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
    loopi(2) if(!strcmp(team, teamnames[i])) return i+1;
    return 0;
}

void z_autoteam()
{
    static const char * const teamnames[2] = {"good", "evil"};
    vector<clientinfo *> team[2];
    float teamrank[2] = {0, 0};
    int remaining = clients.length();
    if(z_persistteams == 1)
    {
        string sttnames[2];
        *sttnames[0] = *sttnames[1] = '\0';
        loopv(clients) if(clients[i]->team[0])
        {
            clientinfo *ci = clients[i];
            if(m_ctf && !sttnames[0]) copystring(sttnames[0], ci->team);
            int standard = z_checkstandardteam(ci->team, m_ctf ? (const char * const *)sttnames : teamnames);
            if(m_ctf && !standard && !sttnames[1]) { copystring(sttnames[1], ci->team); standard = 2; }
            if(m_ctf && !standard) continue;
            float rank = ci->state.state!=CS_SPECTATOR ? ci->state.effectiveness/max(ci->state.timeplayed, 1) : -1;
            if(smode && smode->hidefrags()) rank = 1;
            ci->state.timeplayed = -1;
            if(standard)
            {
                team[standard-1].add(ci);
                if(rank>0) teamrank[standard-1] += rank;
            }
            remaining--;
        }
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
    else if(z_persistteams == 2)
    {
        //FUKKEN TODO
    }
}

#endif // Z_PERSISTTEAMS_H
