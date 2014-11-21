#ifndef Z_SETMASTER_OVERRIDE
#define Z_SETMASTER_OVERRIDE

#include "z_triggers.h"
#include "z_servercommands.h"
#include "z_log.h"

VAR(defaultmastermode, -1, 0, 3);
SVAR(masterpass, "");

static void z_trigger_defaultmastermode(int type)
{
    mastermode = defaultmastermode;
    packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
    putint(p, N_MASTERMODE);
    putint(p, mastermode);
    sendpacket(-1, 1, p.finalize());
}
Z_TRIGGER(z_trigger_defaultmastermode, Z_TRIGGER_STARTUP);

bool setmaster(clientinfo *ci, bool val, const char *pass = "",
               const char *authname = NULL, const char *authdesc = NULL, int authpriv = PRIV_MASTER,
               bool force = false, bool trial = false, bool verbose = true, clientinfo *by = NULL)
{
    if(authname && !val) return false;
    const char *name = "";
    const int opriv = ci->privilege;
    bool haspass = false;
    if(val)
    {
        haspass = adminpass[0] && checkpassword(ci, adminpass, pass);
        int wantpriv = ci->local || haspass ? PRIV_ADMIN : authpriv;
        if(!force && wantpriv <= PRIV_MASTER && masterpass[0] && checkpassword(ci, masterpass, pass)) { force = true; haspass = true; }
        if(ci->privilege)
        {
            if(wantpriv <= ci->privilege) return true;
        }
        else if(wantpriv <= PRIV_MASTER && !force)
        {
            if(ci->state.state==CS_SPECTATOR)
            {
                if(verbose) sendf(ci->clientnum, 1, "ris", N_SERVMSG, "Spectators may not claim master.");
                return false;
            }
            if(!authname && !(mastermask&MM_AUTOAPPROVE) && !ci->privilege && !ci->local)
            {
                if(verbose) sendf(ci->clientnum, 1, "ris", N_SERVMSG, "This server requires you to use the \"/auth\" command to claim master.");
                return false;
            }
            loopv(clients) if(ci!=clients[i] && clients[i]->privilege)
            {
                if(verbose) sendf(ci->clientnum, 1, "ris", N_SERVMSG, "Master is already claimed.");
                return false;
            }
        }
        if(trial) return true;
        ci->privilege = wantpriv;
        name = privname(ci->privilege);
    }
    else
    {
        if(!ci->privilege) return false;
        if(trial) return true;
        name = privname(ci->privilege);
        revokemaster(ci);
    }
    bool hasmaster = false;
    loopv(clients) if(clients[i]->local || clients[i]->privilege >= PRIV_MASTER) hasmaster = true;
    bool mmchange = false;
    if(!hasmaster)
    {
        if(mastermode != defaultmastermode) { mastermode = defaultmastermode; mmchange = true; }
        allowedips.shrink(0);
        z_exectrigger(Z_TRIGGER_NOMASTER);
    }
    string msg;
    const bool inv_ = val ? ci->isinvpriv(ci->privilege) : ci->isinvpriv(opriv);
    if(val && authname)
    {
        if(authdesc && authdesc[0]) formatstring(msg)("%s claimed %s%s as '\fs\f5%s\fr' [\fs\f0%s\fr]", colorname(ci), inv_ ? "invisible " : "", name, authname, authdesc);
        else formatstring(msg)("%s claimed %s%s as '\fs\f5%s\fr'", colorname(ci), inv_ ? "invisible " : "", name, authname);
    }
    else formatstring(msg)("%s %s %s%s", colorname(ci), val ? "claimed" : "relinquished", inv_ ? "invisible " : "", name);

    z_log_setmaster(ci, val, haspass, authname, authdesc, name, by);

    for(int i = demorecord ? -1 : 0; i < clients.length(); i++)
    {
        clientinfo *cj = i >= 0 ? clients[i] : NULL;
        if(cj && cj->state.aitype!=AI_NONE) continue;
        // 1 case: client can see that ci claimed
        // 2 case: client could see old privilege, and ci relinquished
        // action: send out message and new privileges
        if(val ? ci->canseemypriv(cj) : ci->canseemypriv(cj, opriv))
        {
            packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
            putint(p, N_SERVMSG);
            sendstring(msg, p);
            putint(p, N_CURRENTMASTER);
            putint(p, mastermode);
            loopvj(clients) if(clients[j]->privilege >= PRIV_MASTER && clients[j]->canseemypriv(cj))
            {
                putint(p, clients[j]->clientnum);
                putint(p, clients[j]->privilege);
            }
            putint(p, -1);
            if(cj) sendpacket(cj->clientnum, 1, p.finalize());
            else { p.finalize(); recordpacket(1, p.packet->data, p.packet->dataLength); }
        }
        // 3 case: client couldn't see old privilege, but relinquish changed mastermode
        // action: send out mastermode only
        else if(!val && !ci->canseemypriv(cj, opriv) && mmchange)
        {
            packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
            putint(p, N_MASTERMODE);
            putint(p, mastermode);
            if(cj) sendpacket(cj->clientnum, 1, p.finalize());
            else { p.finalize(); recordpacket(1, p.packet->data, p.packet->dataLength); }
        }
        // 4 case: client could see old privilege, but can not see new privilege after claim
        // action: send fake relinquish message and new privileges
        else if(val && opriv >= PRIV_MASTER && ci->canseemypriv(cj, opriv) && !ci->canseemypriv(cj))
        {
            packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
            putint(p, N_SERVMSG);
            sendstring(tempformatstring("%s %s %s%s", colorname(ci), "relinquished", ci->isinvpriv(opriv) ? "invisible " : "", privname(opriv)), p);
            putint(p, N_CURRENTMASTER);
            putint(p, mastermode);
            loopvj(clients) if(clients[j]->privilege >= PRIV_MASTER && clients[j]->canseemypriv(cj))
            {
                putint(p, clients[j]->clientnum);
                putint(p, clients[j]->privilege);
            }
            putint(p, -1);
            if(cj) sendpacket(cj->clientnum, 1, p.finalize());
            else { p.finalize(); recordpacket(1, p.packet->data, p.packet->dataLength); }
        }
    }

    checkpausegame();
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
            if(ci->canseemypriv(cj) == ci->canseemypriv(cj, -1, val)) continue;
            packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
            putint(p, N_CURRENTMASTER);
            putint(p, mastermode);
            loopvj(clients) if(clients[j]->privilege >= PRIV_MASTER && (ci!=clients[j] ? clients[j]->canseemypriv(cj) : ci->canseemypriv(cj, -1, val)))
            {
                putint(p, clients[j]->clientnum);
                putint(p, clients[j]->privilege);
            }
            putint(p, -1);
            if(cj) sendpacket(cj->clientnum, 1, p.finalize());
            else { p.finalize(); recordpacket(1, p.packet->data, p.packet->dataLength); }
        }
        ci->invpriv = val!=0;
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("invpriv %s", val ? "enabled" : "disabled"));
    }
    else sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("invpriv is %s", ci->invpriv ? "enabled" : "disabled"));
}
SCOMMANDA(invpriv, PRIV_NONE, z_servcmd_invpriv, 1);
SCOMMANDAH(hidepriv, PRIV_NONE, z_servcmd_invpriv, 1);

#endif // Z_SETMASTER_OVERRIDE
