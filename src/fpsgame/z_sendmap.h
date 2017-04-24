#ifdef Z_SENDMAP_H
#error "already z_sendmap.h"
#endif
#define Z_SENDMAP_H

#ifndef Z_SERVCMD_H
#error "want z_servcmd.h"
#endif

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
        ci->xi.mapsent = ci->needclipboard = totalmillis ? totalmillis : 1;
        return true;
    }
    return false;
}

void z_servcmd_sendmap(int argc, char **argv, int sender)
{
    if(!m_edit) { sendf(sender, 1, "ris", N_SERVMSG, "\"sendmap\" only works in coop edit mode"); return; }
    if(argc < 2) { z_servcmd_pleasespecifyclient(sender); return; }
    int cn;
    clientinfo *senderci = (clientinfo *)getclientinfo(sender);
    if(!z_parseclient_verify(argv[1], cn, true))
    {
        z_servcmd_unknownclient(argv[1], sender);
        return;
    }
    if(cn >= 0) z_sendmap(getinfo(cn), senderci);
    else loopv(clients) if(clients[i]->state.aitype == AI_NONE && clients[i]->clientnum != sender) z_sendmap(clients[i], senderci);
}
SCOMMANDA(sendmap, PRIV_MASTER, z_servcmd_sendmap, 1);
SCOMMANDAH(sendto, PRIV_MASTER, z_servcmd_sendmap, 1);
