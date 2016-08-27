#ifdef Z_AWARDS_H
#error "already z_awards.h"
#endif
#define Z_AWARDS_H


VAR(awards, 0, 0, 1);

VAR(awards_splitmessage, 0, 1, 1);

SVAR(awards_style_clientsseparator, ", ");
SVAR(awards_style_clientprefix, "\fs\f7");
SVAR(awards_style_clientsuffix, "\fr");

/* some hardcoded defaults. it would be lot easier to put these in server-init.cfg, with concatword */
static const char awards_style_normal[] = "\f3Awards: "
                                          "\f6Kills: %F \f2(\f6%f\f2) "
                                          "\f6KpD: %P \f2(\f6%p\f2) "
                                          "\f6Acc: %A \f2(\f6%a%%\f2) "
                                          "\f6Damage: %D \f2(\f6%d\f2)";

static const char awards_style_protect[] =  "\f3Awards: "
                                            "\f6Kills: %F \f2(\f6%f\f2) "
                                            "\f6KpD: %P \f2(\f6%p\f2) "
                                            "\f6Acc: %A \f2(\f6%a%%\f2) "
                                            "\f6Damage: %D \f2(\f6%d\f2)\n"
                                            "\f3Flags: "
                                            "\f6Scored: %G \f2(\f6%g\f2) "
                                            "\f6Flag guardian: %E \f2(\f6%e sec\f2)";

static const char awards_style_hold[] = "\f3Awards: "
                                        "\f6Kills: %F \f2(\f6%f\f2) "
                                        "\f6KpD: %P \f2(\f6%p\f2) "
                                        "\f6Acc: %A \f2(\f6%a%%\f2) "
                                        "\f6Damage: %D \f2(\f6%d\f2)\n"
                                        "\f3Flags: "
                                        "\f6Scored: %G \f2(\f6%g\f2) "
                                        "\f6Taken: %O \f2(\f6%o\f2)";

static const char awards_style_ctf[] =  "\f3Awards: "
                                        "\f6Kills: %F \f2(\f6%f\f2) "
                                        "\f6KpD: %P \f2(\f6%p\f2) "
                                        "\f6Acc: %A \f2(\f6%a%%\f2) "
                                        "\f6Damage: %D \f2(\f6%d\f2)\n"
                                        "\f3Flags: "
                                        "\f6Scored: %G \f2(\f6%g\f2) "
                                        "\f6Stolen: %O \f2(\f6%o\f2) "
                                        "\f6Returned: %E \f2(\f6%e\f2)";

static const char awards_style_collect[] =  "\f3Awards: "
                                            "\f6Kills: %F \f2(\f6%f\f2) "
                                            "\f6KpD: %P \f2(\f6%p\f2) "
                                            "\f6Acc: %A \f2(\f6%a%%\f2) "
                                            "\f6Damage: %D \f2(\f6%d\f2)\n"
                                            "\f3Skulls: "
                                            "\f6Scored: %G \f2(\f6%g\f2) "
                                            "\f6Stolen: %O \f2(\f6%o\f2) "
                                            "\f6Returned: %E \f2(\f6%e\f2)";

static const char awards_style_capture[] =  "\f3Awards: "
                                            "\f6Kills: %F \f2(\f6%f\f2) "
                                            "\f6KpD: %P \f2(\f6%p\f2) "
                                            "\f6Acc: %A \f2(\f6%a%%\f2) "
                                            "\f6Damage: %D \f2(\f6%d\f2)\n"
                                            "\f3Bases: "
                                            "\f6Capture leader: %C \f2(\f6%c sec\f2) "
                                            "\f6Defense dude: %G \f2(\f6%g sec\f2)";

struct z_statsstyle
{
    int modes;
    static int exclude;
    char *stats;

    z_statsstyle(): stats(NULL) {}
    ~z_statsstyle() { delete[] stats; }

