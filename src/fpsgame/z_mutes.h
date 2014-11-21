#ifndef Z_MUTES_H
#define Z_MUTES_H 1

#include "z_servcmd.h"
#include "z_triggers.h"

static void z_servcmd_mute(int argc, char **argv, int sender)
{
    int val = -1, cn, mutetype = 0;
    clientinfo *ci = NULL, *actor = getinfo(sender);
    const char *minfo = NULL;
    if(argc < 2) { sendf(sender, 1, "ris", N_SERVMSG, "please specify client"); return; }
    if(argc > 2) val = clamp(atoi(argv[2]), 0, 1);

    if(!z_parseclient_verify(argv[1], &cn, true, true))
    {
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("unknown client: %s", argv[1]));
        return;
    }
    if(cn >= 0) ci = getinfo(cn);

    if(!strcmp(argv[0], "mute")) mutetype = 1;
    else if(!strcmp(argv[0], "unmute")) { mutetype = 1; val = 0; }
    else if(!strcmp(argv[0], "editmute") || !strcmp(argv[0], "emute")) mutetype = 2;
    else if(!strcmp(argv[0], "editunmute") || !strcmp(argv[0], "eunmute")) { mutetype = 2; val = 0; }
    else if(!strcmp(argv[0], "nmute") || !strcmp(argv[0], "namemute")) { mutetype = 3; val = 1; if(argc > 2) minfo = argv[2]; }
    else if(!strcmp(argv[0], "nunmute") || !strcmp(argv[0], "nameunmute")) { mutetype = 3; val = 0; }

    if(mutetype == 1)
    {
        if(ci)
        {
            ci->chatmute = val!=0;
            sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("you %s %s", val ? "muted" : "unmuted", colorname(ci)));
            if(ci->state.aitype == AI_NONE) sendf(ci->clientnum, 1, "ris", N_SERVMSG, tempformatstring("you got %s", val ? "muted" : "unmuted"));
        }
        else loopv(clients)
        {
            ci = clients[i];
            ci->chatmute = val!=0;
            sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("you %s %s", val ? "muted" : "unmuted", colorname(ci)));
            if(ci->state.aitype == AI_NONE) sendf(ci->clientnum, 1, "ris", N_SERVMSG, tempformatstring("you got %s", val ? "muted" : "unmuted"));
        }
    }
    else if(mutetype == 2)
    {
        if(ci)
        {
            if(ci->state.aitype == AI_NONE)
            {
                ci->editmute = val!=0;
                sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("you edit-%s %s", val ? "muted" : "unmuted", colorname(ci)));
                sendf(ci->clientnum, 1, "ris", N_SERVMSG, tempformatstring("you got edit-%s", val ? "muted" : "unmuted"));
            }
        }
        else loopv(clients)
        {
            ci = clients[i];
            if(ci->state.aitype != AI_NONE) continue;
            ci->editmute = val!=0;
            sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("you edit-%s %s", val ? "muted" : "unmuted", colorname(ci)));
            sendf(ci->clientnum, 1, "ris", N_SERVMSG, tempformatstring("you got edit-%s", val ? "muted" : "unmuted"));
        }
    }
    else if(mutetype == 3)
    {
        if(ci)
        {
            if(ci->state.aitype == AI_NONE)
            {
                ci->namemute = val!=0;
                if(val && minfo)
                {
                    string name;
                    filtertext(name, minfo, false, MAXNAMELEN);
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
            if(ci->state.aitype != AI_NONE) continue;
            ci->namemute = val!=0;
            if(val && minfo)
            {
                string name;
                filtertext(name, minfo, false, MAXNAMELEN);
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
SCOMMANDNA(mute, PRIV_AUTH, z_servcmd_mute, 2);
SCOMMANDNA(unmute, PRIV_AUTH, z_servcmd_mute, 1);
SCOMMANDNA(editmute, PRIV_MASTER, z_servcmd_mute, 2);
SCOMMANDNA(editunmute, PRIV_MASTER, z_servcmd_mute, 1);
SCOMMANDNAH(emute, PRIV_MASTER, z_servcmd_mute, 2);
SCOMMANDNAH(eunmute, PRIV_MASTER, z_servcmd_mute, 1);
SCOMMANDNA(namemute, PRIV_AUTH, z_servcmd_mute, 2);
SCOMMANDNA(nameunmute, PRIV_AUTH, z_servcmd_mute, 1);
SCOMMANDNAH(nmute, PRIV_AUTH, z_servcmd_mute, 2);
SCOMMANDNAH(nunmute, PRIV_AUTH, z_servcmd_mute, 1);

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
SCOMMANDNA(autoeditmute, PRIV_MASTER, z_servcmd_autoeditmute, 1);

#endif // Z_MUTES_H
