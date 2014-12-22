#ifndef Z_AWARDS_H
#define Z_AWARDS_H

VAR(awards, 0, 0, 1);

/*
SVAR(awards_color_scheme, "");

static void z_awards_set_colors(char *colors, size_t len)
{
    size_t i = 0;
    for(; awards_color_scheme[i] && i < len; i++)
        colors[i] = awards_color_scheme[i] >= '0' && awards_color_scheme[i] <= '9' ? awards_color_scheme[i] : '7';
    for(; i < len; i++) colors[i] = i>0 ? colors[i-1] : '7';
}
template<size_t N> static inline void z_awards_set_colors(char (&s)[N]) { z_awards_set_colors(s, N); }
*/

template<typename T>
static bool z_awards_best_stat(vector<clientinfo *> &best, T &bests, T (* func)(clientinfo *))
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
        if(n) str.put(", ", 2);
        str.put("\fs\f7", 4);
        const char *name = colorname(best[i]);
        str.put(name, strlen(name));
        str.put("\fr", 2);
        ++n;
        if(max && n >= max) break;
    }
    return n;
}

void z_awards()
{
    const int maxnum = 3;
    vector<char> str;
    vector<clientinfo *> best;
    const char *tp;

    if(!awards || m_edit) return;

    tp = "\f3Awards:\f6";
    str.put(tp, strlen(tp));

    int bestk;
    if(z_awards_best_stat<int>(best, bestk, [](clientinfo *ci) { return ci->state.frags; }))
    {
        tp = " Kills: ";
        str.put(tp, strlen(tp));
        z_awards_print_best(str, best, maxnum);
        tp = tempformatstring(" \fs\f2(\f6%d\f2)\fr", bestk);
        str.put(tp, strlen(tp));
    }

    double bestp;
    if(z_awards_best_stat<double>(best, bestp, [](clientinfo *ci) { return double(ci->state.frags)/max(ci->state.deaths, 1); }))
    {
        tp = " KpD: ";
        str.put(tp, strlen(tp));
        z_awards_print_best(str, best, maxnum);
        tp = tempformatstring(" \fs\f2(\f6%.3f\f2)\fr", bestp);
        str.put(tp, strlen(tp));
    }

    int besta;
    if(z_awards_best_stat<int>(best, besta, [](clientinfo *ci) { return ci->state.damage*100/max(ci->state.shotdamage,1); }))
    {
        tp = " Acc: ";
        str.put(tp, strlen(tp));
        z_awards_print_best(str, best, maxnum);
        tp = tempformatstring(" \fs\f2(\f6%d%%\f2)\fr", besta);
        str.put(tp, strlen(tp));
    }

    int bestd;
    if(z_awards_best_stat<int>(best, bestd, [](clientinfo *ci) { return ci->state.damage; }))
    {
        tp = " Damage: ";
        str.put(tp, strlen(tp));
        z_awards_print_best(str, best, maxnum);
        tp = tempformatstring(" \fs\f2(\f6%d\f2)\fr", bestd);
        str.put(tp, strlen(tp));
    }

    str.add('\0');

    sendservmsg(str.getbuf());
}

#endif // Z_AWARDS_H