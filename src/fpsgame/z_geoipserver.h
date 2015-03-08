/* geoip subsystem part dependent on game/server.cpp */

#ifndef Z_GEOIPSERVER_H
#define Z_GEOIPSERVER_H

#include "z_geoipstate.h"
#include "z_geoip.h"

static void z_geoip_print(vector<char> &buf, clientinfo *ci, bool admin)
{
    const char *comp[] =
    {
        (admin ? geoip_show_ip        : geoip_show_ip == 1)        ? getclienthostname(ci->clientnum) : NULL,   // 0
        (admin ? geoip_show_network   : geoip_show_network == 1)   ? ci->xi.geoip.network             : NULL,   // 1
        (admin ? geoip_show_city      : geoip_show_city == 1)      ? ci->xi.geoip.city                : NULL,   // 2
        (admin ? geoip_show_region    : geoip_show_region == 1)    ? ci->xi.geoip.region              : NULL,   // 3
        (admin ? geoip_show_country   : geoip_show_country == 1)   ? ci->xi.geoip.country             : NULL,   // 4
        (admin ? geoip_show_continent : geoip_show_continent == 1) ? ci->xi.geoip.continent           : NULL    // 5
    };
    if(!comp[4] && !comp[5] && (admin ? geoip_show_country : geoip_show_country == 1) && ci->xi.geoip.continent)
        comp[5] = ci->xi.geoip.continent;
    int lastc = -1;
    loopi(sizeof(comp)/sizeof(comp[0])) if(comp[i])
    {
        if(geoip_skip_duplicates && lastc >= 0 && (geoip_skip_duplicates > 1 || (lastc + 1) == i) && !strcmp(comp[i], comp[lastc])) continue;
        lastc = i;
        if(buf.length()) { buf.add(','); buf.add(' '); }
        buf.put(comp[i], strlen(comp[i]));
    }
}

SVAR(geoip_style_normal, "%C connected from %L");
SVAR(geoip_style_normal_query, "%C is connected from %L");
SVAR(geoip_style_local, "%C connected as local client");
SVAR(geoip_style_local_query, "%C is connected as local client");
SVAR(geoip_style_failed_query, "failed to get any geoip information about %C");

