#ifndef Z_GBANS_OVERRIDE_H
#define Z_GBANS_OVERRIDE_H

#include "z_triggers.h"
#include "z_servcmd.h"
#include "z_tree.h"

vector< z_avltree<gbaninfo> > gbans;
z_avltree<gbaninfo> pbans;
vector<pbaninfo> sbans;

static void cleargbans(int m = -1)
{
    if(m < 0) gbans.shrink(0);
    else if(gbans.inrange(m)) gbans[m].clear();
}

VAR(showbanreason, 0, 0, 1);

SVAR(ban_message_banned, "ip/range you are connecting from is banned because: %r");
// SVAR(ban_message_banned, "ip/range you are connecting from (%a) is banned because: %r");

SVAR(pbans_file, "pbans.cfg");
VAR(pbans_expire, 0, 0, INT_MAX);    // pbans expiration time, in seconds

static int nextpbanscheck = 0;

// remove pban from sbans after certain ammount of time
void checkexpiredpbans()
{
    nextpbanscheck = 0;
    if(!pbans_expire) return;
    time_t currsecs;
    time(&currsecs);
    loopvrev(sbans)
    {
        time_t timediff = currsecs - sbans[i].dateadded;
        if(timediff < 0) continue;
        if(timediff >= pbans_expire)
        {
            sbans.remove(i);
            continue;
        }
        // if time difference is more than can be expressed in long, clamp
        if(timediff > LONG_MAX/1000) timediff = LONG_MAX/1000;
        nextpbanscheck = max(nextpbanscheck, int(min(timediff*1000, time_t(INT_MAX-1))));
    }
    if(nextpbanscheck)
    {
        nextpbanscheck += totalmillis;
        if(!nextpbanscheck) nextpbanscheck = 1;
    }
}

static bool checkgban(uint ip, clientinfo *ci, bool connect = false)
{
    uint hip = ENET_NET_TO_HOST_32(ip);
    gbaninfo *p;

    if((p = pbans.find(hip)) && p->check(hip)) return true;

    loopv(sbans) if(sbans[i].check(ip))
    {
        time(&sbans[i].lasthit);
        if(connect && showbanreason && sbans[i].comment)
        {
            string addr, msg;
            sbans[i].print(addr);
            z_formattemplate ft[] =
            {
                { 'a', "%s", addr },
                { 'r', "%s", sbans[i].comment },
                { 0, 0, 0 }
            };
            z_format(msg, sizeof(msg), ban_message_banned, ft);
            if(*msg) ci->xi.setdiscreason(msg);
        }
        return true;
    }

    loopv(gbans)
    {
        const char *ident, *wlauth, *banmsg;
        int mode;
        if(!getmasterbaninfo(i, ident, mode, wlauth, banmsg)) continue;
        if(mode != 1 && mode != -1 && !ident) continue;   // do not perform lookup at all
        if((p = gbans[i].find(hip)) && p->check(hip))
        {
            if(ident && !ci->xi.geoip.network) ci->xi.geoip.network = newstring(ident); // gban used to identify network
            if(mode == -1) return false;
            if(mode != 1) continue;
            if(connect && banmsg)
            {
                string addr, msg;
                p->print(addr);
                z_formattemplate ft[] =
                {
                    { 'a', "%s", addr },
                    { 0, 0, 0 }
                };
                z_format(msg, sizeof(msg), banmsg, ft);
                if(*msg) ci->xi.setdiscreason(msg);
            }
            if(wlauth)
            {
                if(!connect)
                {
                    if(ci->xi.wlauth && !strcmp(ci->xi.wlauth, wlauth)) return false;   // already whitelisted
                }
                else ci->xi.setwlauth(wlauth);
            }
            return true;
        }
    }

    return false;
}

static void recheckgbans(clientinfo *actor = NULL)
{
    loopvrev(clients)
    {
        clientinfo *ci = clients[i];
        if(ci->state.aitype != AI_NONE || ci->local || ci->privilege >= PRIV_ADMIN) continue;
        if(actor && ((ci->privilege > actor->privilege && !actor->local) || ci->clientnum == actor->clientnum)) continue;
        if(checkgban(getclientip(ci->clientnum), ci)) disconnect_client(ci->clientnum, DISC_IPBAN);
    }
}

static void addgban(int m, const char *name)
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
            logoutf("WARNING: gban[%d]: %s conflicts with existing %s", m, name, buf);
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

    recheckgbans();
}

static void addpban(const char *name, const char *comment, time_t dateadded, time_t lasthit)
{
    pbaninfo &ban = sbans.add();
    ban.parse(name);
    if(comment && *comment) ban.comment = newstring(comment);
    ban.dateadded = dateadded;
    ban.lasthit = lasthit;

    checkexpiredpbans();

    recheckgbans();
}

