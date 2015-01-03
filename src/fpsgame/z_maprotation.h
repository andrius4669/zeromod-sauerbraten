#ifndef Z_MAPROTATION_H
#define Z_MAPROTATION_H

enum { MRT_NORMAL = 0, MRT_RANDOM, MRT_STATIC };
int z_maprotationtype = MRT_NORMAL;
void z_setmrt(const char *str)
{
    if(!strcasecmp(str, "random")) z_maprotationtype = MRT_RANDOM;
    else if(!strcasecmp(str, "static")) z_maprotationtype = MRT_STATIC;
    else z_maprotationtype = MRT_NORMAL;
}
SVARF(maprotationmode, "", z_setmrt(maprotationmode));

VAR(maprotation_norepeat, 0, 0, 256);

vector<char *> z_oldmaps;

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
    } while(found && c++ < 1024);
    while(z_oldmaps.length() > max(maprotation_norepeat-1, 0)) delete[] z_oldmaps.remove(0);
    if(maprotation_norepeat) z_oldmaps.add(newstring(maprotations[curmaprotation].map));
    return true;
}

#endif // Z_MAPROTATION_H
