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
        ci->slay = val;
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("\fs\f3%s\fr %s", val ? "slaying" : "unslaying", colorname(ci)));
    }
    else loopv(clients) if(clients[i]->state.aitype==AI_NONE && !clients[i]->local && !clients[i]->spy)
    {
        clients[i]->slay = val;
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("\fs\f3%s\fr %s", val ? "slaying" : "unslaying", colorname(clients[i])));
    }
}
SCOMMANDAH(slay, PRIV_ADMIN, z_servcmd_slay, 1);
SCOMMANDAH(unslay, PRIV_ADMIN, z_servcmd_slay, 1);

#endif // Z_SLAY_H
