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

VAR(maprotation_norepeat, 0, 0, 200);

vector<char *> z_oldmaps;

// add map to maps history, if it matters
void z_addmaptohist(const char *mname)
{
    while(z_oldmaps.length() > max(maprotation_norepeat-1, 0)) delete[] z_oldmaps.remove(0);
    if(maprotation_norepeat) z_oldmaps.add(newstring(mname));
}

bool z_nextmaprotation()
{
    if(z_maprotationtype == MRT_NORMAL) return false;
    if(z_maprotationtype == MRT_STATIC) return true;
    //MRT_RANDOM
    // .... | <--------------- curmaprotation ----------------> | ....
    int minmaprotation = curmaprotation, maxmaprotation = curmaprotation;
    while(maprotations.inrange(minmaprotation-1) && maprotations[minmaprotation-1].modes) minmaprotation--;
    while(maprotations.inrange(maxmaprotation+1) && maprotations[maxmaprotation+1].modes) maxmaprotation++;
    int numrots = maxmaprotation - minmaprotation + 1;
    int c = 0;
    bool found;
    do
    {
        curmaprotation = rnd(numrots) + minmaprotation;
        found = false;
        loopv(z_oldmaps) if(!strcmp(maprotations[curmaprotation].map, z_oldmaps[i])) { found = true; break; }
    } while(found && c++ < 256);
    return true;
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