    int calcmask() const { return modes & (1<<NUMGAMEMODES) ? modes & ~exclude : modes; }
    bool hasmode(int mode, int offset = STARTGAMEMODE) const { return (calcmask() & (1 << (mode - offset))) != 0; }
    bool issubset(z_statsstyle &as) const { return (modes & as.modes) == as.modes; }
};
int z_statsstyle::exclude = 0;
vector<z_statsstyle> z_statsstyles;


struct z_awardsstyle
{
    int modes;
    static int exclude;
    char *awards;

    z_awardsstyle(): awards(NULL) {}
    ~z_awardsstyle() { delete[] awards; }

    int calcmask() const { return modes & (1<<NUMGAMEMODES) ? modes & ~exclude : modes; }
    bool hasmode(int mode, int offset = STARTGAMEMODE) const { return (calcmask() & (1 << (mode - offset))) != 0; }
    bool issubset(z_awardsstyle &as) const { return (modes & as.modes) == as.modes; }
};
int z_awardsstyle::exclude = 0;
vector<z_awardsstyle> z_awardsstyles;


static void z_stats_addstatstyle(int modemask, const char *sstr)
{
    if(!modemask) return;
    if(!(modemask&(1<<NUMGAMEMODES))) z_statsstyle::exclude |= modemask;
    z_statsstyle &s = z_statsstyles.add();
    s.modes = modemask;
    if(sstr && sstr[0]) s.stats = newstring(sstr);
}

static void z_stats_addawardstyle(int modemask, const char *astr)
{
    if(!modemask) return;
    if(!(modemask&(1<<NUMGAMEMODES))) z_awardsstyle::exclude |= modemask;
    z_awardsstyle &s = z_awardsstyles.add();
    s.modes = modemask;
    if(astr && astr[0]) s.awards = newstring(astr);
}

void stats_style(tagval *args, int numargs)
{
    vector<char *> modes;
    for(int i = 0; i < numargs; i += 2)
    {
        if(i+1 < numargs) explodelist(args[i+1].getstr(), modes);
        if(modes.empty()) modes.add(newstring("*"));
        int modemask = genmodemask(modes);
        z_stats_addstatstyle(modemask, args[i].getstr());
        modes.deletearrays();
    }
}
COMMAND(stats_style, "ss2V");

void stats_stylereset()
{
    z_statsstyle::exclude = 0;
    z_statsstyles.shrink(0);
}
COMMAND(stats_stylereset, "");

void awards_style(tagval *args, int numargs)
{
    vector<char *> modes;
    for(int i = 0; i < numargs; i += 2)
    {
        if(i+1 < numargs) explodelist(args[i+1].getstr(), modes);
        if(modes.empty()) modes.add(newstring("*"));
        int modemask = genmodemask(modes);
        z_stats_addawardstyle(modemask, args[i].getstr());
        modes.deletearrays();
    }
}
COMMAND(awards_style, "ss2V");

void awards_stylereset()
{
    z_awardsstyle::exclude = 0;
    z_awardsstyles.shrink(0);
}
COMMAND(awards_stylereset, "");

static int z_pickbeststatsstyle()
{
    int best = -1;
    loopv(z_statsstyles)
    {
        z_statsstyle &s = z_statsstyles[i];
        if(s.hasmode(gamemode) && (best < 0 || z_statsstyles[best].issubset(s))) best = i;
    }
    return best;
}

static int z_pickbestawardsstyle()
{
    int best = -1;
    loopv(z_awardsstyles)
    {
        z_awardsstyle &s = z_awardsstyles[i];
        if(s.hasmode(gamemode) && (best < 0 || z_awardsstyles[best].issubset(s))) best = i;
    }
    return best;
}

VAR(awards_fallback, 0, 1, 1);

