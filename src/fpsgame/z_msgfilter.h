#ifndef Z_MSGFILTER_H
#define Z_MSGFILTER_H

#include "z_rename.h"

// message checking for editmute and flood protection
bool allowmsg(clientinfo *ci, clientinfo *cq, int type)
{
    if(!ci || ci->local) return true;
    switch(type)
    {
        case N_EDITMODE:
            if(smode==&racemode)
            {
                racemode.racecheat(ci, 1);
                return true;
            }
            return true;

        case N_TEXT:
        case N_SAYTEAM:
            if(cq && z_checkchatmute(ci, cq))
            {
                if(!cq->xi.lastchat || totalmillis-cq->xi.lastchat>=1000)
                {
                    cq->xi.lastchat = totalmillis ? totalmillis : 1;
                    if(cq->state.aitype == AI_NONE) sendf(cq->clientnum, 1, "ris", N_SERVMSG, "your text messages are muted");
                }
                return false;
            }
            return true;

        case N_EDITF: case N_EDITT: case N_EDITM:
        case N_FLIP: case N_ROTATE: case N_REPLACE: case N_DELCUBE:
        case N_EDITENT: case N_EDITVAR:
        case N_COPY: case N_CLIPBOARD: case N_PASTE:
        case N_REMIP: case N_NEWMAP:
#ifndef OLDPROTO
        case N_EDITVSLOT: case N_UNDO: case N_REDO:
#endif
            if(smode==&racemode && !racemode_allowedit)
            {
                if(type == N_COPY || type == N_CLIPBOARD) ci->cleanclipboard();
                if(type == N_REMIP) return false;   /* drop messages, but don't disqualify client for them */
                racemode.racecheat(ci, 2);
                return false;
            }
            if(z_isghost(*ci))
            {
                if(type == N_COPY || type == N_CLIPBOARD) ci->cleanclipboard();
                return false;   // just drop without warning
            }
            if(ci->xi.editmute)
            {
                if(type == N_COPY || type == N_CLIPBOARD) ci->cleanclipboard();
                const char *msg;
                switch(type)
                {
                    case N_REMIP: msg = "your remip message was muted"; break;
                    case N_NEWMAP: msg = "your newmap message was muted"; break;
                    default:
                        if(!ci->xi.lastedit || totalmillis-ci->xi.lastedit>=10000)
                        {
                            ci->xi.lastedit = totalmillis ? totalmillis : 1;
                            msg = "your map editing message was muted";
                        }
                        else msg = NULL;
                        break;
                }
                if(msg) sendf(ci->clientnum, 1, "ris", N_SERVMSG, msg);
                return false;
            }
            return true;

        case N_SWITCHNAME:
            if(ci->xi.namemute)
            {
                z_rename(ci, ci->name, false);
                return false;
            }
            return true;

        case N_TAUNT:
            return cq && cq->state.state==CS_ALIVE && !z_isghost(*cq);

        default: return true;
    }
}

static inline bool z_allowsound(clientinfo *ci, clientinfo *cq, int sound)
{
    // allow some fun
    return cq && (cq->state.state==CS_ALIVE || cq->state.state==CS_EDITING) && !z_isghost(*cq) && sound >= S_JUMP && sound <= S_FLAGFAIL;
}

// included here basically because this is included in right place in server.cpp
int z_timeleft()
{
    if(smode==&racemode) return racemode.secsleft();
    return -1;
}

#endif // Z_MSGFILTER_H
