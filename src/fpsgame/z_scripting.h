#ifdef Z_SCRIPTING_H
#error "already z_scripting.h"
#endif
#define Z_SCRIPTING_H

#ifndef Z_SERVCMD_H
#error "want z_servcmd.h"
#endif

enum
{
    ZS_SLEEPS = 0,
    ZS_ANNOUNCES,
    ZS_AUTOSPEC,
    ZS_NUM
};

vector<z_sleepstruct> z_sleeps[ZS_NUM];

static void z_addsleep(vector<z_sleepstruct> &sleeps, int offset, int delay, bool reload, z_sleepfunc func, void *val, z_freevalfunc freeval)
{
    z_sleepstruct &ss = sleeps.add();
    ss.millis = totalmillis + offset;
    ss.delay = delay;
    ss.func = func;
    ss.freeval = freeval;
    ss.val = val;
    ss.reload = reload;
}

static void z_clearsleep(vector<z_sleepstruct> &sleeps) { sleeps.shrink(0); }

static void z_checksleep(vector<z_sleepstruct> &sleeps)
{
    loopv(sleeps)
    {
        z_sleepstruct &ss = sleeps[i];
        if(totalmillis-ss.millis >= ss.delay)
        {
            // prepare
            z_sleepfunc sf = ss.func;
            ss.func = NULL;     // its sign that this entry is currently being executed
            z_freevalfunc fvf = ss.freeval;
            ss.freeval = NULL;  // disallow freeing value while executing
            void *val = ss.val;
            // execute
            sf(val);
            // check if not cleared
            if(sleeps.inrange(i) && ss.func == NULL)  // its still the same entry
            {
                if(ss.reload)
                {
                    ss.func = sf;
                    ss.freeval = fvf;
                    ss.millis += ss.delay;
                }
                else
                {
                    if(fvf) fvf(val);
                    sleeps.remove(i--);
                }
            }
            else    // sleeps array was modified in function
            {
                if(fvf) fvf(val);
                break;              // abandom, since array was modified
            }
        }
    }
}

void z_checksleep()
{
    loopi(sizeof(z_sleeps)/sizeof(z_sleeps[0])) z_checksleep(z_sleeps[i]);
}

static void z_sleepcmd_announce(void *str)
{
    if(str) sendservmsg((char *)str);
}

static void z_freestring(void *str) { delete[] (char *)str; }

void s_announce(int *offset, int *delay, char *message)
{
    z_addsleep(z_sleeps[ZS_ANNOUNCES], *offset, *delay, true, z_sleepcmd_announce, newstring(message), z_freestring);
}
COMMAND(s_announce, "iis");

void s_clearannounces()
{
    z_clearsleep(z_sleeps[ZS_ANNOUNCES]);
}
COMMAND(s_clearannounces, "");

static void z_sleepcmd_cubescript(void *cmd)
{
    if(cmd) execute((char *)cmd);
}

void s_sleep(int *offset, int *delay, char *script)
{
    z_addsleep(z_sleeps[ZS_SLEEPS], *offset, *delay, false, z_sleepcmd_cubescript, newstring(script), z_freestring);
}
COMMAND(s_sleep, "iis");

void s_sleep_r(int *offset, int *delay, char *script)
{
    z_addsleep(z_sleeps[ZS_SLEEPS], *offset, *delay, true, z_sleepcmd_cubescript, newstring(script), z_freestring);
}
COMMAND(s_sleep_r, "iis");

void s_clearsleep()
{
    z_clearsleep(z_sleeps[ZS_SLEEPS]);
}
COMMAND(s_clearsleep, "");

