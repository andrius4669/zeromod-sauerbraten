#ifndef Z_SENDMAP_H
#define Z_SENDMAP_H

#include "z_servcmd.h"

bool z_sendmap(clientinfo *ci, clientinfo *sender = NULL, stream *map = NULL, bool force = false, bool verbose = true)
{
    if(!map) map = mapdata;
    if(!map) { if(verbose && sender) sendf(sender->clientnum, 1, "ris", N_SERVMSG, "no map to send"); }
    else if(ci->getmap && !force)
    {
        if(verbose && sender) sendf(sender->clientnum, 1, "ris", N_SERVMSG,
            ci->clientnum == sender->clientnum ? "already sending map" : tempformatstring("already sending map to %s", colorname(ci)));
    }
    else
    {
        if(verbose) sendservmsgf("[%s is getting the map]", colorname(ci));
        ENetPacket *getmap = sendfile(ci->clientnum, 2, map, "ri", N_SENDMAP);
        if(getmap)
        {
            getmap->freeCallback = freegetmap;
            ci->getmap = getmap;
        }
        ci->needclipboard = totalmillis ? totalmillis : 1;
        return true;
    }
    return false;
}

void z_servcmd_sendmap(int argc, char **argv, int sender)
{
    if(!m_edit) { sendf(sender, 1, "ris", N_SERVMSG, "\"sendmap\" only works in coop edit mode"); return; }
    if(argc < 2) { sendf(sender, 1, "ris", N_SERVMSG, "please specify client"); return; }
    int cn;
    clientinfo *ci = NULL, *senderci = (clientinfo *)getclientinfo(sender);
    if(!z_parseclient(argv[1], &cn)) goto failcn;
    if(cn >= 0)
    {
        ci = getinfo(cn);
        if(!ci || !ci->connected) goto failcn;
        if(ci->state.aitype != AI_NONE) { sendf(sender, 1, "ris", N_SERVMSG, "can not send map to bot"); return; }
    }
    if(ci) z_sendmap(ci, senderci);
    else loopv(clients) if(clients[i]->state.aitype == AI_NONE && clients[i]->clientnum != sender) z_sendmap(clients[i], senderci);
    return;
failcn:
    sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("unknown client: %s", argv[1]));
}
SCOMMANDA(sendmap, PRIV_MASTER, z_servcmd_sendmap, 1);
SCOMMANDAH(sendto, PRIV_MASTER, z_servcmd_sendmap, 1);

#endif // Z_SENDMAP_H
