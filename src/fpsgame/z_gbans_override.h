#ifndef Z_GBANS_OVERRIDE_H
#define Z_GBANS_OVERRIDE_H

#include "z_triggers.h"
#include "z_servcmd.h"
#include "z_tree.h"

// gban/pban struct optimised for use in tree
struct gbaninfo
{
    // in host byte order, cause easier to compare
    uint first, last;

    void parse(const char *name)
    {
        union { uchar b[sizeof(enet_uint32)]; enet_uint32 i; } ipconv, maskconv;
        ipconv.i = 0;
        maskconv.i = 0;
        loopi(4)
        {
            char *end = NULL;
            int n = strtol(name, &end, 10);
            if(!end) break;
            if(end > name) { ipconv.b[i] = n; maskconv.b[i] = 0xFF; }
            name = end;
            while(int c = *name)
            {
                ++name;
                if(c == '.') break;
                if(c == '/')
                {
                    int range = clamp(int(strtol(name, NULL, 10)), 0, 32);
                    int mask = range ? 0xFFffFFff << (32 - range) : ENET_NET_TO_HOST_32(maskconv.i);
                    first = ENET_NET_TO_HOST_32(ipconv.i) & mask;
                    last = first | ~mask;
                    return;
                }
            }
        }
        first = ENET_NET_TO_HOST_32(ipconv.i & maskconv.i);
        last = ENET_NET_TO_HOST_32(ipconv.i | ~maskconv.i);
    }

    int print(char *buf) const
    {
        char *start = buf;
        union { uchar b[sizeof(enet_uint32)]; enet_uint32 i; } ipconv, maskconv;
        ipconv.i = ENET_HOST_TO_NET_32(first);
        maskconv.i = ~ENET_HOST_TO_NET_32(first ^ last);
        int lastdigit = -1;
        loopi(4) if(maskconv.b[i])
        {
            if(lastdigit >= 0) *buf++ = '.';
            loopj(i - lastdigit - 1) { *buf++ = '*'; *buf++ = '.'; }
            buf += sprintf(buf, "%d", ipconv.b[i]);
            lastdigit = i;
        }
        enet_uint32 bits = first ^ last;
        int range = 32;
        for(; (bits&0xFF) == 0xFF; bits >>= 8) range -= 8;
        for(; bits&1; bits >>= 1) --range;
        if(!bits && range%8) buf += sprintf(buf, "/%d", range);
        return int(buf-start);
    }

    inline int compare(const gbaninfo &b) const
    {
        if(b.last < first) return -1;
        if(b.first > last) return +1;
        return 0;   /* ranges overlap */
    }

    // host byte order
    inline int compare(uint ip) const
    {
        if(ip < first) return -1;
        if(ip > last) return +1;
        return 0;
    }

    // host byte order
    bool check(uint ip) const { return (ip | (first ^ last)) == last; }
};

// basic pban struct with comments
struct pbaninfo: ipmask
{
    char *comment;
    pbaninfo(): comment(NULL) {}
    ~pbaninfo() { delete[] comment; }
};

vector< z_avltree<gbaninfo> > gbans;
z_avltree<gbaninfo> pbans;
vector<pbaninfo> sbans;

static void cleargbans(int m = -1)
{
    if(m < 0) gbans.shrink(0);
    else if(gbans.inrange(m)) gbans[m].clear();
}

VAR(showbanreason, 0, 0, 1);
VAR(showbanrange, 0, 0, 1);

static bool checkgban(uint ip, clientinfo *ci, bool connect = false)
{
    uint hip = ENET_NET_TO_HOST_32(ip);
    gbaninfo *p;
    if((p = pbans.find(hip)) && p->check(hip)) return true;
    loopv(sbans) if(sbans[i].check(ip))
    {
        if(connect && showbanreason && sbans[i].comment)
        {
            if(showbanrange)
            {
                string buf;
                sbans[i].print(buf);
                ci->xi.setdiscreason(tempformatstring("ip/range you are connecting from (%s) is banned because: %s", buf, sbans[i].comment));
            }
            else ci->xi.setdiscreason(tempformatstring("ip/range you are connecting from is banned because: %s", sbans[i].comment));
        }
        return true;
    }
    loopv(gbans) if((p = gbans[i].find(hip)) && p->check(hip))
    {
        const char *s;
        if((s = getmasternetident(i)))
        {
            if(!ci->xi.geoip.network) ci->xi.geoip.network = newstring(s);
            return false;
        }
        if(connect && showbanreason && (s = getmasterbanmsg(i)))
        {
            if(showbanrange)
            {
                string buf;
                p->print(buf);
                ci->xi.setdiscreason(tempformatstring("ip/range you are connecting from (%s) is banned because: %s", buf, s));
            }
            else ci->xi.setdiscreason(tempformatstring("ip/range you are connecting from is banned because: %s", s));
        }
        if((s = getmasterwlauth(i)))
        {
            if(!connect)
            {
                if(ci->xi.wlauth && !strcmp(ci->xi.wlauth, s)) return false;
            }
            else ci->xi.setwlauth(s);
        }
        return true;
    }
    return false;
}

static void addgban(int m, const char *name, clientinfo *actor = NULL, const char *comment = NULL, bool save = false)
{
    if(!save)
    {
        gbaninfo ban, *old;
        ban.parse(name);
        if(m >= 0)
        {
            while(gbans.length() <= m) gbans.add();
            if(!gbans[m].add(ban, &old))
            {
                string buf;
                old->print(buf);
                logoutf("WARNING: addgban[%d]: %s conflicts with existing %s", m, name, buf);
            }
        }
        else
        {
            if(!pbans.add(ban, &old))
            {
                string buf;
                old->print(buf);
                logoutf("WARNING: pban: %s conflicts with existing %s", name, buf);
            }
        }
    }
    else
    {
        pbaninfo &ban = sbans.add();
        ban.parse(name);
        if(comment && *comment) ban.comment = newstring(comment);
    }

    loopvrev(clients)
    {
        clientinfo *ci = clients[i];
        if(ci->state.aitype != AI_NONE || ci->local || ci->privilege >= PRIV_ADMIN) continue;
        if(actor && ((ci->privilege > actor->privilege && !actor->local) || ci->clientnum == actor->clientnum)) continue;
        if(checkgban(getclientip(ci->clientnum), ci)) disconnect_client(ci->clientnum, DISC_IPBAN);
    }
}