extern int autospecsecs;
static void z_autospec_process(void *)
{
    z_clearsleep(z_sleeps[ZS_AUTOSPEC]);
    if(!autospecsecs) return;
    int mindiff = 10*1000; // 10 secs by default
    loopv(clients)
    {
        if(!m_mp(gamemode) || m_edit || interm) break;
        clientinfo *ci = clients[i];
        if(ci->spy || ci->state.aitype != AI_NONE || ci->state.state != CS_DEAD) continue;
        int diff = totalmillis - ci->state.statemillis;
        if(diff >= autospecsecs*1000)
        {
            forcespectator(ci);
            continue;
        }
        if(diff < 0) continue; // wraparound
        diff = autospecsecs*1000 - diff;
        if(mindiff > diff) mindiff = diff;
    }
    z_addsleep(z_sleeps[ZS_AUTOSPEC], 0, mindiff, false, z_autospec_process, NULL, NULL);
}

// in seconds
VARF(autospecsecs, 0, 0, 604800, z_autospec_process(NULL));


void s_write(int *cn, char *msg)
{
    if(!getclientinfo(*cn)) return;
    sendf(*cn, 1, "ris", N_SERVMSG, msg);
}
COMMAND(s_write, "is");

ICOMMAND(s_wall, "C", (char *s), sendservmsg(s));

void s_kick(int *cn, char *reason)
{
    clientinfo *ci = getinfo(*cn);
    if(!ci || !ci->connected || ci->local || ci->privilege >= PRIV_ADMIN) return;
    z_showkick("", NULL, ci, reason);
    disconnect_client(*cn, DISC_KICK);
}
COMMAND(s_kick, "is");

void s_kickban(int *cn, char *t, char *reason)
{
    clientinfo *ci = getinfo(*cn);
    if(!ci || !ci->connected || ci->local || ci->privilege >= PRIV_ADMIN) return;
    uint ip = getclientip(*cn);
    if(!ip) return;

    int time = t[0] ? z_readbantime(t) : 4*60*60000;

    z_showkick("", NULL, ci, reason);
    if(time > 0) addban(ip, time, BAN_KICK, reason);
    kickclients(ip);
}
COMMAND(s_kickban, "iss");

void s_listclients(int *bots)
{
    string cn;
    vector<char> buf;
    loopv(clients) if(clients[i]->state.aitype==AI_NONE || (*bots > 0 && clients[i]->state.state!=CS_SPECTATOR))
    {
        if(buf.length()) buf.add(' ');
        formatstring(cn, "%d", clients[i]->clientnum);
        buf.put(cn, strlen(cn));
    }
    buf.add('\0');
    result(buf.getbuf());
}
COMMAND(s_listclients, "b");

ICOMMAND(s_getclientname, "i", (int *cn), { clientinfo *ci = getinfo(*cn); result(ci && ci->connected ? ci->name : ""); });
ICOMMAND(s_getclientprivilege, "i", (int *cn), { clientinfo *ci = getinfo(*cn); intret(ci && ci->connected ? ci->privilege : 0); });
ICOMMAND(s_getclientprivname, "i", (int *cn), { clientinfo *ci = getinfo(*cn); result(ci && ci->connected ? privname(ci->privilege) : ""); });
ICOMMAND(s_getclienthostname, "i", (int *cn), { const char *hn = getclienthostname(*cn); result(hn ? hn : ""); });

ICOMMAND(s_numclients, "bbbb", (int *exclude, int *nospec, int *noai, int *priv),
{
    intret(numclients(*exclude >= 0 ? *exclude : -1, *nospec!=0, *noai!=0, *priv>0));
});

ICOMMAND(s_addai, "ib", (int *skill, int *limit), intret(aiman::addai(*skill, *limit) ? 1 : 0));
ICOMMAND(s_delai, "", (), intret(aiman::deleteai() ? 1 : 0));