void pban(char *name, char *dateadded, char *comment, char *lasthit)
{
    time_t ta, lh;
    char *end;

    end = NULL;
    ta = (time_t)strtoull(dateadded, &end, 10);
    if(!end || *end) ta = 0;

    end = NULL;
    lh = (time_t)strtoull(lasthit, &end, 10);
    if(!end || *end) lh = 0;

    if(!ta) addgban(-1, name);
    else addpban(name, comment, ta, lh);
}
COMMAND(pban, "ssss");

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
    if(!*pbans_file) return;
    loopv(sbans)
    {
        if(!f)
        {
            f = openutf8file(path(pbans_file, true), "w"); /* executed after server-init.cfg */
            if(!f) return;
            f->printf("// list of persistent bans. do not edit this file directly while server is running\n");
        }
        sbans[i].print(buf);
        f->printf("pban %s %llu %s %llu\n",
                  buf,
                  (unsigned long long)sbans[i].dateadded,
                  sbans[i].comment ? escapestring(sbans[i].comment) : "\"\"",
                  (unsigned long long)sbans[i].lasthit);
    }
    if(f) delete f;
    else remove(findfile(path(pbans_file, true), "r"));
}

void z_trigger_loadpbans(int type)
{
    atexit(z_savepbans);
    if(*pbans_file) execfile(pbans_file, false);
}
Z_TRIGGER(z_trigger_loadpbans, Z_TRIGGER_STARTUP);

void z_servcmd_listpbans(int argc, char **argv, int sender)
{
    string buf[3];
    sendf(sender, 1, "ris", N_SERVMSG, sbans.empty() ? "pbans list is empty" : "pbans list:");
    loopv(sbans)
    {
        sbans[i].print(buf[0]);
        z_formatdate(buf[1], sizeof(buf[1]), sbans[i].dateadded);
        if(sbans[i].lasthit) z_formatdate(buf[2], sizeof(buf[2]), sbans[i].lasthit);
        else strcpy(buf[2], "never");
        sendf(sender, 1, "ris", N_SERVMSG, sbans[i].comment
            ? tempformatstring("\f2id: \f7%2d\f2, ip: \f7%s\f2, added: \f7%s\f2, last hit: \f7%s\f2, reason: \f7%s", i, buf[0], buf[1], buf[2], sbans[i].comment)
            : tempformatstring("\f2id: \f7%2d\f2, ip: \f7%s\f2, added: \f7%s\f2, last hit: \f7%s",                   i, buf[0], buf[1], buf[2]));
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

VAR(pbans_minrange, 0, 0, 32);

void z_servcmd_pban(int argc, char **argv, int sender)
{
    if(argc < 2) { z_servcmd_pleasespecifyclient(sender); return; }
    char *end = NULL, *range = strchr(argv[1], '/');
    if(range) *range++ = 0;
    int cn = (int)strtol(argv[1], &end, 10);
    clientinfo *ci;
    if(!end || !(ci = (clientinfo *)getclientinfo(cn))) { z_servcmd_unknownclient(argv[1], sender); return; }
    if(ci->local) { sendf(sender, 1, "ris", N_SERVMSG, "you cannot ban local client"); return; }
    int r;
    if(range)
    {
        end = NULL;
        r = (int)clamp(strtol(range, &end, 10), 0, 32);
        if(end <= range || *end) r = 32;    /* failed to read int or string doesn't end after integer */
    }
    else r = 32;

    if(r < pbans_minrange)
    {
        sendf(sender, 1, "ris", N_SERVMSG, "pban range is too big");
        return;
    }

    pbaninfo &ban = sbans.add();
    ban.mask = ENET_HOST_TO_NET_32(0xFFffFFff << (32 - r));
    ban.ip = getclientip(cn) & ban.mask;
    if(argc > 2) ban.comment = newstring(argv[2]);
    time(&ban.dateadded);
    ban.lasthit = 0;

    string buf;
    ban.print(buf);
    sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("added pban for %s", buf));

    checkexpiredpbans();

    recheckgbans(getinfo(sender));
}
SCOMMANDA(pban, PRIV_ADMIN, z_servcmd_pban, 2);

void z_servcmd_pbanip(int argc, char **argv, int sender)
{
    if(argc <= 1) { sendf(sender, 1, "ris", N_SERVMSG, "please specify ip address"); return; }

    pbaninfo &ban = sbans.add();
    ban.parse(argv[1]);

    if(pbans_minrange && ((0xFFffFFff << (32 - pbans_minrange)) & ~ENET_NET_TO_HOST_32(ban.mask)))
    {
        sendf(sender, 1, "ris", N_SERVMSG, "pban range is too big");
        sbans.drop();
        return;
    }

    if(argc > 2) ban.comment = newstring(argv[2]);
    time(&ban.dateadded);
    ban.lasthit = 0;

    string buf;
    ban.print(buf);
    sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("added pban for %s", buf));

    checkexpiredpbans();

    recheckgbans(getinfo(sender));
}
SCOMMANDA(pbanip, PRIV_ADMIN, z_servcmd_pbanip, 2);

#endif // Z_GBANS_OVERRIDE_H
