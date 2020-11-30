
static string userserverdesc;
static string computedserverdesc[2]; // 0 - unfiltered, 1 - filtered

SVARF(serverdesctmpl, "%s", z_serverdescchanged()); // template to use with user settable stuff
VAR(serverdescfilter, 0, 1, 1); // whether stuff sent to serverlist is filtered
VAR(serverdescuserfilter, 0, 1, 1); // whether user settable stuff is filtered

static inline const char *z_serverdesc(bool shouldfilter)
{
    return computedserverdesc[shouldfilter && serverdescfilter];
}

static bool z_serverdesc_recompute()
{
    string oldcomputed;
    memcpy(oldcomputed, computedserverdesc[0], sizeof(oldcomputed));
    if(!userserverdesc[0]) copystring(computedserverdesc[0], serverdesc);
    else
    {
        z_formattemplate tmp[] =
        {
            { 's', "%s", (const void *)userserverdesc },
            { 0, NULL, NULL }
        };
        z_format(computedserverdesc[0], sizeof(computedserverdesc[0]), serverdesctmpl, tmp);
    }
    filtertext(computedserverdesc[1], computedserverdesc[0]);
    return strcmp(computedserverdesc[0], oldcomputed) != 0;
}

static void z_serverdesc_broadcast()
{
    loopv(clients)
    {
        clientinfo &ci = *clients[i];
        if(ci.state.aitype == AI_NONE)
        {
            sendf(ci.clientnum, 1, "ri5ss", N_SERVINFO, ci.clientnum, PROTOCOL_VERSION, ci.sessionid, 0, z_serverdesc(false), serverauth);
        }
    }
}

static void z_servcmd_servname(int argc, char **argv, int sender)
{
    if(argc < 2)
    {
        memset(userserverdesc, 0, sizeof(userserverdesc));
        sendservmsgf("%s unset serverdesc", colorname(getinfo(sender)));
    }
    else
    {
        if(serverdescuserfilter) filtertext(userserverdesc, argv[1]);
        else copystring(userserverdesc, argv[1]);
        sendservmsgf("%s set serverdesc to %s", colorname(getinfo(sender)), userserverdesc);
    }
    if(z_serverdesc_recompute()) z_serverdesc_broadcast();
}
SCOMMANDAH(servname, ZC_DISABLED | PRIV_AUTH, z_servcmd_servname, 1);
SCOMMANDAH(servdesc, ZC_DISABLED | PRIV_AUTH, z_servcmd_servname, 1);
SCOMMANDAH(servername, ZC_DISABLED | PRIV_AUTH, z_servcmd_servname, 1);
SCOMMANDA(serverdesc, ZC_DISABLED | PRIV_AUTH, z_servcmd_servname, 1);


static void z_serverdescchanged()
{
    if(z_serverdesc_recompute()) z_serverdesc_broadcast();
}

static void z_serverdesc_trigger(int type)
{
    memset(userserverdesc, 0, sizeof(userserverdesc));
    z_serverdesc_recompute();
}
Z_TRIGGER(z_serverdesc_trigger, Z_TRIGGER_STARTUP);
Z_TRIGGER(z_serverdesc_trigger, Z_TRIGGER_NOCLIENTS);