ICOMMAND(s_m_noitems,      "", (), intret(m_noitems      ? 1 : 0));
ICOMMAND(s_m_noammo,       "", (), intret(m_noammo       ? 1 : 0));
ICOMMAND(s_m_insta,        "", (), intret(m_insta        ? 1 : 0));
ICOMMAND(s_m_tactics,      "", (), intret(m_tactics      ? 1 : 0));
ICOMMAND(s_m_efficiency,   "", (), intret(m_efficiency   ? 1 : 0));
ICOMMAND(s_m_capture,      "", (), intret(m_capture      ? 1 : 0));
ICOMMAND(s_m_capture_only, "", (), intret(m_capture_only ? 1 : 0));
ICOMMAND(s_m_regencapture, "", (), intret(m_regencapture ? 1 : 0));
ICOMMAND(s_m_ctf,          "", (), intret(m_ctf          ? 1 : 0));
ICOMMAND(s_m_ctf_only,     "", (), intret(m_ctf_only     ? 1 : 0));
ICOMMAND(s_m_protect,      "", (), intret(m_protect      ? 1 : 0));
ICOMMAND(s_m_hold,         "", (), intret(m_hold         ? 1 : 0));
ICOMMAND(s_m_collect,      "", (), intret(m_collect      ? 1 : 0));
ICOMMAND(s_m_teammode,     "", (), intret(m_teammode     ? 1 : 0));
ICOMMAND(s_m_overtime,     "", (), intret(m_overtime     ? 1 : 0));
ICOMMAND(s_m_edit,         "", (), intret(m_edit         ? 1 : 0));


ICOMMAND(s_setteam, "isi", (int *cn, char *newteam, int *mode),
{
    string team;
    filtertext(team, newteam, false, false, MAXTEAMLEN);
    clientinfo *ci = getinfo(*cn);
    if(!m_teammode || !team[0] || !ci || !ci->connected || !strcmp(ci->team, team)) return;
    if((!smode || smode->canchangeteam(ci, ci->team, team)) && addteaminfo(team))
    {
        if(ci->state.state==CS_ALIVE) suicide(ci);
        copystring(ci->team, team, MAXTEAMLEN+1);
    }
    aiman::changeteam(ci);
    sendf(!ci->spy ? -1 : ci->ownernum, 1, "riisi", N_SETTEAM, ci->clientnum, ci->team, *mode);
});

ICOMMAND(s_getteam, "i", (int *cn),
{
    clientinfo *ci = getinfo(*cn);
    result(m_teammode && ci ? ci->team : "");
});

ICOMMAND(s_listteam, "sbb", (char *team, int *spec, int *bots),
{
    string cn;
    vector<char> buf;

    if(m_teammode && team[0]) loopv(clients) if(!strcmp(clients[i]->team, team))
    {
        if(clients[i]->state.state==CS_SPECTATOR && *spec <= 0)
            continue;
        if(clients[i]->state.aitype!=AI_NONE &&
            (*bots <= 0 || clients[i]->state.state==CS_SPECTATOR))
        {
            continue;
        }

        if(buf.length()) buf.add(' ');
        formatstring(cn, "%d", clients[i]->clientnum);
        buf.put(cn, strlen(cn));
    }
    buf.add('\0');
    result(buf.getbuf());
});

ICOMMAND(s_listteams, "bb", (int *spec, int *bots),
{
    vector<const char *> teams;
    vector<char> buf;

    if(m_teammode) loopv(clients)
    {
        const char *team = clients[i]->team;
        if(!team[0]) continue;

        if(clients[i]->state.state==CS_SPECTATOR && *spec <= 0)
            continue;
        if(clients[i]->state.aitype!=AI_NONE &&
            (*bots <= 0 || clients[i]->state.state==CS_SPECTATOR))
        {
            continue;
        }

        loopvj(teams) if(!strcmp(teams[j], team)) goto nextteam;
        teams.add(team);
        if(buf.length()) buf.add(' ');
        buf.put(team, strlen(team));
    nextteam:
        ;
    }
    buf.add('\0');
    result(buf.getbuf());
});

ICOMMAND(s_countteam, "sbb", (char *team, int *spec, int *bots),
{
    int count = 0;

    if(m_teammode && team[0]) loopv(clients) if(!strcmp(clients[i]->team, team))
    {
        if(clients[i]->state.state==CS_SPECTATOR && *spec <= 0)
            continue;
        if(clients[i]->state.aitype!=AI_NONE &&
            (*bots <= 0 || clients[i]->state.state==CS_SPECTATOR))
        {
            continue;
        }

        ++count;
    }
    intret(count);
});
