#ifndef Z_AWARDS_H
#define Z_AWARDS_H

VAR(awards, 0, 0, 1);

VAR(awards_splitmessage, 0, 1, 1);

SVAR(awards_style_clientsseparator, ", ");
SVAR(awards_style_clientprefix, "\fs\f7");
SVAR(awards_style_clientsuffix, "\fr");
SVAR(awards_style_normal, "\f3Awards: \f6Kills: %F \f2(\f6%f\f2) \f6KpD: %P \f2(\f6%p\f2) \f6Acc: %A \f2(\f6%a%%\f2) \f6Damage: %D \f2(\f6%d\f2)");


template<typename T> static bool z_awards_best_stat(vector<clientinfo *> &best, T &bests, T (* func)(clientinfo *))
{
    best.setsize(0);
    loopv(clients) if(!clients[i]->spy)
    {
        if(best.empty())
        {
            best.add(clients[i]);
            bests = func(clients[i]);
        }
        else
        {
            T s = func(clients[i]);
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

static int z_client_getfrags        (clientinfo *ci) { return ci->state.frags; }
static int z_client_getkpd          (clientinfo *ci) { return ci->state.frags*1000/max(ci->state.deaths, 1); }
static int z_client_getacc          (clientinfo *ci) { return ci->state.damage*100/max(ci->state.shotdamage,1); }
static int z_client_getdamage       (clientinfo *ci) { return ci->state.damage; }
static int z_client_getteamkills    (clientinfo *ci) { return ci->state.teamkills; }
static int z_client_getdeaths       (clientinfo *ci) { return ci->state.deaths; }
static int z_client_getsuicides     (clientinfo *ci) { return ci->state.suicides; }
static int z_client_getdamagewasted (clientinfo *ci) { return ci->state.shotdamage - ci->state.damage; }

void z_awards()
{
    const int maxnum = 3;
    vector<char> str;
    vector<clientinfo *> best;

    if(!awards || m_edit) return;

    int clientsnum = 0;
    loopv(clients) if(!clients[i]->spy) ++clientsnum;
    if(!clientsnum) return;

    vector<char> F; int f = 0;
    z_awards_best_stat<int>(best, f, z_client_getfrags);
    z_awards_print_best(F, best, maxnum);

    vector<char> P; int p = 0; string ps;
    z_awards_best_stat<int>(best, p, z_client_getkpd);
    z_awards_print_best(P, best, maxnum);
    sprintf(ps, "%d.%03d", p/1000, p%1000);

    vector<char> A; int a = 0;
    z_awards_best_stat<int>(best, a, z_client_getacc);
    z_awards_print_best(A, best, maxnum);

    vector<char> D; int d = 0;
    z_awards_best_stat<int>(best, d, z_client_getdamage);
    z_awards_print_best(D, best, maxnum);

    vector<char> T; int t = 0;
    z_awards_best_stat<int>(best, t, z_client_getteamkills);
    z_awards_print_best(T, best, maxnum);

    vector<char> L; int l = 0;
    z_awards_best_stat<int>(best, l, z_client_getdeaths);
    z_awards_print_best(L, best, maxnum);

    vector<char> S; int s = 0;
    z_awards_best_stat<int>(best, s, z_client_getsuicides);
    z_awards_print_best(S, best, maxnum);

    vector<char> W; int w = 0;
    z_awards_best_stat<int>(best, w, z_client_getdamagewasted);
    z_awards_print_best(W, best, maxnum);


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
        { 0,   NULL, NULL }
    };

    char buf[MAXTRANS];
    z_format(buf, sizeof buf, awards_style_normal, ft);
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

#endif // Z_AWARDS_H
