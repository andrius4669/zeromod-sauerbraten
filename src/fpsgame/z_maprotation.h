#ifdef Z_MAPROTATION_H
#error "already z_maprotation.h"
#endif
#define Z_MAPROTATION_H

#ifndef Z_SERVERCOMMANDS_H
#error "want z_servercommands.h"
#endif

enum { MRT_NORMAL = 0, MRT_STATIC, MRT_RANDOM, MRT_BATTLE };
int z_maprotationtype = MRT_NORMAL;
void z_setmrt(const char *str)
{
    /**/ if(!strcasecmp(str, "normal")) z_maprotationtype = MRT_NORMAL;
    else if(!strcasecmp(str, "static")) z_maprotationtype = MRT_STATIC;
    else if(!strcasecmp(str, "random")) z_maprotationtype = MRT_RANDOM;
    else if(!strcasecmp(str, "battle")) z_maprotationtype = MRT_BATTLE;
    else
    {
        logoutf("WARNING: maprotationmode unknown, setting to normal");
        z_maprotationtype = MRT_NORMAL;
    }
}
SVARF(maprotationmode, "", z_setmrt(maprotationmode));
// normal - default handling
// static - next map is current map
// random - next map is random map from current map rotation slice
// battle - players are given selection

VAR(maprotation_norepeat, 0, 10, 9000); // how much old maps we should exclude when using random selection

static vector<char *> z_oldmaps;

struct mapvotestruct
{
    string map;
    int mode;
};
static mapvotestruct *z_forcenextmap = NULL;

VAR(nextmapclear, 0, 1, 2); // 0 - don't, 1 - on same-map change, 2 - on any map change

// add map to maps history, if it matters
void z_addmaptohist()
{
    int shouldremove = z_oldmaps.length() - max(maprotation_norepeat-1, 0);
    if(shouldremove > 0)
    {
        loopi(shouldremove) delete[] z_oldmaps[i];
        z_oldmaps.remove(0, shouldremove);
    }
    if(maprotation_norepeat) z_oldmaps.add(newstring(smapname));

    if (z_forcenextmap &&
        nextmapclear &&
        (nextmapclear == 2 ||
            (!strcmp(z_forcenextmap->map, smapname) &&
                z_forcenextmap->mode == gamemode)))
    {
        DELETEP(z_forcenextmap);
    }
}

void z_maprotation_range(int &min, int &max)
{
    min = curmaprotation;
    max = curmaprotation;
    while(maprotations.inrange(min-1) && maprotations[min-1].modes) --min;
    while(maprotations.inrange(max+1) && maprotations[max+1].modes) ++max;
}

VAR(mapbattlemaps, 2, 3, 10);

static vector<mapvotestruct> mapbattleallowlist;

void z_countmbvotes(vector<int> &votes)
{
    loopv(mapbattleallowlist) votes.add(0);
    loopv(clients)
    {
        clientinfo *oi = clients[i];
        if(oi->state.state==CS_SPECTATOR && !oi->privilege && !oi->local) continue;
        if(oi->state.aitype!=AI_NONE) continue;
        if(!m_valid(oi->modevote)) continue;
        // if its in allowlist, find out which and count vote
        loopvj(mapbattleallowlist)
        {
            if(!strcmp(mapbattleallowlist[j].map, oi->mapvote) &&
                mapbattleallowlist[j].mode == oi->modevote)
            {
                ++votes[j];
                break;
            }
        }
    }
}


VAR(mapbattle_intermission, 1, 20, 3600);
VAR(mapbattle_chat, 0, 0, 1);

SVAR(mapbattle_style_announce, "\f3mapbattle:\f7 %M"); // %M - map list separated by separator
SVAR(mapbattle_style_announce_map, "%m \f0#%n\f7"); // how map should look in map list
SVAR(mapbattle_style_announce_separator, ", ");

