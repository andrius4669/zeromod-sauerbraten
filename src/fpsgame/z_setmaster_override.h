#ifdef Z_SETMASTER_OVERRIDE_H
#error "already z_setmaster_overrhide.h"
#endif
#define Z_SETMASTER_OVERRIDE_H

#ifndef Z_TRIGGERS_H
#error "want z_triggers.h"
#endif
#ifndef Z_SERVERCOMMANDS_H
#error "want z_servercommands.h"
#endif
#ifndef Z_LOG_H
#error "want z_log.h"
#endif
#ifndef Z_INVPRIV_H
#error "want z_invpriv.h"
#endif


VAR(defaultmastermode, -1, 0, 3);

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
        if(authname && !trial && (wantpriv == PRIV_NONE || ci->xi.authident))
        {
            z_log_ident(ci, authname, authdesc);
            ci->xi.ident.set(authname, authdesc);
            ci->xi.authident = false;
            wantpriv = PRIV_NONE;
        }
        if(wantpriv <= ci->privilege) return true;
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
    if(val && authname) ci->xi.claim.set(authname, authdesc);
    else ci->xi.claim.clear();
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
    const bool inv_ = val ? z_isinvpriv(ci, ci->privilege) : z_isinvpriv(ci, opriv);
    if(val && authname)
    {
        if(authdesc && authdesc[0]) formatstring(msg, "%s claimed %s%s as '\fs\f5%s\fr' [\fs\f0%s\fr]", colorname(ci), inv_ ? "invisible " : "", name, authname, authdesc);
        else formatstring(msg, "%s claimed %s%s as '\fs\f5%s\fr'", colorname(ci), inv_ ? "invisible " : "", name, authname);
    }
    else formatstring(msg, "%s %s %s%s", colorname(ci), val ? "claimed" : "relinquished", inv_ ? "invisible " : "", name);

    z_log_setmaster(ci, val, haspass, authname, authdesc, name, by);

    for(int i = demorecord ? -1 : 0; i < clients.length(); i++)
    {
        clientinfo *cj = i >= 0 ? clients[i] : NULL;
        if(cj && cj->state.aitype!=AI_NONE) continue;
        // 1 case: client can see that ci claimed
        // 2 case: client could see old privilege, and ci relinquished
        // action: send out message and new privileges
        if(val ? z_canseemypriv(ci, cj) : z_canseemypriv(ci, cj, opriv))
        {
            packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
            putint(p, N_SERVMSG);
            sendstring(msg, p);
            putint(p, N_CURRENTMASTER);
            putint(p, mastermode);
            loopvj(clients) if(clients[j]->privilege >= PRIV_MASTER && z_canseemypriv(clients[j], cj))
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
        else if(!val && !z_canseemypriv(ci, cj, opriv) && mmchange)
        {
            packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
            putint(p, N_MASTERMODE);
            putint(p, mastermode);
            if(cj) sendpacket(cj->clientnum, 1, p.finalize());
            else { p.finalize(); recordpacket(1, p.packet->data, p.packet->dataLength); }
        }
        // 4 case: client could see old privilege, but can not see new privilege after claim
        // action: send fake relinquish message and new privileges
        else if(val && opriv >= PRIV_MASTER && z_canseemypriv(ci, cj, opriv) && !z_canseemypriv(ci, cj))
        {
            packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
            putint(p, N_SERVMSG);
            sendstring(tempformatstring("%s %s %s%s", colorname(ci), "relinquished", z_isinvpriv(ci, opriv) ? "invisible " : "", privname(opriv)), p);
            putint(p, N_CURRENTMASTER);
            putint(p, mastermode);
            loopvj(clients) if(clients[j]->privilege >= PRIV_MASTER && z_canseemypriv(clients[j], cj))
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