void z_geoip_show(clientinfo *ci)
{
    if(!geoip_enable || !ci) return;

    z_formattemplate ft[] =
    {
        { 'C', "%s", (const void *)colorname(ci) },
        { 'c', "%s", (const void *)ci->name },
        { 'n', "%d", (const void *)(long)ci->clientnum },
        { 'L', "%s", (const void *)NULL },
        { 0, NULL, NULL }
    };

    packetbuf *qpacks[2] = { NULL, NULL };
    bool filled_in[2] = { false, false };
    for(int i = demorecord ? -1 : 0; i < clients.length(); i++) if(i < 0 || clients[i]->state.aitype==AI_NONE)
    {
        bool isadmin = i >= 0 && (clients[i]->privilege >= PRIV_ADMIN || clients[i]->local);
        int idx = isadmin ? 1 : 0;
        if(!filled_in[idx])
        {
            string msg;

            if(!ci->local)
            {
                vector<char> locationbuf;
                z_geoip_print(locationbuf, ci, isadmin);
                if(locationbuf.length())
                {
                    locationbuf.add(0);
                    ft[3].ptr = locationbuf.getbuf();
                    z_format(msg, sizeof msg, geoip_style_normal, ft);
                }
                else
                {
                    *msg = 0;
                }
            }
            else
            {
                ft[3].ptr = NULL;
                if((isadmin ? geoip_show_ip : geoip_show_ip == 1) || (isadmin ? geoip_show_network : geoip_show_network == 1))
                    z_format(msg, sizeof msg, geoip_style_local, ft);
                else
                    *msg = 0;
            }

            filled_in[idx] = true;
            if(*msg)
            {
                qpacks[idx] = new packetbuf(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
                packetbuf &p = *qpacks[idx];
                putint(p, N_SERVMSG);
                sendstring(msg, p);
                p.finalize();
            }
        }

        if(qpacks[idx])
        {
            if(i >= 0) sendpacket(clients[i]->clientnum, 1, qpacks[idx]->packet);
            else recordpacket(1, qpacks[idx]->packet->data, qpacks[idx]->packet->dataLength);
        }
    }

    loopi(2) DELETEP(qpacks[i]);
}

void z_servcmd_geoip(int argc, char **argv, int sender)
{
    string msg;
    int i, cn;
    clientinfo *ci, *senderci = getinfo(sender);
    vector<clientinfo *> cis;
    vector<char> buf;
    bool isadmin = senderci->privilege>=PRIV_ADMIN || senderci->local;
    for(i = 1; i < argc; i++)
    {
        if(!z_parseclient(argv[i], &cn)) goto fail;
        if(cn < 0)
        {
            cis.shrink(0);
            loopvj(clients) if(clients[j]->state.aitype==AI_NONE && (!clients[j]->spy || isadmin)) cis.add(clients[j]);
            break;
        }
        ci = getinfo(cn);
        if(!ci || ((!ci->connected || ci->spy) && !isadmin)) goto fail;
        if(cis.find(ci)<0) cis.add(ci);
    }

    if(cis.empty()) { sendf(sender, 1, "ris", N_SERVMSG, "please specify client number"); return; }

    loopv(cis)
    {
        ci = cis[i];
        if(ci->state.aitype==AI_NONE)
        {
            if(!ci->local)
            {
                buf.setsize(0);
                z_geoip_print(buf, ci, isadmin);
                if(buf.length())
                {
                    buf.add(0);
                    z_formattemplate ft[] =
                    {
                        { 'C', "%s", (const void *)colorname(ci) },
                        { 'c', "%s", (const void *)ci->name },
                        { 'n', "%d", (const void *)(long)ci->clientnum },
                        { 'L', "%s", (const void *)buf.getbuf() },
                        { 0, NULL, NULL }
                    };
                    z_format(msg, sizeof msg, geoip_style_normal_query, ft);
                }
                else
                {
                    *msg = 0;
                }
            }
            else
            {
                if((isadmin ? geoip_show_ip : geoip_show_ip == 1) || (isadmin ? geoip_show_network : geoip_show_network == 1))
                {
                    z_formattemplate ft[] =
                    {
                        { 'C', "%s", (const void *)colorname(ci) },
                        { 'c', "%s", (const void *)ci->name },
                        { 'n', "%d", (const void *)(long)ci->clientnum },
                        { 'L', "%s", (const void *)NULL },
                        { 0, NULL, NULL }
                    };
                    z_format(msg, sizeof msg, geoip_style_local_query, ft);
                }
                else
                {
                    *msg = 0;
                }
            }
        }
        else *msg = 0;

        if(*msg) sendf(sender, 1, "ris", N_SERVMSG, msg);
        else
        {
            z_formattemplate ft[] =
            {
                { 'C', "%s", (const void *)colorname(ci) },
                { 'c', "%s", (const void *)ci->name },
                { 'n', "%d", (const void *)(long)ci->clientnum },
                { 'L', "%s", (const void *)NULL },
                { 0, NULL, NULL }
            };
            z_format(msg, sizeof msg, geoip_style_failed_query, ft);
            sendf(sender, 1, "ris", N_SERVMSG, msg);
        }
    }
    return;
fail:
    sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("unknown client: %s", argv[i]));
}
SCOMMAND(geoip, PRIV_NONE, z_servcmd_geoip);
SCOMMANDH(getip, PRIV_NONE, z_servcmd_geoip);

ICOMMAND(s_geoip_resolveclients, "", (),
{
    loopv(clients) if(clients[i]->state.aitype == AI_NONE) z_geoip_resolveclient(clients[i]->xi.geoip, getclientip(clients[i]->clientnum));
});

#endif // Z_GEOIPSERVER_H