void z_mapbattle_announce()
{
    // set different intermission than what it was
    interm = gamemillis + max(serverintermission, mapbattle_intermission)*1000;

    vector<char> mapbuf;
    loopv(mapbattleallowlist)
    {
        z_formattemplate ft[] =
        {
            { 'm', "%s", (const void *)mapbattleallowlist[i].map },
            { 'd', "%s", (const void *)modename(mapbattleallowlist[i].mode) },
            { 'n', "%d", (const void *)(intptr_t)(i+1) },
            { 0, NULL, NULL }
        };
        string buf;
        z_format(buf, sizeof(buf), mapbattle_style_announce_map, ft);

        if(i) mapbuf.put(mapbattle_style_announce_separator, strlen(mapbattle_style_announce_separator));
        mapbuf.put(buf, strlen(buf));
    }
    mapbuf.add('\0');

    z_formattemplate ft[] =
    {
        { 'M', "%s", (const void *)mapbuf.getbuf() },
        { 'd', "%s", (const void *)modename(gamemode) },
        { 0, NULL, NULL }
    };
    char buf[MAXTRANS];
    z_format(buf, sizeof(buf), mapbattle_style_announce, ft);
    sendservmsg(buf);
}

SVAR(mapbattle_style_vote, "\f3mapbattle:\f2 \f7%c \f2voted for \f7%m \f0#%n\f2; current votes: %V");
SVAR(mapbattle_style_vote_current, "\fs\f7%m \f0#%n\f2: \f3%v\fr");
SVAR(mapbattle_style_vote_separator, ", ");

bool z_mapbattle_announce_vote(clientinfo *ci)
{
    int voted = -1;
    loopv(mapbattleallowlist)
    {
        if(!strcmp(mapbattleallowlist[i].map, ci->mapvote) &&
            mapbattleallowlist[i].mode == ci->modevote)
        {
            voted = i;
            break;
        }
    }
    // if voted for something not part of our mapbattle
    if(voted < 0) return false;

    vector<int> votes;
    z_countmbvotes(votes);

    vector<char> votebuf;
    loopv(mapbattleallowlist)
    {
        z_formattemplate ft[] =
        {
            { 'm', "%s", (const void *)mapbattleallowlist[i].map },
            { 'd', "%s", (const void *)modename(mapbattleallowlist[i].mode) },
            { 'n', "%d", (const void *)(intptr_t)(i+1) },
            { 'v', "%d", (const void *)(intptr_t)(votes[i]) },
            { 0, NULL, NULL }
        };
        string buf;
        z_format(buf, sizeof(buf), mapbattle_style_vote_current, ft);

        if(i) votebuf.put(mapbattle_style_vote_separator, strlen(mapbattle_style_vote_separator));
        votebuf.put(buf, strlen(buf));
    }
    votebuf.add('\0');

    z_formattemplate ft[] =
    {
        { 'c', "%s", (const void *)colorname(ci) },
        { 'm', "%s", (const void *)mapbattleallowlist[voted].map },
        { 'd', "%s", (const void *)modename(mapbattleallowlist[voted].mode) },
        { 'n', "%d", (const void *)(intptr_t)(voted+1) },
        { 'V', "%s", (const void *)votebuf.getbuf() },
        { 0, NULL, NULL }
    };
    char buf[MAXTRANS];
    z_format(buf, sizeof(buf), mapbattle_style_vote, ft);
    sendservmsg(buf);

    return true;
}

