#ifdef Z_PATCHMAP_H
#error "already z_patchmap.h"
#endif
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

VAR(s_patchreliable, -1, 1, 1); // use reliable packets or unreliable ones?
static void z_sendpatchpacket(clientinfo *ci, bool reliable)
{
    if(z_patchpacket.length() <= 0) return;
    if(!ci)
    {
        loopv(clients) if(clients[i]->state.aitype==AI_NONE) z_sendpatchpacket(clients[i], reliable);
        return;
    }
    packetbuf p(MAXTRANS, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
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

void z_sendallpatches(clientinfo *ci, bool reliable)
{
    z_initpatchpacket();
    z_addvarpatches();
    z_addentpatches();
    z_sendpatchpacket(ci, reliable);
    // workaround for weird bug when entities doesn't show up for some reason
    // have no idea why it works but it does
    z_initpatchpacket();
    z_addentpatches();
    z_sendpatchpacket(ci, reliable);
}

void s_sendallpatches() { z_sendallpatches(NULL, s_patchreliable > 0); }
COMMAND(s_sendallpatches, ""); // sends out all var and ent patches

void s_sendentpatches(int *n)
{
    z_initpatchpacket();
    z_addentpatches(*n);
    z_sendpatchpacket(NULL, s_patchreliable > 0);
}
COMMAND(s_sendentpatches, "i"); // sends out last x ent patches. if x is unspecified, send out all ent patches

void s_sendvarpatches(int *n)
{
    z_initpatchpacket();
    z_addvarpatches(*n);
    z_sendpatchpacket(NULL, s_patchreliable > 0);
}
COMMAND(s_sendvarpatches, "i"); // sends out last x var patches. if x is unspecified, send out all var patches

VAR(s_undoentpatches, 0, 1, 1); // should we clear/undo ents when doing s_clearentpatches/s_clearpatches?

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
        {
            size_t i = ep.i;
            z_patchent(i, ments.inrange(i) ? ments[i] : emptyent);
            if(sents.inrange(i))
            {
                int type = ments.inrange(i) ? ments[i].type : NOTUSED;
                sents[i].type = type;
                bool canspawn = canspawnitem(type);
                if(canspawn ? !sents[i].spawned : (sents[i].spawned || sents[i].spawntime))
                {
                    sents[i].spawntime = canspawn ? 1 : 0;
                    sents[i].spawned = false;
                }
            }
        }
        z_entpatches.drop();
    }
    z_sendpatchpacket(NULL, s_patchreliable > 0);
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
COMMAND(s_clearpatches, ""); // undos/clears var and ent patches

void s_clearentpatches(int *n) { z_clearentpatches(*n); }
COMMAND(s_clearentpatches, "i"); // undos/clears last x ent patches

void s_clearvarpatches(int *n) { z_clearvarpatches(*n); }
COMMAND(s_clearvarpatches, "i"); // clears last x var patches

VAR(s_autosendpatch, 0, 1, 1); // whether to automatically send patches on s_patchent/s_patchvar* or not
VAR(s_patchadd, 0, 1, 1); // whether to add patch to list or not

void s_patchent(char *name, int *id, float *x, float *y, float *z, int *type, int *a1, int *a2, int *a3, int *a4, int *a5)
{
    int i = *id;
    int typ = *type;

    z_entpatch &ep = z_entpatches.add();
    copystring(ep.mapname, name);
    ep.i = i;
    ep.e.o.x = *x;
    ep.e.o.y = *y;
    ep.e.o.z = *z;
    ep.e.type = typ;
    ep.e.attr1 = *a1;
    ep.e.attr2 = *a2;
    ep.e.attr3 = *a3;
    ep.e.attr4 = *a4;
    ep.e.attr5 = *a5;
    ep.e.reserved = 0;

    bool canspawn = canspawnitem(typ);
    if(sents.inrange(i) || canspawn)
    {
        const server_entity se = { NOTUSED, 0, false };
        while(sents.length() <= i) sents.add(se);
        sents[i].type = typ;
        if(canspawn ? !sents[i].spawned : (sents[i].spawned || sents[i].spawntime))
        {
            sents[i].spawntime = canspawn ? 1 : 0;
            sents[i].spawned = false;
        }
    }

    if(s_autosendpatch && (!*name || !strcmp(name, smapname)))
    {
        z_initpatchpacket();
        z_patchent(ep.i, ep.e);
        z_sendpatchpacket(NULL, s_patchreliable > 0);
    }

    if(!s_patchadd) z_entpatches.drop();
}
COMMAND(s_patchent, "sifffiiiiii"); // map id x y z type a1 a2 a3 a4 a5

static void z_checkvarpatch(const z_varpatch &vp)
{
    if(s_autosendpatch && (!vp.mapname[0] || !strcmp(vp.mapname, smapname)))
    {
        z_initpatchpacket();
        z_patchvar(vp.varname, vp.v);
        z_sendpatchpacket(NULL, s_patchreliable > 0);
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
