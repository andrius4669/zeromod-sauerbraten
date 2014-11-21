/* geoip subsystem part dependent on game/server.cpp */

#ifndef Z_GEOIPSERVER_H
#define Z_GEOIPSERVER_H

#include "z_geoipstate.h"
#include "z_geoip.h"

static void z_geoip_gencolors(char *cbuf, size_t len)
{
    size_t i = 0;
    for(; geoip_color_scheme[i] && i < len; i++)
        cbuf[i] = geoip_color_scheme[i] >= '0' && geoip_color_scheme[i] <= '9' ? geoip_color_scheme[i] : '7';
    for(; i < len; i++) cbuf[i] = i>0 ? cbuf[i-1] : '7';
}
template<size_t N> static inline void z_geoip_gencolors(char (&s)[N]) { z_geoip_gencolors(s, N); }

static void z_geoip_print(vector<char> &buf, clientinfo *ci, bool admin)
{
    const char *comp[] =
    {
        (admin ? geoip_show_ip : geoip_show_ip == 1) ? getclienthostname(ci->clientnum) : NULL,
        (admin ? geoip_show_network : geoip_show_network == 1) ? ci->geoip.network : NULL,
        (admin ? geoip_show_city : geoip_show_city == 1) ? ci->geoip.city : NULL,
        (admin ? geoip_show_region : geoip_show_region == 1) ? ci->geoip.region : NULL,
        (admin ? geoip_show_country : geoip_show_country == 1) ? ci->geoip.country : NULL,
        (admin ? geoip_show_continent : geoip_show_continent == 1) ? ci->geoip.continent : NULL
    };
    int lastc = -1;
    loopi(sizeof(comp)/sizeof(comp[0])) if(comp[i])
    {
        if(geoip_skip_duplicates && lastc >= 0 && (geoip_skip_duplicates > 1 || (lastc + 1) == i) && !strcmp(comp[i], comp[lastc])) continue;
        lastc = i;
        if(buf.length()) { buf.add(','); buf.add(' '); }
        buf.put(comp[i], strlen(comp[i]));
    }
    buf.add('\0');
}

void z_geoip_show(clientinfo *ci)
{
    if(!geoip_enable || !ci) return;

    char colors[3];
    z_geoip_gencolors(colors);

    vector<char> cbufs[2];
    packetbuf *qpacks[2] = { NULL, NULL };
    for(int i = demorecord ? -1 : 0; i < clients.length(); i++) if(i < 0 || clients[i]->state.aitype==AI_NONE)
    {
        bool isadmin = i >= 0 && (clients[i]->privilege >= PRIV_ADMIN || clients[i]->local);
        int idx = isadmin ? 1 : 0;
        if(!qpacks[idx])
        {
            if(cbufs[idx].empty())
            {
                if(ci->local)
                {
                    const bool show = (isadmin ? geoip_show_ip : geoip_show_ip == 1) || (isadmin ? geoip_show_network : geoip_show_network == 1);
                    if(show) cbufs[idx].put("local client", strlen("local client"));
                    cbufs[idx].add('\0');
                }
                else z_geoip_print(cbufs[idx], ci, isadmin);
            }
            if(cbufs[idx].length() <= 1) continue;
            qpacks[idx] = new packetbuf(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
            packetbuf &p = *qpacks[idx];
            putint(p, N_SERVMSG);
            sendstring(tempformatstring("\f%c%s \f%cconnected %s \f%c%s",
                                        colors[0], colorname(ci), colors[1], ci->local ? "as" : "from", colors[2], cbufs[idx].getbuf()), p);
            p.finalize();
        }
        if(i >= 0) sendpacket(clients[i]->clientnum, 1, qpacks[idx]->packet);
        else recordpacket(1, qpacks[idx]->packet->data, qpacks[idx]->packet->dataLength);
    }
    loopi(2) DELETEP(qpacks[i]);
}

void z_servcmd_geoip(int argc, char **argv, int sender)
{
    int i, cn;
    clientinfo *ci, *senderci = getinfo(sender);
    vector<clientinfo *> cis;
    vector<char> buf;
    char c[3];
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

    z_geoip_gencolors(c);

    for(i = 0; i < cis.length(); i++)
    {
        buf.setsize(0);
        if(cis[i]->state.aitype==AI_NONE)
        {
            if(cis[i]->local)
            {
                const bool show = (isadmin ? geoip_show_ip : geoip_show_ip == 1) || (isadmin ? geoip_show_network : geoip_show_network == 1);
                if(show) buf.put("local client", strlen("local client"));
                buf.add('\0');
            }
            else z_geoip_print(buf, cis[i], isadmin);
        }
        if(buf.length() > 1) sendf(sender, 1, "ris", N_SERVMSG,
            tempformatstring("\f%c%s \f%cis connected %s \f%c%s", c[0], colorname(cis[i]), c[1], cis[i]->local ? "as" : "from", c[2], buf.getbuf()));
        else sendf(sender, 1, "ris", N_SERVMSG,
            tempformatstring("\f%cfailed to get any geoip information about \f%c%s", c[1], c[0], colorname(cis[i])));
    }
    return;
fail:
    sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("unknown client: %s", argv[i]));
}
SCOMMAND(geoip, PRIV_NONE, z_servcmd_geoip);
SCOMMANDH(getip, PRIV_NONE, z_servcmd_geoip);

ICOMMAND(s_geoip_resolveclients, "", (),
{
    loopv(clients) if(clients[i]->state.aitype == AI_NONE) z_geoip_resolveclient(clients[i]->geoip, getclientip(clients[i]->clientnum));
});

#endif // Z_GEOIPSERVER_H