static inline const char *z_pickawardstemplate()
{
    if(!z_awardsstyles.empty())
    {
        int best = z_pickbestawardsstyle();
        if(best >= 0) return z_awardsstyles[best].awards;
        if(!awards_fallback) return NULL;
    }
    // some default hardcoded templates
    if(m_ctf)
    {
        if(m_protect) return awards_style_protect;
        else if(m_hold) return awards_style_hold;
        else return awards_style_ctf;
    }
    else if(m_collect) return awards_style_collect;
    else if(m_capture) return awards_style_capture;
    else if(m_edit) return NULL;
    else return awards_style_normal;
}


template<typename T> static bool z_awards_best_stat(vector<clientinfo *> &best, T &bests, T (* func)(clientinfo &))
{
    best.setsize(0);
    loopv(clients) if(!clients[i]->spy)
    {
        if(best.empty())
        {
            best.add(clients[i]);
            bests = func(*clients[i]);
        }
        else
        {
            T s = func(*clients[i]);
            if(s == bests) best.add(clients[i]);
            else if(s > bests)
            {
                best.setsize(0);
                best.add(clients[i]);
                bests = s;
            }
        }
    }
    return best.length();
}

static int z_awards_print_best(vector<char> &str, vector<clientinfo *> &best, int max = 0)
{
    int n = 0;
    loopv(best)
    {
        if(n) str.put(awards_style_clientsseparator, strlen(awards_style_clientsseparator));
        str.put(awards_style_clientprefix, strlen(awards_style_clientprefix));
        const char *name = colorname(best[i]);
        str.put(name, strlen(name));
        str.put(awards_style_clientsuffix, strlen(awards_style_clientsuffix));
        ++n;
        if(max && n >= max) break;
    }
    str.add(0);
    return n;
}

static int z_client_getfrags        (clientinfo &ci) { return ci.state.frags; }
static int z_client_getkpd          (clientinfo &ci) { return ci.state.frags*1000/max(ci.state.deaths, 1); }
static int z_client_getacc          (clientinfo &ci) { return ci.state.damage*100/max(ci.state.shotdamage,1); }
static int z_client_getdamage       (clientinfo &ci) { return ci.state.damage; }
static int z_client_getteamkills    (clientinfo &ci) { return ci.state.teamkills; }
static int z_client_getdeaths       (clientinfo &ci) { return ci.state.deaths; }
static int z_client_getsuicides     (clientinfo &ci) { return ci.state.suicides; }
static int z_client_getdamagewasted (clientinfo &ci) { return ci.state.shotdamage - ci.state.damage; }
static int z_client_getmaxstreak    (clientinfo &ci) { return ci.state.maxstreak; }
static int z_client_getflags        (clientinfo &ci) { return ci.state.flags; }
static int z_client_getstolen       (clientinfo &ci) { return ci.state.stolen; }
static int z_client_getreturned     (clientinfo &ci) { return ci.state.returned; }
static int z_client_getcaptured     (clientinfo &ci) { return ci.state.stolen + ci.state.returned; }

