#ifndef Z_RENAME_H
#define Z_RENAME_H

#include "z_log.h"
#include "z_servcmd.h"

static void z_rename(clientinfo *ci, const char *name, bool broadcast = true)
{
    vector<uchar> b;
    putint(b, N_SWITCHNAME);
    sendstring(name, b);
    if(broadcast) ci->messages.put(b.getbuf(), b.length());
    packetbuf p(MAXSTRLEN, ENET_PACKET_FLAG_RELIABLE);
    putint(p, N_CLIENT);
    putint(p, ci->clientnum);
    putuint(p, b.length());
    p.put(b.getbuf(), b.length());
    sendpacket(ci->ownernum, 1, p.finalize());
    if(broadcast) flushserver(true);
}

void z_servcmd_rename(int argc, char **argv, int sender)
{
    clientinfo *ci = NULL, *actor = getinfo(sender);
    int cn;
    string name;

    if(argc < 2) { z_servcmd_pleasespecifyclient(sender); return; }
    if(!z_parseclient_verify(argv[1], cn, true, true))
    {
        z_servcmd_unknownclient(argv[1], sender);
        return;
    }
    if(cn >= 0) ci = getinfo(cn);

    name[0] = '\0';
    if(argc > 2) filtertext(name, argv[2], false, false, MAXNAMELEN);
    if(!name[0]) copystring(name, "unnamed");

    if(ci)
    {
        z_log_rename(ci, name, actor);
        copystring(ci->name, name);
        z_rename(ci, name);
    }
    else loopv(clients) if(!clients[i]->spy)
    {
        z_log_rename(clients[i], name, actor);
        copystring(clients[i]->name, name);
        z_rename(clients[i], name);
    }
}
SCOMMANDA(rename, PRIV_AUTH, z_servcmd_rename, 2);

#endif // Z_RENAME_H
