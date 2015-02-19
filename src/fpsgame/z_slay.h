#ifndef Z_SLAY_H
#define Z_SLAY_H

#include "z_servcmd.h"

// note: plz don't troll inoccent players with this
void z_servcmd_slay(int argc, char **argv, int sender)
{
    if(argc < 2) { sendf(sender, 1, "ris", N_SERVMSG, "please specify client number"); return; }
    int cn;
    bool val = !strcasecmp(argv[0], "slay");
    if(!z_parseclient_verify(argv[1], &cn, !val))
    {
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("unknown client: %s", argv[1]));
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
SCOMMANDAH(slay, PRIV_ADMIN, z_servcmd_slay, 1);
SCOMMANDAH(unslay, PRIV_ADMIN, z_servcmd_slay, 1);

void z_servcmd_kill(int argc, char **argv, int sender)
{
    if(argc < 2) { sendf(sender, 1, "ris", N_SERVMSG, "please specify client number"); return; }
    int cn;
    if(!z_parseclient_verify(argv[1], &cn, true))
    {
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("unknown client: %s", argv[1]));
        return;
    }
    clientinfo *ci = cn >= 0 ? getinfo(cn) : NULL;
    if(ci && ci->local) { sendf(sender, 1, "ris", N_SERVMSG, "killing local client is not allowed"); return; }
    if(ci) suicide(ci);
    else loopv(clients) if(clients[i]->state.aitype==AI_NONE && !clients[i]->local && !clients[i]->spy) suicide(clients[i]);
}
SCOMMANDAH(kill, PRIV_AUTH, z_servcmd_kill, 1);

#endif // Z_SLAY_H
