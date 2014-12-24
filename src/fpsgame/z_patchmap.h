#ifndef Z_PATCHMAP_H
#define Z_PATCHMAP_H

struct z_entpatch
{
    string mapname;
    entity e;
    int i;
};
vector<z_entpatch> z_entpatches;

struct z_varpatch
{
    string mapname;
    string varname;
    tagval v;
    ~z_varpatch() { v.cleanup(); }
};
vector<z_varpatch> z_varpatches;

static vector<uchar> z_patchpacket;
static inline void z_cleanpatchpacket() { z_patchpacket.setsize(0); }
static inline void z_initpatchpacket() { z_cleanpatchpacket(); }

static void z_patchent(int i, const entity &e)
{
    putint(z_patchpacket, N_EDITENT);
    putint(z_patchpacket, i);
    putint(z_patchpacket, (int)(e.o.x*DMF));
    putint(z_patchpacket, (int)(e.o.y*DMF));
    putint(z_patchpacket, (int)(e.o.z*DMF));
    putint(z_patchpacket, e.type);
    putint(z_patchpacket, e.attr1);
    putint(z_patchpacket, e.attr2);
    putint(z_patchpacket, e.attr3);
    putint(z_patchpacket, e.attr4);
    putint(z_patchpacket, e.attr5);
}

static void z_patchvar(const char *name, const tagval &v)
{
    putint(z_patchpacket, N_EDITVAR);
    putint(z_patchpacket, v.type);
    sendstring(name, z_patchpacket);
    switch(v.type)
    {
        case ID_VAR:
            putint(z_patchpacket, v.getint());
            break;
        case ID_FVAR:
            putfloat(z_patchpacket, v.getfloat());
            break;
        case ID_SVAR:
            sendstring(v.getstr(), z_patchpacket);
            break;
    }
}

static void z_sendpatchpacket(clientinfo *ci = NULL)
{
    if(!ci)
    {
        loopv(clients) if(clients[i]->state.aitype==AI_NONE) z_sendpatchpacket(clients[i]);
        return;
    }
    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    putint(p, N_CLIENT);
    putint(p, ci->clientnum);
    putint(p, z_patchpacket.length());
    p.put(z_patchpacket.getbuf(), z_patchpacket.length());
    sendpacket(ci->ownernum, 1, p.finalize());
}

void z_sendallpatches(clientinfo *ci)
{
    z_initpatchpacket();
    bool needsend = false;
    loopv(z_varpatches) if(!z_varpatches[i].mapname[0] || !strcmp(z_varpatches[i].mapname, smapname))
    {
        needsend = true;
        z_patchvar(z_varpatches[i].varname, z_varpatches[i].v);
    }
    loopv(z_entpatches) if(!z_entpatches[i].mapname[0] || !strcmp(z_entpatches[i].mapname, smapname))
    {
        needsend = true;
        z_patchent(z_entpatches[i].i, z_entpatches[i].e);
    }
    if(needsend) z_sendpatchpacket(ci);
    z_cleanpatchpacket();
}

static void z_clearentpatches()
{
    entity emptyent =
    {
        vec(0),         // explict vec
        0, 0, 0, 0, 0,
        NOTUSED,
        0
    };
    bool needsend = false;
    z_initpatchpacket();
    loopvrev(z_entpatches)
    {
        z_entpatch &ep = z_entpatches[i];
        if(!ep.mapname[0] || !strcmp(ep.mapname, smapname))
        {
            needsend = true;
            z_patchent(ep.i, ments.inrange(ep.i) ? ments[ep.i] : emptyent);
        }
    }
    z_entpatches.setsize(0);
    if(needsend) z_sendpatchpacket();
    z_cleanpatchpacket();
}
static void z_resetentpatches() { z_entpatches.setsize(0); }
static void z_clearvarpatches() { z_varpatches.shrink(0); }

void s_clearpatches()
{
    z_clearvarpatches();
    z_clearentpatches();
}
COMMAND(s_clearpatches, "");

void s_resetpatches()
{
    z_clearvarpatches();
    z_resetentpatches();
}
COMMAND(s_resetpatches, "");

void s_patchent(char *name, int *id, float *x, float *y, float *z, int *type, int *a1,int *a2, int *a3, int *a4, int *a5)
{
    z_entpatch &ep = z_entpatches.add();
    copystring(ep.mapname, name);
    ep.i = *id;
    ep.e.o.x = *x;
    ep.e.o.y = *y;
    ep.e.o.z = *z;
    ep.e.type = *type;
    ep.e.attr1 = *a1;
    ep.e.attr2 = *a2;
    ep.e.attr3 = *a3;
    ep.e.attr4 = *a4;
    ep.e.attr5 = *a5;
    ep.e.reserved = 0;

    if(!*name || !strcmp(name, smapname))
    {
        z_initpatchpacket();
        z_patchent(ep.i, ep.e);
        z_sendpatchpacket();
        z_cleanpatchpacket();
    }
}
COMMAND(s_patchent, "sifffiiiiii");

static void z_checkvarpatch(const z_varpatch &vp)
{
    if(!vp.mapname[0] || !strcmp(vp.mapname, smapname))
    {
        z_initpatchpacket();
        z_patchvar(vp.varname, vp.v);
        z_sendpatchpacket();
        z_cleanpatchpacket();
    }
}

void s_patchvari(char *mapname, char *varname, int *i)
{
    z_varpatch &vp = z_varpatches.add();
    copystring(vp.mapname, mapname);
    copystring(vp.varname, varname);
    vp.v.setint(*i);
    z_checkvarpatch(vp);
}
COMMAND(s_patchvari, "ssi");

void s_patchvarf(char *mapname, char *varname, float *f)
{
    z_varpatch &vp = z_varpatches.add();
    copystring(vp.mapname, mapname);
    copystring(vp.varname, varname);
    vp.v.setfloat(*f);
    z_checkvarpatch(vp);
}
COMMAND(s_patchvarf, "ssf");

void s_patchvars(char *mapname, char *varname, char *s)
{
    z_varpatch &vp = z_varpatches.add();
    copystring(vp.mapname, mapname);
    copystring(vp.varname, varname);
    vp.v.setstr(newstring(s));
    z_checkvarpatch(vp);
}
COMMAND(s_patchvars, "sss");

#endif // Z_PATCHMAP_H
