#ifdef Z_SLAY_H
#error "already z_slay.h"
#endif
#define Z_SLAY_H

#ifndef Z_SERVCMD_H
#error "want z_servcmd.h"
#endif


// note: plz don't troll inoccent players with this
void z_servcmd_slay(int argc, char **argv, int sender)
{
    if(argc < 2) { z_servcmd_pleasespecifyclient(sender); return; }
    int cn;
    bool val = !strcasecmp(argv[0], "slay");
    if(!z_parseclient_verify(argv[1], cn, !val))
    {
        z_servcmd_unknownclient(argv[1], sender);
        return;
    }
    clientinfo *ci = cn >= 0 ? getinfo(cn) : NULL;
    if(ci && ci->local) { sendf(sender, 1, "ris", N_SERVMSG, "slaying local client is not allowed"); return; }
    if(ci)
    {
        ci->xi.slay = val;
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("\fs\f3%s\fr %s", val ? "slaying" : "unslaying", colorname(ci)));
    }
    else loopv(clients) if(clients[i]->state.aitype==AI_NONE && !clients[i]->local && !clients[i]->spy)
    {
        clients[i]->xi.slay = val;
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("\fs\f3%s\fr %s", val ? "slaying" : "unslaying", colorname(clients[i])));
    }
}
SCOMMANDAH(slay, ZC_DISABLED | PRIV_ADMIN, z_servcmd_slay, 1);
SCOMMANDAH(unslay, ZC_DISABLED | PRIV_ADMIN, z_servcmd_slay, 1);

void z_servcmd_kill(int argc, char **argv, int sender)
{
    if(argc < 2) { z_servcmd_pleasespecifyclient(sender); return; }
    int cn;
    if(!z_parseclient_verify(argv[1], cn, true))
    {
        z_servcmd_unknownclient(argv[1], sender);
        return;
    }
    clientinfo *ci = cn >= 0 ? getinfo(cn) : NULL;
    if(ci && ci->local) { sendf(sender, 1, "ris", N_SERVMSG, "killing local client is not allowed"); return; }
    if(ci) suicide(ci);
    else loopv(clients) if(clients[i]->state.aitype==AI_NONE && !clients[i]->local && !clients[i]->spy) suicide(clients[i]);
}
SCOMMANDAH(kill, ZC_DISABLED | PRIV_AUTH, z_servcmd_kill, 1);
