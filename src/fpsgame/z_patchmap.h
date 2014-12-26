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
static inline void z_initpatchpacket() { z_patchpacket.setsize(0); }

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
    int idtype;
    switch(v.type)
    {
        case VAL_INT: idtype = ID_VAR; break;
        case VAL_FLOAT: idtype = ID_FVAR; break;
        case VAL_STR: idtype = ID_SVAR; break;
        default: return;
    }
    putint(z_patchpacket, N_EDITVAR);
    putint(z_patchpacket, idtype);
    sendstring(name, z_patchpacket);
    switch(idtype)
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

VAR(s_patchreliable, 0, 1, 1);
static void z_sendpatchpacket(clientinfo *ci = NULL)
{
    if(z_patchpacket.length() <= 0) return;
    if(!ci)
    {
        loopv(clients) if(clients[i]->state.aitype==AI_NONE) z_sendpatchpacket(clients[i]);
        return;
    }
    packetbuf p(MAXTRANS, s_patchreliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    putint(p, N_CLIENT);
    putint(p, ci->clientnum);
    putuint(p, z_patchpacket.length());
    p.put(z_patchpacket.getbuf(), z_patchpacket.length());
    sendpacket(ci->ownernum, 1, p.finalize());
}

static void z_addvarpatches(int i = 0)
{
    if(i) i = z_varpatches.length() - clamp(i, 0, z_varpatches.length());
    for(; i < z_varpatches.length(); i++)
        if(!z_varpatches[i].mapname[0] || !strcmp(z_varpatches[i].mapname, smapname))
            z_patchvar(z_varpatches[i].varname, z_varpatches[i].v);
}

static void z_addentpatches(int i = 0)
{
    if(i) i = z_entpatches.length() - clamp(i, 0, z_entpatches.length());
    for(; i < z_entpatches.length(); i++)
        if(!z_entpatches[i].mapname[0] || !strcmp(z_entpatches[i].mapname, smapname))
            z_patchent(z_entpatches[i].i, z_entpatches[i].e);
}

void z_sendallpatches(clientinfo *ci)
{
    z_initpatchpacket();
    z_addvarpatches();
    z_addentpatches();
    z_sendpatchpacket(ci);
}

void s_sendallpatches() { z_sendallpatches(NULL); }
COMMAND(s_sendallpatches, "");

void s_sendentpatches(int *n)
{
    z_initpatchpacket();
    z_addentpatches(*n);
    z_sendpatchpacket();
}
COMMAND(s_sendentpatches, "i");

void s_sendvarpatches(int *n)
{
    z_initpatchpacket();
    z_addvarpatches(*n);
    z_sendpatchpacket();
}

VAR(s_undoentpatches, 0, 1, 1);
static void z_clearentpatches(int n = 0)
{
    entity emptyent =
    {
        vec(0),
        0, 0, 0, 0, 0,
        NOTUSED,
        0
    };
    z_initpatchpacket();
    n = n ? clamp(n, 0, z_entpatches.length()) : z_entpatches.length();
    while(n--)
    {
        z_entpatch &ep = z_entpatches.last();
        if(s_undoentpatches && (!ep.mapname[0] || !strcmp(ep.mapname, smapname)))
            z_patchent(ep.i, ments.inrange(ep.i) ? ments[ep.i] : emptyent);
        z_entpatches.drop();
    }
    z_sendpatchpacket();
}

static void z_clearvarpatches(int n = 0)
{
    n = n ? clamp(n, 0, z_varpatches.length()) : z_varpatches.length();
    while(n--) z_varpatches.drop();
}

void s_clearpatches()
{
    z_clearvarpatches();
    z_clearentpatches();
}
COMMAND(s_clearpatches, "");

void s_clearentpatches(int *n) { z_clearentpatches(*n); }
COMMAND(s_clearentpatches, "i");

void s_clearvarpatches(int *n) { z_clearvarpatches(*n); }
COMMAND(s_clearvarpatches, "i");

VAR(s_autosendpatch, 0, 1, 1);
VAR(s_patchadd, 0, 1, 1);

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

    if(s_autosendpatch && (!*name || !strcmp(name, smapname)))
    {
        z_initpatchpacket();
        z_patchent(ep.i, ep.e);
        z_sendpatchpacket();
    }

    if(!s_patchadd) z_entpatches.drop();
}
COMMAND(s_patchent, "sifffiiiiii");

static void z_checkvarpatch(const z_varpatch &vp)
{
    if(s_autosendpatch && (!vp.mapname[0] || !strcmp(vp.mapname, smapname)))
    {
        z_initpatchpacket();
        z_patchvar(vp.varname, vp.v);
        z_sendpatchpacket();
    }
}

void s_patchvari(char *mapname, char *varname, int *i)
{
    z_varpatch &vp = z_varpatches.add();
    copystring(vp.mapname, mapname);
    copystring(vp.varname, varname);
    vp.v.setint(*i);
    z_checkvarpatch(vp);
    if(!s_patchadd) z_varpatches.drop();
}
COMMAND(s_patchvari, "ssi");

void s_patchvarf(char *mapname, char *varname, float *f)
{
    z_varpatch &vp = z_varpatches.add();
    copystring(vp.mapname, mapname);
    copystring(vp.varname, varname);
    vp.v.setfloat(*f);
    z_checkvarpatch(vp);
    if(!s_patchadd) z_varpatches.drop();
}
COMMAND(s_patchvarf, "ssf");

void s_patchvars(char *mapname, char *varname, char *s)
{
    z_varpatch &vp = z_varpatches.add();
    copystring(vp.mapname, mapname);
    copystring(vp.varname, varname);
    vp.v.setstr(newstring(s));
    z_checkvarpatch(vp);
    if(!s_patchadd) z_varpatches.drop();
}
COMMAND(s_patchvars, "sss");

#endif // Z_PATCHMAP_H