void z_maprotation_intermission()
{
    if(z_maprotationtype != MRT_BATTLE) return;

    mapbattleallowlist.setsize(0);

    int minmaprotation, maxmaprotation;
    z_maprotation_range(minmaprotation, maxmaprotation);

    vector<int> possible;
    // try to check old maps, but each iteration less of it
    for(int start = 0; start < z_oldmaps.length(); ++start)
    {
        possible.setsize(0);
        // fill possible buffer with indexes filtered out from old maps
        for(int i = minmaprotation; i <= maxmaprotation; i++)
        {
            bool exists = false;
            for(int j = start; j < z_oldmaps.length(); j++)
            {
                if(!strcmp(maprotations[i].map, z_oldmaps[j]))
                {
                    exists = true;
                    break;
                }
            }
            if(!exists) possible.add(i);
        }
        int possiblelen = possible.length();
        // if we got enough possibilities...
        if(possiblelen >= mapbattlemaps)
        {
            vector<int> choices;
            // fill in choices
            while(choices.length() < mapbattlemaps)
            {
                choices.addunique(possible[rnd(possiblelen)]);
            }
            // convert them to actual allowlist
            loopv(choices)
            {
                mapvotestruct mv;
                copystring(mv.map, maprotations[choices[i]].map);
                mv.mode = gamemode;
                mapbattleallowlist.add(mv);
            }
            // done here
            z_mapbattle_announce();
            return;
        }
        // if we didn't then try again with less oldmaps accounted
    }
    // if we don't need to account oldmaps, we don't need to construct possible buffer
    int possiblerotlen = maxmaprotation - minmaprotation + 1;
    if(possiblerotlen >= 2)
    {
        vector<int> choices;
        // fill in choices
        while(choices.length() < min(mapbattlemaps, possiblerotlen))
        {
            choices.addunique(minmaprotation + rnd(possiblerotlen));
        }
        // convert them to actual allowlist
        loopv(choices)
        {
            mapvotestruct mv;
            copystring(mv.map, maprotations[choices[i]].map);
            mv.mode = gamemode;
            mapbattleallowlist.add(mv);
        }
        // done here
        z_mapbattle_announce();
        return;
    }
}

bool z_maproatation_onvote(clientinfo *ci)
{
    if(!interm || z_maprotationtype != MRT_BATTLE || !mapbattleallowlist.length())
    {
        return false;
    }

    return z_mapbattle_announce_vote(ci);
}

SVAR(mapbattle_style_notmapbattle, "currently there is no map battle");
SVAR(mapbattle_style_outofrange, "map battle vote out of range");

void z_servcmd_votemb(int argc, char **argv, int sender)
{
    if(z_maprotationtype != MRT_BATTLE || !mapbattleallowlist.length() || !interm)
    {
        sendf(sender, 1, "ris", N_SERVMSG, mapbattle_style_notmapbattle);
        return;
    }

    int i = atoi(argv[0]) - 1;
    if(!mapbattleallowlist.inrange(i))
    {
        sendf(sender, 1, "ris", N_SERVMSG, mapbattle_style_outofrange);
        return;
    }

    clientinfo *ci = getinfo(sender);
    if(!ci || (ci->state.state==CS_SPECTATOR && !ci->privilege && !ci->local)) return;

    copystring(ci->mapvote, mapbattleallowlist[i].map);
    ci->modevote = mapbattleallowlist[i].mode;

    z_mapbattle_announce_vote(ci);
}
#define X(n) SCOMMANDH(n, PRIV_NONE, z_servcmd_votemb)
X(1); X(2); X(3); X(4); X(5); X(6); X(7); X(8); X(9); X(10);
#undef X

void z_maprotation_ontext(clientinfo *ci, const char *text)
{
    if(!interm || z_maprotationtype != MRT_BATTLE || !mapbattleallowlist.length()) return;
    if(!mapbattle_chat) return;
    while(*text == ' ' || *text == '\t') ++text;
    if(!*text) return;
    char *end;
    unsigned long x = strtoul(text, &end, 10);
    while(*end == ' ' || *end == '\t') ++end;
    if(*end) return; // wasn't only number
    int i = int(x) - 1;
    if(!mapbattleallowlist.inrange(i)) return;
    if(!ci || (ci->state.state==CS_SPECTATOR && !ci->privilege && !ci->local)) return;

    copystring(ci->mapvote, mapbattleallowlist[i].map);
    ci->modevote = mapbattleallowlist[i].mode;

    z_mapbattle_announce_vote(ci);
}