void z_awards()
{
    const int maxnum = 3;
    vector<char> str;
    vector<clientinfo *> best;

    if(!awards) return;
    const char *style = z_pickawardstemplate();
    if(!style || !style[0]) return;

    int clientsnum = 0;
    loopv(clients) if(!clients[i]->spy) ++clientsnum;
    if(!clientsnum) return;

    // frags
    vector<char> F; int f = 0;
    z_awards_best_stat<int>(best, f, z_client_getfrags);
    z_awards_print_best(F, best, maxnum);

    // KpD
    vector<char> P; int p = 0; string ps;
    z_awards_best_stat<int>(best, p, z_client_getkpd);
    z_awards_print_best(P, best, maxnum);
    sprintf(ps, "%d.%03d", p/1000, p%1000);

    // acc
    vector<char> A; int a = 0;
    z_awards_best_stat<int>(best, a, z_client_getacc);
    z_awards_print_best(A, best, maxnum);

    // damage done
    vector<char> D; int d = 0;
    z_awards_best_stat<int>(best, d, z_client_getdamage);
    z_awards_print_best(D, best, maxnum);

    // tk
    vector<char> T; int t = 0;
    z_awards_best_stat<int>(best, t, z_client_getteamkills);
    z_awards_print_best(T, best, maxnum);

    // deaths
    vector<char> L; int l = 0;
    z_awards_best_stat<int>(best, l, z_client_getdeaths);
    z_awards_print_best(L, best, maxnum);

    // suicides
    vector<char> S; int s = 0;
    z_awards_best_stat<int>(best, s, z_client_getsuicides);
    z_awards_print_best(S, best, maxnum);

    // damage wasted
    vector<char> W; int w = 0;
    z_awards_best_stat<int>(best, w, z_client_getdamagewasted);
    z_awards_print_best(W, best, maxnum);

    // maxstreak
    vector<char> R; int r = 0;
    z_awards_best_stat<int>(best, r, z_client_getmaxstreak);
    z_awards_print_best(R, best, maxnum);

    // ctf/hold/protect: flags scored
    // collect: skulls scored
    // capture: total time in own team bases
    vector<char> G; int g = 0;
    z_awards_best_stat<int>(best, g, z_client_getflags);
    z_awards_print_best(G, best, maxnum);

    // ctf: enemy team flags stolen
    // hold: flags taken
    // collect: skulls stolen from enemy base
    // capture: total time in enemy team bases
    vector<char> O; int o = 0;
    z_awards_best_stat<int>(best, o, z_client_getstolen);
    z_awards_print_best(O, best, maxnum);

    // ctf: own team flags returned
    // protect: total time held flag
    // collect: own team skulls returned
    // capture: total time in neutral bases
    vector<char> E; int e = 0;
    z_awards_best_stat<int>(best, e, z_client_getreturned);
    z_awards_print_best(E, best, maxnum);

    // capture: total time in non-own team bases (enemy + neutral)
    vector<char> C; int c = 0;
    z_awards_best_stat<int>(best, c, z_client_getcaptured);
    z_awards_print_best(C, best, maxnum);

    z_formattemplate ft[] =
    {
        { 'F', "%s", F.getbuf() },
        { 'f', "%d", (const void *)(long)f },
        { 'P', "%s", P.getbuf() },
        { 'p', "%s", ps },
        { 'A', "%s", A.getbuf() },
        { 'a', "%d", (const void *)(long)a },
        { 'D', "%s", D.getbuf() },
        { 'd', "%d", (const void *)(long)d },
        { 'T', "%s", T.getbuf() },
        { 't', "%d", (const void *)(long)t },
        { 'L', "%s", L.getbuf() },
        { 'l', "%d", (const void *)(long)l },
        { 'S', "%s", S.getbuf() },
        { 's', "%d", (const void *)(long)s },
        { 'W', "%s", W.getbuf() },
        { 'w', "%d", (const void *)(long)w },
        { 'R', "%s", R.getbuf() },
        { 'r', "%d", (const void *)(long)r },
        { 'G', "%s", G.getbuf() },
        { 'g', "%d", (const void *)(long)g },
        { 'O', "%s", O.getbuf() },
        { 'o', "%d", (const void *)(long)o },
        { 'E', "%s", E.getbuf() },
        { 'e', "%d", (const void *)(long)e },
        { 'C', "%s", C.getbuf() },
        { 'c', "%d", (const void *)(long)c },
        { 0,   NULL, NULL }
    };

    char buf[MAXTRANS];
    z_format(buf, sizeof(buf), style, ft);
    if(awards_splitmessage)
    {
        char *ptr, *next = buf;
        while(next)
        {
            ptr = next;
            next = strchr(ptr, '\n');
            if(next) *next++ = '\0';
            sendservmsg(ptr);
        }
    }
    else sendservmsg(buf);
}

