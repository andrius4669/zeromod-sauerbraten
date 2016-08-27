#ifdef Z_AUTOSENDMAP_H
#error "already z_autosendmap.h"
#endif
#define Z_AUTOSENDMAP_H

#ifndef Z_TRIGGERS_H
#error "want z_triggers.h"
#endif

#ifndef Z_SENDMAP_H
#error "want z_sendmap.h"
#endif

int z_autosendmap = 0;
VARFN(autosendmap, defaultautosendmap, 0, 0, 2, { if(clients.empty()) z_autosendmap = defaultautosendmap; });
static void z_autosendmap_trigger(int type) { z_autosendmap = defaultautosendmap; }
Z_TRIGGER(z_autosendmap_trigger, Z_TRIGGER_STARTUP);
Z_TRIGGER(z_autosendmap_trigger, Z_TRIGGER_NOCLIENTS);

static void z_servcmd_autosendmap(int argc, char **argv, int sender)
{
    int val = argc >= 2 ? atoi(argv[1]) : -1;

    static const char * const modes[3] = { "disabled", "enabled", "enabled with crc check" };
    const char *mstr = modes[clamp((val < 0 ? z_autosendmap : val), 0, 2)];

    if(val < 0) sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("autosendmap is %s", mstr));
    else
    {
        z_autosendmap = clamp(val, 0, 2);
        sendservmsgf("autosendmap %s", mstr);
    }
}
SCOMMANDA(autosendmap, PRIV_MASTER, z_servcmd_autosendmap, 1);
SCOMMANDA(connectsendmap, ZC_HIDDEN | PRIV_MASTER, z_servcmd_autosendmap, 1);