bool z_nextmaprotation()
{
    if(z_maprotationtype == MRT_NORMAL) return false; // caller will handle
    if(z_maprotationtype == MRT_STATIC) return true;  // pretend we handled

    // MRT_RANDOM && MRT_BATTLE
    // .... | <--------------- curmaprotation ----------------> | ....
    // detect current slice: slices are separated by boundaries where modes == 0
    int minmaprotation, maxmaprotation;
    z_maprotation_range(minmaprotation, maxmaprotation);

    if(z_maprotationtype == MRT_BATTLE && mapbattleallowlist.length())
    {
        // count votes
        vector<int> votes;
        z_countmbvotes(votes);

        while(votes.length())
        {
            int maxvote = 0;
            // find the one which was voted the most
            for(int i = 1; i < votes.length(); ++i) if(votes[maxvote] < votes[i]) maxvote = i;
            // take it out
            mapvotestruct v = mapbattleallowlist.remove(maxvote);
            votes.remove(maxvote);
            // find relevant maprotation in current slice for that map
            for(int i = minmaprotation; i <= maxmaprotation; i++)
            {
                if(!strcmp(maprotations[i].map, v.map) && gamemode == v.mode)
                {
                    // found, use it
                    curmaprotation = i;
                    return true;
                }
            }
            // nothing found, retry for next candidate
        }
        // all candidates are gone from rotation, wtf, proceeed with random one
    }

    {
        // make buffer with possible maprotation indexes
        vector<int> possible;
        // try to check old maps, but each iteration less of it
        for(int start = 0; start < z_oldmaps.length(); ++start)
        {
            // fill possible buffer with indexes filtered out from old maps
            for(int i = minmaprotation; i <= maxmaprotation; i++)
            {
                bool exists = false;
                for(int j = start; j < z_oldmaps.length(); j++) if(!strcmp(maprotations[i].map, z_oldmaps[j])) { exists = true; break; }
                if(!exists) possible.add(i);
            }
            int possiblelen = possible.length();
            // if we got any possibilities...
            if(possiblelen > 0)
            {
                int possiblei = possiblelen > 1 ? rnd(possiblelen) : 0;
                curmaprotation = possible[possiblei];
                return true;
            }
            // if we didn't then try again with less oldmaps accounted
        }
        // if we don't need to account oldmaps, we don't need to construct possible buffer
        if(minmaprotation < maxmaprotation) curmaprotation = minmaprotation + rnd(maxmaprotation - minmaprotation + 1);
        return true;
    }
}

void z_nextmap(clientinfo &ci, const char *map, int mode)
{
    sendservmsgf("%s forced %s on map %s for next match", colorname(&ci), modename(mode), map);
    if(z_forcenextmap) delete z_forcenextmap;
    z_forcenextmap = new mapvotestruct;
    copystring(z_forcenextmap->map, map);
    z_forcenextmap->mode = mode;
}

void z_servcmd_nextmap(int argc, char **argv, int sender)
{
    clientinfo &ci = *getinfo(sender);
    if(argc < 2)
    {
        if(m_valid(ci.modevote)) z_nextmap(ci, ci.mapvote, ci.modevote);
        else sendf(sender, 1, "ris", N_SERVMSG, "please specify map");
        return;
    }

    if(lockmaprotation && !ci.local && ci.privilege < (lockmaprotation > 1 ? PRIV_ADMIN : PRIV_MASTER) && findmaprotation(gamemode, argv[1]) < 0)
    {
        sendf(sender, 1, "ris", N_SERVMSG, "This server has locked the map rotation.");
        return;
    }

    z_nextmap(ci, argv[1], gamemode);
}
SCOMMANDA(nextmap, PRIV_MASTER, z_servcmd_nextmap, 2);