void pban(char *name, int *save, char *comment) { addgban(-1, name, NULL, comment, (*save)!=0); }
COMMAND(pban, "sis");

void clearpbans(int *all)
{
    pbans.clear();
    if(*all) sbans.shrink(0);
}
COMMAND(clearpbans, "i");

static void z_savepbans()
{
    stream *f = NULL;
    string buf;
    loopv(sbans)
    {
        if(!f)
        {
            f = openutf8file(path("pbans.cfg", true), "w"); /* executed after server-init.cfg */
            if(!f) return;
            f->printf("// list of persistent bans. do not edit this file directly while server is running\n");
        }
        sbans[i].print(buf);
        if(sbans[i].comment) f->printf("pban %s 1 %s\n", buf, escapestring(sbans[i].comment));
        else f->printf("pban %s 1\n", buf);
    }
    if(f) delete f;
    else remove(findfile(path("pbans.cfg", true), "r"));
}

void z_trigger_loadpbans(int type)
{
    atexit(z_savepbans);
    execfile("pbans.cfg", false);
}
Z_TRIGGER(z_trigger_loadpbans, Z_TRIGGER_STARTUP);

void z_servcmd_listpbans(int argc, char **argv, int sender)
{
    string buf;
    sendf(sender, 1, "ris", N_SERVMSG, sbans.empty() ? "pbans list is empty" : "pbans list:");
    loopv(sbans)
    {
        sbans[i].print(buf);
        sendf(sender, 1, "ris", N_SERVMSG, sbans[i].comment
            ? tempformatstring("%02d %s %s", i, buf, sbans[i].comment)
            : tempformatstring("%02d %s",    i, buf));
    }
}
SCOMMANDA(listpbans, PRIV_ADMIN, z_servcmd_listpbans, 1);

void z_servcmd_unpban(int argc, char **argv, int sender)
{
    string buf;
    if(argc < 2) { sendf(sender, 1, "ris", N_SERVMSG, "please specify pban id"); return; }

    char *end = NULL;
    int id = int(strtol(argv[1], &end, 10));
    if(!end) { sendf(sender, 1, "ris", N_SERVMSG, "incorrect pban id"); return; }

    if(id < 0)
    {
        loopv(sbans)
        {
            sbans[i].print(buf);
            sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("removing pban for %s", buf));
        }
        if(sbans.empty()) sendf(sender, 1, "ris", N_SERVMSG, "no pbans found");
        sbans.shrink(0);
    }
    else
    {
        if(!sbans.inrange(id)) { sendf(sender, 1, "ris", N_SERVMSG, "incorrect pban id"); return; }
        sbans[id].print(buf);
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("removing pban for %s", buf));
        sbans.remove(id);
    }
}
SCOMMANDA(unpban, PRIV_ADMIN, z_servcmd_unpban, 1);

void z_servcmd_pban(int argc, char **argv, int sender)
{
    if(argc < 2) { sendf(sender, 1, "ris", N_SERVMSG, "please specify client number"); return; }
    char *end = NULL, *range = strchr(argv[1], '/');
    if(range) *range++ = 0;
    int cn = (int)strtol(argv[1], &end, 10);
    clientinfo *ci;
    if(!end || !(ci = (clientinfo *)getclientinfo(cn))) { sendf(sender, 1, "ris", N_SERVMSG, "incorrect client number"); return; }
    if(ci->local) { sendf(sender, 1, "ris", N_SERVMSG, "you cannot ban local client"); return; }
    int r;
    if(range)
    {
        end = NULL;
        r = clamp((int)strtol(range, &end, 10), 0, 32);
        if(!end || *end) r = 32;    /* failed to read int or string doesn't end after integer */
    }
    else r = 32;

    pbaninfo &b = sbans.add();
    b.mask = ENET_HOST_TO_NET_32(0xFFffFFff << (32 - r));
    b.ip = getclientip(cn) & b.mask;
    if(argc > 2 && argv[2] && *argv[2]) b.comment = newstring(argv[2]);

    string buf;
    b.print(buf);
    sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("adding pban for %s", buf));

    clientinfo *actor = getinfo(sender);
    loopvrev(clients)
    {
        ci = clients[i];
        if(ci->state.aitype != AI_NONE || ci->local || ci->privilege >= PRIV_ADMIN) continue;
        if((ci->privilege > actor->privilege && !actor->local) || ci->clientnum == actor->clientnum) continue;
        if(checkgban(getclientip(ci->clientnum), ci)) disconnect_client(ci->clientnum, DISC_IPBAN);
    }
}
SCOMMANDA(pban, PRIV_ADMIN, z_servcmd_pban, 2);

void z_servcmd_pbanip(int argc, char **argv, int sender)
{
    if(argc < 2) { sendf(sender, 1, "ris", N_SERVMSG, "please specify ip address"); return; }
    sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("adding pban for %s", argv[1]));
    addgban(-1, argv[1], getinfo(sender), argc > 2 ? argv[2] : NULL, true);
}
SCOMMANDA(pbanip, PRIV_ADMIN, z_servcmd_pbanip, 2);

#endif // Z_GBANS_OVERRIDE_H
