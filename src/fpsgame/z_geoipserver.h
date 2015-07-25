/* geoip subsystem part dependent on game/server.cpp */

#ifndef Z_GEOIPSERVER_H
#define Z_GEOIPSERVER_H

#include "z_geoipstate.h"
#include "z_geoip.h"

static bool checkgeoipban(clientinfo *ci)
{
    geoipstate &gs = ci->xi.geoip;
    if(geoip_ban_anonymous && gs.anonymous) return true;
    loopv(z_geoipbans)
    {
        const char *str;
        switch(z_geoipbans[i].type)
        {
            case GIB_COUNTRY: default: str = gs.country; break;
            case GIB_CONTINENT: str = gs.continent; break;
        }
        if(!strcasestr(str, z_geoipbans[i].key))
        {
            if(z_geoipbans[i].message && !geoip_ban_mode) ci->xi.setdiscreason(z_geoipbans[i].message);
            return !geoip_ban_mode;
        }
    }
    return !!geoip_ban_mode;
}

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

VAR(geoip_log, 0, 0, 2);

void z_geoip_log(clientinfo *ci)
{
    if(!geoip_log) return;
    vector<char> buf;
    z_geoip_print(buf, ci, geoip_log > 1);
    if(buf.length())
    {
        buf.add('\0');
        logoutf("geoip: client %d connected from %s", ci->clientnum, buf.getbuf());
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
                    z_format(msg, sizeof(msg), geoip_style_normal, ft);
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
                    ft[3].ptr = NULL;
                    z_format(msg, sizeof(msg), geoip_style_local, ft);
                }
                else
                {
                    *msg = 0;
                }
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

void z_geoip_print_query(char *msg, size_t len, clientinfo *ci, bool isadmin)
{
    if(ci->state.aitype==AI_NONE)
    {
        if(!ci->local)
        {
            vector<char> buf;
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
                z_format(msg, len, geoip_style_normal_query, ft);
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
                    { 0, NULL, NULL }
                };
                z_format(msg, len, geoip_style_local_query, ft);
            }
            else
            {
                *msg = 0;
            }
        }
    }
    else *msg = 0;
}

void z_servcmd_geoip(int argc, char **argv, int sender)
{
    clientinfo *ci, *sci = getinfo(sender);
    vector<clientinfo *> cis;
    const bool isadmin = sci->privilege>=PRIV_ADMIN || sci->local;

    for(int i = 1; i < argc; i++)
    {
        int cn;
        if(!z_parseclient_verify(argv[i], cn, true, true, isadmin))
        {
            z_servcmd_unknownclient(argv[i], sender);
            return;
        }
        if(cn < 0)
        {
            cis.shrink(0);
            loopvj(clients) if(clients[j]->state.aitype==AI_NONE && (!clients[j]->spy || isadmin)) cis.add(clients[j]);
            break;
        }
        ci = getinfo(cn);
        if(cis.find(ci)<0) cis.add(ci);
    }

    if(cis.empty()) { z_servcmd_pleasespecifyclient(sender); return; }

    loopv(cis)
    {
        ci = cis[i];

        string msg;
        z_geoip_print_query(msg, sizeof(msg), ci, isadmin);

        if(*msg) sendf(sender, 1, "ris", N_SERVMSG, msg);
        else
        {
            z_formattemplate ft[] =
            {
                { 'C', "%s", (const void *)colorname(ci) },
                { 'c', "%s", (const void *)ci->name },
                { 'n', "%d", (const void *)(long)ci->clientnum },
                { 0, NULL, NULL }
            };
            z_format(msg, sizeof(msg), geoip_style_failed_query, ft);
            sendf(sender, 1, "ris", N_SERVMSG, msg);
        }
    }
}
SCOMMAND(geoip, PRIV_NONE, z_servcmd_geoip);
SCOMMAND(getip, ZC_HIDDEN | PRIV_NONE, z_servcmd_geoip);

void z_servcmd_whois(int argc, char **argv, int sender)
{
    clientinfo * const sci = getinfo(sender);
    const bool isadmin = sci->privilege>=PRIV_ADMIN || sci->local;
    string msg;

    if(argc < 2) { z_servcmd_pleasespecifyclient(sender); return; }

    int cn;
    if(!z_parseclient_verify(argv[1], cn, false, false, isadmin))
    {
        z_servcmd_unknownclient(argv[1], sender);
        return;
    }
    clientinfo *ci = getinfo(cn);

    z_geoip_print_query(msg, sizeof(msg), ci, isadmin);
    if(*msg) sendf(sender, 1, "ris", N_SERVMSG, msg);

    if(ci->privilege > PRIV_NONE && z_canseemypriv(ci, sci))
    {
        if(ci->xi.claim.isset())
        {
            if(*ci->xi.claim.desc) formatstring(msg)("%s is claimed %s%s as '\fs\f5%s\fr' [\fs\f0%s\fr]",
                                                     colorname(ci), z_isinvpriv(ci, ci->privilege) ? "invisible " : "",
                                                     privname(ci->privilege), ci->xi.claim.name, ci->xi.claim.desc);
            else formatstring(msg)("%s is claimed %s%s as '\fs\f5%s\fr'",
                                   colorname(ci), z_isinvpriv(ci, ci->privilege) ? "invisible " : "",
                                   privname(ci->privilege), ci->xi.claim.name);
        }
        else
        {
            formatstring(msg)("%s is claimed %s%s", colorname(ci), z_isinvpriv(ci, ci->privilege) ? "invisible " : "", privname(ci->privilege));
        }
        sendf(sender, 1, "ris", N_SERVMSG, msg);
    }

    if(isadmin && ci->xi.ident.isset())
    {
        if(*ci->xi.ident.desc) formatstring(msg)("%s is identified as '\fs\f5%s\fr' [\fs\f0%s\fr]", colorname(ci),
                                                 ci->xi.ident.name, ci->xi.ident.desc);
        else formatstring(msg)("%s is identified as '\fs\f5%s\fr'", colorname(ci), ci->xi.ident.name);
        sendf(sender, 1, "ris", N_SERVMSG, msg);
    }

    uint mspassed = uint(totalmillis-ci->connectmillis);
    if(mspassed/1000 != 0)
    {
        vector<char> timebuf;
        z_formatsecs(timebuf, mspassed/1000);
        if(timebuf.length())
        {
            timebuf.add(0);
            sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("%s connected %s ago", colorname(ci), timebuf.getbuf()));
        }
    }
}
SCOMMAND(whois, PRIV_NONE, z_servcmd_whois);

ICOMMAND(s_geoip_resolveclients, "", (),
{
    loopv(clients) if(clients[i]->state.aitype == AI_NONE) z_geoip_resolveclient(clients[i]->xi.geoip, getclientip(clients[i]->clientnum));
});

#endif // Z_GEOIPSERVER_H
