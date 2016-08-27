#ifdef Z_MUTES_H
#error "already z_mutes.h"
#endif
#define Z_MUTES_H

#ifndef Z_SERVCMD_H
#error "want z_servcmd.h"
#endif
#ifndef Z_TRIGGERS_H
#error "want z_triggers.h"
#endif
#ifndef Z_RENAME_H
#error "want z_rename.h"
#endif


// used literally only for sendmap check
static inline bool z_iseditmuted(clientinfo *ci)
{
    return ci->xi.editmute
        || (smode==&racemode && !racemode_allowedit)
        || z_shouldblockgameplay(ci);
}

static void z_servcmd_mute(int argc, char **argv, int sender)
{
    int val = -1, cn, mutetype = 0;
    clientinfo *ci = NULL, *actor = getinfo(sender);
    const char *minfo = NULL;
    if(argc < 2) { z_servcmd_pleasespecifyclient(sender); return; }
    if(argc > 2) val = clamp(atoi(argv[2]), 0, 1);

    if(!z_parseclient_verify(argv[1], cn, true, true))
    {
        z_servcmd_unknownclient(argv[1], sender);
        return;
    }
    if(cn >= 0) ci = getinfo(cn);

    if(!strcasecmp(argv[0], "mute")) mutetype = 1;
    else if(!strcasecmp(argv[0], "unmute")) { mutetype = 1; val = 0; }
    else if(!strcasecmp(argv[0], "editmute") || !strcasecmp(argv[0], "emute")) mutetype = 2;
    else if(!strcasecmp(argv[0], "editunmute") || !strcasecmp(argv[0], "eunmute")) { mutetype = 2; val = 0; }
    else if(!strcasecmp(argv[0], "nmute") || !strcasecmp(argv[0], "namemute")) { mutetype = 3; val = 1; if(argc > 2) minfo = argv[2]; }
    else if(!strcasecmp(argv[0], "nunmute") || !strcasecmp(argv[0], "nameunmute")) { mutetype = 3; val = 0; }

    if(mutetype == 1)
    {
        if(ci)
        {
            if(ci->xi.chatmute != (val ? 1 : 0))
            {
                ci->xi.chatmute = val ? 1 : 0;
                sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("you %s %s", val ? "muted" : "unmuted", colorname(ci)));
                if(ci->state.aitype == AI_NONE) sendf(ci->clientnum, 1, "ris", N_SERVMSG, tempformatstring("you got %s", val ? "muted" : "unmuted"));
            }
        }
        else loopv(clients) if(!clients[i]->spy)
        {
            ci = clients[i];
            if(ci->xi.chatmute == (val ? 1 : 0)) continue;
            sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("you %s %s", val ? "muted" : "unmuted", colorname(ci)));
            ci->xi.chatmute = val ? 1 : 0;
            if(ci->state.aitype == AI_NONE) sendf(ci->clientnum, 1, "ris", N_SERVMSG, tempformatstring("you got %s", val ? "muted" : "unmuted"));
        }
    }
    else if(mutetype == 2)
    {
        if(ci)
        {
            if(ci->state.aitype == AI_NONE && ci->xi.editmute != (val!=0))
            {
                sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("you edit-%s %s", val ? "muted" : "unmuted", colorname(ci)));
                ci->xi.editmute = val!=0;
                sendf(ci->clientnum, 1, "ris", N_SERVMSG, tempformatstring("you got edit-%s", val ? "muted" : "unmuted"));
            }
        }
        else loopv(clients)
        {
            ci = clients[i];
            if(ci->state.aitype != AI_NONE || ci->spy || ci->xi.editmute == (val!=0)) continue;
            sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("you edit-%s %s", val ? "muted" : "unmuted", colorname(ci)));
            ci->xi.editmute = val!=0;
            sendf(ci->clientnum, 1, "ris", N_SERVMSG, tempformatstring("you got edit-%s", val ? "muted" : "unmuted"));
        }
    }
    else if(mutetype == 3)
    {
        if(ci)
        {
            if(ci->state.aitype == AI_NONE)
            {
                ci->xi.namemute = val!=0;
                if(val && minfo)
                {
                    string name;
                    filtertext(name, minfo, false, false, MAXNAMELEN);
                    if(!name[0]) copystring(name, "unnamed");
                    if(isdedicatedserver()) logoutf("rename: %s (%d) is now known as %s by %s (%d)",
                        ci->name, ci->clientnum, name, actor->name, actor->clientnum);
                    copystring(ci->name, name);
                    z_rename(ci, name);
                }
                sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("you name-%s %s", val ? "muted" : "unmuted", colorname(ci)));
                sendf(ci->clientnum, 1, "ris", N_SERVMSG, tempformatstring("you got name-%s", val ? "muted" : "unmuted"));
            }
        }
        else loopv(clients)
        {
            ci = clients[i];
            if(ci->state.aitype != AI_NONE || ci->spy) continue;
            ci->xi.namemute = val!=0;
            if(val && minfo)
            {
                string name;
                filtertext(name, minfo, false, false, MAXNAMELEN);
                if(!name[0]) copystring(name, "unnamed");
                if(isdedicatedserver()) logoutf("rename: %s (%d) is now known as %s by %s (%d)",
                    ci->name, ci->clientnum, name, actor->name, actor->clientnum);
                copystring(ci->name, name);
                z_rename(ci, name);
            }
            sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("you name-%s %s", val ? "muted" : "unmuted", colorname(ci)));
            sendf(ci->clientnum, 1, "ris", N_SERVMSG, tempformatstring("you got name-%s", val ? "muted" : "unmuted"));
        }
    }
}
SCOMMANDA(mute, PRIV_AUTH, z_servcmd_mute, 2);
SCOMMANDA(unmute, PRIV_AUTH, z_servcmd_mute, 1);
SCOMMANDA(editmute, PRIV_MASTER, z_servcmd_mute, 2);
SCOMMANDA(editunmute, PRIV_MASTER, z_servcmd_mute, 1);
SCOMMANDAH(emute, PRIV_MASTER, z_servcmd_mute, 2);
SCOMMANDAH(eunmute, PRIV_MASTER, z_servcmd_mute, 1);
SCOMMANDA(namemute, PRIV_AUTH, z_servcmd_mute, 2);
SCOMMANDA(nameunmute, PRIV_AUTH, z_servcmd_mute, 1);
SCOMMANDAH(nmute, PRIV_AUTH, z_servcmd_mute, 2);
SCOMMANDAH(nunmute, PRIV_AUTH, z_servcmd_mute, 1);

bool z_autoeditmute = false;
VARFN(autoeditmute, z_defaultautoeditmute, 0, 0, 1, { if(clients.empty()) z_autoeditmute = z_defaultautoeditmute!=0; });
static void z_trigger_autoeditmute(int type)
{
    z_autoeditmute = z_defaultautoeditmute!=0;
}
Z_TRIGGER(z_trigger_autoeditmute, Z_TRIGGER_STARTUP);
Z_TRIGGER(z_trigger_autoeditmute, Z_TRIGGER_NOCLIENTS);

void z_servcmd_autoeditmute(int argc, char **argv, int sender)
{
    int val = -1;
    if(argc > 1) val = atoi(argv[1]);
    if(val >= 0)
    {
        z_autoeditmute = val!=0;
        sendservmsgf("autoeditmute %s", z_autoeditmute ? "enabled" : "disabled");
    }
    else sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("autosendmute is %s", z_autoeditmute ? "enabled" : "disabled"));
}
SCOMMANDA(autoeditmute, PRIV_MASTER, z_servcmd_autoeditmute, 1);
