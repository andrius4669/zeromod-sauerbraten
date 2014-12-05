#ifndef Z_MAPROTATION_H
#define Z_MAPROTATION_H

enum { MRT_NORMAL = 0, MRT_RANDOM };
int z_maprotationtype = MRT_NORMAL;
void z_setmrt(const char *str)
{
    if(!strcasecmp(str, "random")) z_maprotationtype = MRT_RANDOM;
    else z_maprotationtype = MRT_NORMAL;
}
SVARF(maprotationmode, "", z_setmrt(maprotationmode));

bool z_nextmaprotation()
{
    if(z_maprotationtype == MRT_NORMAL) return false;
    //MRT_RANDOM
    // .... | <--------------- curmaprotation ----------------> | ....
    int minmaprotation = curmaprotation, maxmaprotation = curmaprotation;
    while(maprotations.inrange(minmaprotation-1) && maprotations[minmaprotation-1].modes) minmaprotation--;
    while(maprotations.inrange(maxmaprotation+1) && maprotations[maxmaprotation+1].modes) maxmaprotation++;
    int numrots = maxmaprotation - minmaprotation + 1;
    curmaprotation = rnd(numrots) + minmaprotation;
    return true;
}

#endif // Z_MAPROTATION_H
