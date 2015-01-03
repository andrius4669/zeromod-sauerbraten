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
            if(cq && cq->chatmute)
            {
                if(!cq->lastchat || totalmillis-cq->lastchat>=2000)
                {
                    cq->lastchat = totalmillis ? totalmillis : 1;
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
            if(smode==&racemode)
            {
                if(type == N_COPY || type == N_CLIPBOARD) ci->cleanclipboard();
                if(type == N_REMIP) return false;   /* drop messages, but don't disqualify client for them */
                racemode.racecheat(ci, 2);
                return false;
            }
            if(ci->editmute)
            {
                if(type == N_COPY || type == N_CLIPBOARD) ci->cleanclipboard();
                const char *msg;
                switch(type)
                {
                    case N_REMIP: msg = "your remip message was muted"; break;
                    case N_NEWMAP: msg = "your newmap message was muted"; break;
                    default:
                        if(!ci->lastedit || totalmillis-ci->lastedit>=10000)
                        {
                            ci->lastedit = totalmillis ? totalmillis : 1;
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
            if(ci->namemute)
            {
                z_rename(ci, ci->name, false);
                return false;
            }
            return true;

        default: return true;
    }
}

// included here basically because this is included in right place in server.cpp
int z_timeleft()
{
    if(smode==&racemode) return racemode.secsleft();
    return -1;
}

#endif // Z_MSGFILTER_H