SVAR(stats_style_normal, "\f6stats: \f7%C: \f2frags: \f7%f\f2, deaths: \f7%l\f2, suicides: \f7%s\f2, accuracy(%%): \f7%a\f2, kpd: \f7%p");
SVAR(stats_style_teamplay, "\f6stats: \f7%C: \f2frags: \f7%f\f2, deaths: \f7%l\f2, suicides: \f7%s\f2, teamkills: \f7%t\f2, accuracy(%%): \f7%a\f2, kpd: \f7%p");
SVAR(stats_style_ctf, "\f6stats: \f7%C: \f2frags: \f7%f\f2, flags: \f7%g\f2, deaths: \f7%l\f2, suicides: \f7%s\f2, teamkills: \f7%t\f2, accuracy(%%): \f7%a\f2, kpd: \f7%p");

VAR(stats_fallback, 0, 1, 1);

static inline const char *z_pickstatstemplate()
{
    if(!z_statsstyles.empty())
    {
        int best = z_pickbeststatsstyle();
        if(best >= 0) return z_statsstyles[best].stats;
        if(!stats_fallback) return NULL;
    }
    // fallback to defaults
    if(m_ctf) return stats_style_ctf;
    else if(m_teammode) return stats_style_teamplay;
    else return stats_style_normal;
}

static inline void z_putstats(char (&msg)[MAXSTRLEN], clientinfo *ci)
{
    const char *statstemplate = z_pickstatstemplate();
    if(!statstemplate || !statstemplate[0])
    {
        msg[0] = '\0';
        return;
    }

    gamestate &gs = ci->state;

    int p = gs.frags*1000/max(gs.deaths, 1);
    string ps;
    formatstring(ps, "%d.%03d", p/1000, p%1000);

    z_formattemplate ft[] =
    {
        { 'C', "%s", colorname(ci) },
        { 'f', "%d", (const void *)(long)gs.frags },                            // frags
        { 'p', "%s", ps },                                                      // kpd
        { 'a', "%d", (const void *)(long)(gs.damage*100/max(gs.shotdamage,1)) },// acc
        { 'd', "%d", (const void *)(long)gs.damage },                           // damage done
        { 't', "%d", (const void *)(long)gs.teamkills },                        // teamkills
        { 'l', "%d", (const void *)(long)gs.deaths },                           // deaths
        { 's', "%d", (const void *)(long)gs.suicides },                         // suicides
        { 'w', "%d", (const void *)(long)(gs.shotdamage-gs.damage) },           // damage wasted
        { 'r', "%d", (const void *)(long)gs.maxstreak },                        // max streak
        { 'g', "%d", (const void *)(long)gs.flags },                            // scored/time in own bases
        { 'o', "%d", (const void *)(long)gs.stolen },                           // stolen/taken/time in enemy bases
        { 'e', "%d", (const void *)(long)gs.returned },                         // returned/time in neutral bases/time protected flag
        { 'c', "%d", (const void *)(long)(gs.stolen+gs.returned) },             // time in non own bases
        { 0, 0, 0 }
    };

    z_format(msg, sizeof(msg), statstemplate, ft);
}

void z_servcmd_stats(int argc, char **argv, int sender)
{
    int cn, i;
    clientinfo *ci = NULL, *senderci = getinfo(sender);
    vector<clientinfo *> cis;
    for(i = 1; i < argc; i++)
    {
        if(!z_parseclient_verify(argv[i], cn, true, true, senderci->local || senderci->privilege>=PRIV_ADMIN))
        {
            z_servcmd_unknownclient(argv[i], sender);
            return;
        }
        if(cn < 0)
        {
            cis.shrink(0);
            loopvj(clients) if(!clients[j]->spy || !senderci || senderci->local || senderci->privilege>=PRIV_ADMIN) cis.add(clients[j]);
            break;
        }
        ci = getinfo(cn);
        if(cis.find(ci)<0) cis.add(ci);
    }

    if(cis.empty() && senderci) cis.add(senderci);

    string buf;
    for(i = 0; i < cis.length(); i++)
    {
        z_putstats(buf, cis[i]);
        if(buf[0]) sendf(sender, 1, "ris", N_SERVMSG, buf);
    }
}
SCOMMAND(stats, PRIV_NONE, z_servcmd_stats);
