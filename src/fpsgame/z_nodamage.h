#ifndef Z_NODAMAGE_H
#define Z_NODAMAGE_H

int z_nodamage = 0;
VARF(servernodamage, 0, 0, 2, { if(clients.empty()) z_nodamage = servernodamage; });
static void z_nodamage_trigger(int type)
{
    z_nodamage = servernodamage;
}
Z_TRIGGER(z_nodamage_trigger, Z_TRIGGER_STARTUP);
Z_TRIGGER(z_nodamage_trigger, Z_TRIGGER_NOCLIENTS);

VARF(servernodamage_global, 0, 1, 1,
{
    z_nodamage = servernodamage;
    if(servernodamage_global) { loopv(clients) clients[i]->nodamage = z_nodamage; }
    z_servcmd_set_privilege("nodamage", servernodamage_global ? PRIV_MASTER : PRIV_NONE);
});

static int z_hasnodamage(clientinfo *target, clientinfo *actor)
{
    if(!m_edit || !actor) return 0;
    return servernodamage_global ? z_nodamage : max(target->nodamage, actor->nodamage);
}

static void z_servcmd_nodamage(int argc, char **argv, int sender)
{
    int val = argc >= 2 ? atoi(argv[1]) : -1;
    clientinfo *senderci = getinfo(sender), *ci;

    if(servernodamage_global)
    {
        if(senderci->privilege < PRIV_MASTER && !senderci->local)
        {
            sendf(sender, 1, "ris", N_SERVMSG, "you need to claim master to execute this server command");
            return;
        }
        ci = NULL;
    }
    else
    {
        ci = senderci;
    }

    int &nodamage = ci ? ci->nodamage : z_nodamage;

    if(val < 0) sendf(sender, 1, "ris", N_SERVMSG,
        tempformatstring("nodamage is %s", nodamage <= 0 ? "disabled" : nodamage > 1 ? "enabled (without hitpush)" : "enabled"));
    else
    {
        nodamage = clamp(val, 0, 2);
        sendf(ci ? sender : -1, 1, "ris", N_SERVMSG,
              tempformatstring("nodamage %s", nodamage <= 0 ? "disabled" : nodamage > 1 ? "enabled (without hitpush)" : "enabled"));
    }

    if(servernodamage_global)
    {
        loopv(clients) clients[i]->nodamage = z_nodamage;
    }
}
SCOMMANDNA(nodamage, PRIV_NONE, z_servcmd_nodamage, 1);

#endif // Z_NODAMAGE_H