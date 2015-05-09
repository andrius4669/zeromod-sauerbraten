#ifndef Z_GHOST_H
#define Z_GHOST_H

void nullifyclientpos(clientinfo &ci)
{
    vector<uchar> &p = ci.position;
    p.setsize(0);
    putint(p, N_POS);
    putuint(p, ci.clientnum);
    p.put(PHYS_FLOAT | ((ci.state.lifesequence&1)<<3)); // physstate + lifesequence, no move or strafe
    putuint(p, 0); // flags
    loopk(3) { p.put(0); p.put(0); } // pos (x, y, z)
    p.put(0); p.put(0); // dir
    p.put(90); // roll
    p.put(0); // vel
    p.put(0); p.put(0); // veldir
}

bool z_isghost(clientinfo &ci)
{
    extern bool isracemode();
    extern bool z_race_shouldhide(clientinfo &ci);
    return ci.xi.ghost || (isracemode() && z_race_shouldhide(ci));
}

#if 0
void z_setghost(clientinfo &ci, bool val)
{
    if(ci.xi.ghost!=val)
    {
        ci.xi.ghost = val;
        if(ci.state.aitype==AI_NONE) sendf(ci.clientnum, 1, "ris", N_SERVMSG, tempformatstring("you got %s", val ? "ghosted" : "unghosted"));
    }
}

void z_servcmd_ghost(int argc, char **argv, int sender)
{
    //...
}
#endif

#endif
