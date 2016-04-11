#ifndef Z_MAPROTATION_H
#define Z_MAPROTATION_H

#include "z_servercommands.h"

enum { MRT_NORMAL = 0, MRT_RANDOM, MRT_STATIC };
int z_maprotationtype = MRT_NORMAL;
void z_setmrt(const char *str)
{
    if(!strcasecmp(str, "random")) z_maprotationtype = MRT_RANDOM;
    else if(!strcasecmp(str, "static")) z_maprotationtype = MRT_STATIC;
    else z_maprotationtype = MRT_NORMAL;
}
SVARF(maprotationmode, "", z_setmrt(maprotationmode));
// normal - default handling
// static - next map is current map
// random - next map is random map from current map rotation slice

VAR(maprotation_norepeat, 0, 0, 9000); // how much old maps we should exclude when using random selection

static vector<char *> z_oldmaps;

// add map to maps history, if it matters
void z_addmaptohist(const char *mname)
{
    int shouldremove = z_oldmaps.length() - max(maprotation_norepeat-1, 0);
    if(shouldremove > 0)
    {
        loopi(shouldremove) delete[] z_oldmaps[i];
        z_oldmaps.remove(0, shouldremove);
    }
    if(maprotation_norepeat) z_oldmaps.add(newstring(mname));
}

bool z_nextmaprotation()
{
    if(z_maprotationtype == MRT_NORMAL) return false; // caller will handle
    if(z_maprotationtype == MRT_STATIC) return true;  // pretend we handled

    {
        // MRT_RANDOM
        // .... | <--------------- curmaprotation ----------------> | ....
        // detect current slice: slices are separated by boundaries where modes == 0
        int minmaprotation = curmaprotation, maxmaprotation = curmaprotation;
        while(maprotations.inrange(minmaprotation-1) && maprotations[minmaprotation-1].modes) --minmaprotation;
        while(maprotations.inrange(maxmaprotation+1) && maprotations[maxmaprotation+1].modes) ++maxmaprotation;
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
        }
        // if we don't need to account oldmaps, we don't need to construct possible buffer
        if(maxmaprotation > minmaprotation) curmaprotation = minmaprotation + rnd(maxmaprotation - minmaprotation + 1);
        return true;
    }
}

struct forcenextmapstruct
{
    string map;
    int mode;
};
static forcenextmapstruct *z_forcenextmap = NULL;

void z_nextmap(clientinfo &ci, const char *map, int mode)
{
    sendservmsgf("%s forced %s on map %s for next match", colorname(&ci), modename(mode), map);
    if(z_forcenextmap) delete z_forcenextmap;
    z_forcenextmap = new forcenextmapstruct;
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

#endif // Z_MAPROTATION_H
