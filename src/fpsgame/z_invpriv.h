#ifdef Z_INVPRIV_H
#error "already z_invpriv.h"
#endif
#define Z_INVPRIV_H

static bool z_isinvpriv(clientinfo *ci, int priv, int inv = -1)
{
    return (inv < 0 ? ci->xi.invpriv : inv!=0) && priv > PRIV_MASTER;
}

bool z_canseemypriv(clientinfo *me, clientinfo *ci, int priv = -1, int inv = -1)
{
    if(priv < 0) priv = me->privilege;
    if(me->spy) return ci && me->clientnum == ci->clientnum;
    if(z_isinvpriv(me, priv, inv)) return ci && (ci->privilege >= priv || ci->local || ci->clientnum == me->clientnum);
    return true;
}

void z_servcmd_invpriv(int argc, char **argv, int sender)
{
    clientinfo *ci = getinfo(sender);
    int val = (argc >= 2 && argv[1] && *argv[1]) ? clamp(atoi(argv[1]), 0, 1) : -1;
    if(val >= 0)
    {
        // if client has priv and privilege visibility changed, resend privs
        if(ci->privilege >= PRIV_MASTER) for(int i = demorecord ? -1 : 0; i < clients.length(); i++)
        {
            clientinfo *cj = i >= 0 ? clients[i] : NULL;
            if(cj && cj->state.aitype!=AI_NONE) continue;
            if(z_canseemypriv(ci, cj) == z_canseemypriv(ci, cj, -1, val)) continue;
            packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
            putint(p, N_CURRENTMASTER);
            putint(p, mastermode);
            loopvj(clients) if(clients[j]->privilege >= PRIV_MASTER && (ci!=clients[j] ? z_canseemypriv(clients[j], cj) : z_canseemypriv(ci, cj, -1, val)))
            {
                putint(p, clients[j]->clientnum);
                putint(p, clients[j]->privilege);
            }
            putint(p, -1);
            if(cj) sendpacket(cj->clientnum, 1, p.finalize());
            else { p.finalize(); recordpacket(1, p.packet->data, p.packet->dataLength); }
        }
        ci->xi.invpriv = val!=0;
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("invpriv %s", val ? "enabled" : "disabled"));
    }
    else sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("invpriv is %s", ci->xi.invpriv ? "enabled" : "disabled"));
}
SCOMMANDA(invpriv, PRIV_NONE, z_servcmd_invpriv, 1);
SCOMMANDAH(hidepriv, PRIV_NONE, z_servcmd_invpriv, 1);
