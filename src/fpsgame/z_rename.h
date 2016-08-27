#ifdef Z_RENAME_H
#error "already z_rename.h"
#endif
#define Z_RENAME_H

#ifndef Z_LOG_H
#error "want z_log.h"
#endif
#ifndef Z_SERVCMD_H
#error "want z_servcmd.h"
#endif


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
        copystring(ci->name, name, MAXNAMELEN+1);
        z_rename(ci, ci->name);
    }
    else loopv(clients) if(!clients[i]->spy)
    {
        z_log_rename(clients[i], name, actor);
        copystring(clients[i]->name, name, MAXNAMELEN+1);
        z_rename(clients[i], clients[i]->name);
    }
}
SCOMMANDA(rename, PRIV_AUTH, z_servcmd_rename, 2);

ICOMMAND(s_rename, "is", (int *cn, char *newname),
{
    clientinfo *ci = getinfo(*cn);
    if(!ci) return;
    string name;
    filtertext(name, newname, false, false, MAXNAMELEN);
    if(!name[0]) copystring(name, "unnamed");
    z_log_rename(ci, name);
    copystring(ci->name, name, MAXNAMELEN+1);
    z_rename(ci, ci->name);
});
