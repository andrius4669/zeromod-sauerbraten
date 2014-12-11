#ifndef Z_RACEMODE_H
#define Z_RACEMODE_H

#include "z_sendmap.h"
#include "z_autosendmap.h"
#include "z_loadmap.h"

bool z_racemode = false;
VARFN(racemode, defaultracemode, 0, 0, 1, { if(clients.empty()) z_racemode = defaultracemode!=0; });
static void z_racemode_trigger(int type) { z_racemode = defaultracemode!=0; }
Z_TRIGGER(z_racemode_trigger, Z_TRIGGER_STARTUP);
Z_TRIGGER(z_racemode_trigger, Z_TRIGGER_NOCLIENTS);

void z_servcmd_racemode(int argc, char **argv, int sender)
{
    if(argc < 2) { sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("racemode is %s", z_racemode ? "enabled" : "disabled")); return; }
    z_racemode = atoi(argv[1]) > 0;
    sendservmsgf("racemode %s", z_racemode ? "enabled" : "disabled");
}
SCOMMANDA(racemode, PRIV_ADMIN, z_servcmd_racemode, 1);

VAR(racemode_waitmap, 0, 10000, INT_MAX);
VAR(racemode_startmillis, 0, 10000, INT_MAX);
VAR(racemode_gamelimit, 0, 0, INT_MAX);
VAR(racemode_winnerwait, 0, 30000, INT_MAX);
VAR(racemode_finishmillis, 0, 5000, INT_MAX);

#define RACE_DEFAULT_RADIUS     24
#define RACE_MAXPLACES          20

struct z_raceendinfo
{
    vec o;
    int radius;
    int place;  /* starting from 0 */
};
struct z_mapraceend
{
    string map;
    z_raceendinfo rend;
};
vector<z_mapraceend> z_rends;

ICOMMAND(clearrends, "", (), z_rends.shrink(0));
ICOMMAND(rend, "sfffii", (char *map, float *x, float *y, float *z, int *r, int *p),
{
    if(!*map) return;
    z_mapraceend &e = z_rends.add();
    copystring(e.map, map);
    e.rend.o.x = *x;
    e.rend.o.y = *y;
    e.rend.o.z = *z;
    e.rend.radius = *r > 0 ? *r : RACE_DEFAULT_RADIUS;
    e.rend.place = clamp(*p, 1, RACE_MAXPLACES) - 1;    /* in command specified place starts from 1, in struct it starts from 0 */
});

struct raceservmode: servmode
{
    static const int RACE_MAXENDS = 64;
    enum { ST_NONE = 0, ST_WAITMAP, ST_READY, ST_STARTED, ST_FINISHED, ST_INT };

    int state;
    int statemillis;
    int countermillis;
    int minraceend;     // used to prevent nasty bug when only non-first places are specified
    struct winner
    {
        int cn, racemillis;
        winner(): cn(-1), racemillis(0) {}
    };
    vector<winner> race_winners;
    vector<z_raceendinfo> raceends;

    raceservmode() { reset(); }

    void cleanup()
    {
        reset();
        loopv(clients) clients[i]->state.flags = 0;
    }

    void reset()
    {
        state = ST_NONE;
        statemillis = gamemillis;
        countermillis = gamemillis;
        minraceend = INT_MAX;
        race_winners.shrink(0);
        raceends.shrink(0);
    }

    void setup()
    {
        reset();
        bool foundstatic = false;
        loopv(z_rends) if(!strcmp(smapname, z_rends[i].map))
        {
            foundstatic = true;
            raceends.add(z_rends[i].rend);
            int place = z_rends[i].rend.place;
            while(race_winners.length() <= place) race_winners.add();
            if(place < minraceend) minraceend = place;
        }
        // we don't need to read ents from map ents if admin supply them in config
        if(foundstatic) return;
        if(notgotitems || ments.empty()) return;
        loopv(ments)
        {
            entity &e = ments[i];
            if((e.type != PLAYERSTART && e.type != FLAG) || e.attr2 >= 0) continue;
            z_raceendinfo &r = raceends.add();
            r.o = e.o;
            r.radius = e.attr3 > 0 ? e.attr3 : RACE_DEFAULT_RADIUS;
            r.place = min(-e.attr2, RACE_MAXPLACES) - 1;
            //logoutf("dbg: added mrend (%f; %f; %f) r(%d) p(%d)", r.o.x, r.o.y, r.o.z, r.radius, place);
            while(race_winners.length() <= r.place) race_winners.add();
            //logoutf("dbg: race_winners.length() = %d", race_winners.length());
            if(r.place < minraceend) minraceend = r.place;
            if(raceends.length() >= RACE_MAXENDS) break;
        }
    }

    static inline bool reached_raceend(const z_raceendinfo &r, const vec &o)
    {
        float dx = (r.o.x-o.x), dy = (r.o.y-o.y), dz = (r.o.z-o.z);
        return dx*dx + dy*dy <= r.radius*r.radius && fabs(dz) <= r.radius;
    }

    const char *placename(int n)
    {
        switch(++n)
        {
            case 1: return "FIRST";
            case 2: return "SECOND";
            case 3: return "THRID";
            case 4: return "FOURTH";
            case 5: return "FIFTH";
            default: return tempformatstring("%dth", n);
        }
    }

    void checkplaces()
    {
        // start intermission 5 seconds after last place has been taken (or its too less clients to take any more places)
        int numfinished = 0, numavaiable = 0;
        loopv(race_winners)
        {
            if(race_winners[i].cn >= 0) numfinished++;
            else numavaiable++;
        }
        int numplayers = numclients(-1, true, false), unfinished = numplayers-numfinished;
        if(numavaiable <= 0 || unfinished <= 0)
        {
            state = ST_FINISHED;
            statemillis = countermillis = totalmillis;
        }
        else if(racemode_winnerwait && numfinished > 0 && (!statemillis || statemillis-totalmillis > racemode_winnerwait))
        {
            statemillis = totalmillis + racemode_winnerwait;
            if(!statemillis) statemillis = 1;
            countermillis = totalmillis;
        }
    }

    void moved(clientinfo *ci, const vec &oldpos, bool oldclip, const vec &newpos, bool newclip)
    {
        if(state != ST_STARTED && state != ST_FINISHED) return;
        if(ci->state.flags) return;     /* flags are reused for race cheating info */
        int avaiable_place = -1;
        loopv(race_winners)
        {
            int cn = race_winners[i].cn;
            if(cn == ci->clientnum) break;
            if(cn < 0)
            {
                avaiable_place = i;
                break;
            }
        }
        if(avaiable_place >= 0) loopv(raceends)
        {
            z_raceendinfo &r = raceends[i];
            if(r.place > avaiable_place && r.place > minraceend) continue;  /* don't skip is place is already minimal */
            if(!reached_raceend(r, newpos)) continue;
            int racemillis = 0;
            for(int j = race_winners.length()-1; j > avaiable_place; j--) if(race_winners[j].cn == ci->clientnum)
            {
                race_winners[j].cn = -1;
                racemillis = race_winners[j].racemillis;
            }
            if(!racemillis) racemillis = gamemillis-ci->state.lastdeath;    /* lastdeath is reused for spawntime */
            race_winners[avaiable_place].cn = ci->clientnum;
            race_winners[avaiable_place].racemillis = racemillis;
            sendservmsgf("\f6race: \f7%s \f2won \f6%s PLACE!! \f7(%d.%03ds)", colorname(ci), placename(avaiable_place), racemillis/1000, racemillis%1000);
            break;
        }
        if(state == ST_STARTED) checkplaces();
    }

    static void warnracecheat(clientinfo *ci)
    {
        if(ci->state.flags != 1 && ci->state.flags != 2) return;
        sendf(ci->clientnum, 1, "ris", N_SERVMSG, ci->state.flags == 1
            ? "edit mode is not allowed in racemode. please suicide to continue racing"
            : "editing map is not allowed in racemode. please getmap or reconnect to continue racing");
    }

    void racecheat(clientinfo *ci, int type)
    {
        if((state < ST_STARTED && type < 2) || state > ST_FINISHED) return;
        if(ci->state.flags < type)
        {
            ci->state.flags = type;
            warnracecheat(ci);
        }
    }

    void entergame(clientinfo *ci)
    {
        warnracecheat(ci);
    }

    void leavegame(clientinfo *ci, bool disconnecting = false)
    {
        if(disconnecting)
        {
            ci->state.flags = 0;    /* flags field is reused for cheating info */
            loopvrev(race_winners) if(race_winners[i].cn == ci->clientnum)
            {
                sendservmsgf("\f6race: \f7%s \f2left \f6%s PLACE!!", colorname(ci), placename(i));
                race_winners[i].cn = -1;
            }
        }
        if(state == ST_STARTED) checkplaces();
    }

    void spawned(clientinfo *ci)
    {
        if(ci->state.flags == 1) ci->state.flags = 0;
    }

    static void sendmaptoclients()
    {
        if(z_autosendmap == 1)
        {
            sendservmsg("[sending map to clients]");
            loopv(clients) if(clients[i]->state.aitype==AI_NONE) z_sendmap(clients[i], NULL, NULL, true, false);
        }
        else sendservmsg("[waiting for clients to load map]");
    }

    static bool canstartrace()
    {
        if(z_autosendmap != 1 && !smapname[0]) return true;
        loopv(clients) if(clients[i]->state.aitype == AI_NONE && clients[i]->state.state != CS_SPECTATOR)
        {
            if(z_autosendmap == 0)
            {
                if(!clients[i]->mapcrc) return false;
            }
            else if(z_autosendmap == 1)
            {
                if(clients[i]->getmap) return false;
            }
            else
            {
                if(!clients[i]->mapcrc || clients[i]->getmap) return false;
            }
        }
        return true;
    }

    static bool shouldshowtimer(int secs)
    {
        return secs >= 60 ? (secs % 60 == 0) : secs >= 30 ? (secs % 10 == 0) : secs >= 5 ? (secs % 5 == 0) : (secs > 0);
    }

    static const char *formatsecs(int secs)
    {
        int mins = secs / 60;
        secs %= 60;
        if(!mins) return tempformatstring("%d second%s", secs, secs != 1 ? "s" : "");
        else if(mins && secs) return tempformatstring("%d minute%s %d second%s", mins, mins != 1 ? "s" : "", secs, secs != 1 ? "s" : "");
        else return tempformatstring("%d minute%s", mins, mins != 1 ? "s" : "");
    }

    void update()
    {
        switch(state)
        {
            case ST_NONE:
            {
                bool hasmap = false, shouldstart;
                if(smapname[0])
                {
                    DELETEP(mapdata);
                    hasmap = z_loadmap(smapname);
                }
                shouldstart = race_winners.length() && (hasmap || z_autosendmap!=1);
                if(shouldstart)
                {
                    pausegame(true, NULL);
                    sendmaptoclients();
                }
                state = shouldstart ? ST_WAITMAP : ST_FINISHED;
                statemillis = countermillis = totalmillis;
                break;
            }

            case ST_WAITMAP:
                if(totalmillis-statemillis>=racemode_waitmap || canstartrace())
                {
                    state = ST_READY;
                    statemillis = totalmillis;
                    countermillis = totalmillis;
                }
                else break;
                /* FALL THROUGH */
            case ST_READY:
                if(totalmillis-countermillis >= 0)
                {
                    countermillis += 1000;
                    int secsleft = (racemode_startmillis - (totalmillis - statemillis) + 500)/1000;
                    if(shouldshowtimer(secsleft)) sendservmsgf("\f6race: \f2starting race in %s...", formatsecs(secsleft));
                }
                if(totalmillis-statemillis>=racemode_startmillis)
                {
                    state = ST_STARTED;
                    if(racemode_gamelimit)
                    {
                        statemillis = totalmillis + racemode_gamelimit;
                        if(!statemillis) statemillis = 1;
                        countermillis = totalmillis;
                    }
                    else statemillis = 0;
                    pausegame(false, NULL);
                    sendservmsg("\f6race: \f7START!!");
                    loopv(clients) if(clients[i]->state.state != CS_SPECTATOR)
                    {
                        if(clients[i]->state.state!=CS_EDITING) sendspawn(clients[i]);
                        else racecheat(clients[i], 1);
                    }
                }
                break;

            case ST_STARTED:
                if(statemillis)
                {
                    if(totalmillis-countermillis >= 0)
                    {
                        countermillis += 1000;
                        int secsleft = (statemillis - totalmillis + 500)/1000;
                        if(shouldshowtimer(secsleft)) sendservmsgf("\f6race: \f2time left: %s", formatsecs(secsleft));
                    }
                    if(statemillis-totalmillis <= 0)
                    {
                        /* jump straight into intermission */
                        startintermission();
                        state = ST_INT;
                    }
                }
                break;

            case ST_FINISHED:
                if(totalmillis-countermillis >= 0)
                {
                    countermillis += 1000;
                    int secsleft = (racemode_finishmillis - (totalmillis - statemillis) + 500)/1000;
                    if(shouldshowtimer(secsleft)) sendservmsgf("\f6race: \f2ending race in %s...", formatsecs(secsleft));
                }
                if(totalmillis-statemillis >= racemode_finishmillis)
                {
                    startintermission();
                    state = ST_INT;
                }
                break;

            case ST_INT:
                /* nothing to do here */
                break;
        }
    }

    void intermission()
    {
        vector<char> buf;
        const char * const msg_win = "\f6RACE WINNERS:";
        const char * const msg_plc = " PLACE: \f7";
        const char *msg_p;
        bool won = false;

        state = ST_INT;
        loopv(race_winners) if(race_winners[i].cn >= 0)
        {
            clientinfo *ci = getinfo(race_winners[i].cn);
            if(!ci) continue;
            if(!won) buf.put(msg_win, strlen(msg_win)); /* first time executing this */
            won = true;
            buf.add(' ');
            buf.add('\f');
            buf.add('2');
            msg_p = placename(i);
            buf.put(msg_p, strlen(msg_p));
            buf.put(msg_plc, strlen(msg_plc));
            msg_p = colorname(ci);
            buf.put(msg_p, strlen(msg_p));
            msg_p = tempformatstring(" (%d.%03ds)", race_winners[i].racemillis/1000, race_winners[i].racemillis%1000);
            buf.put(msg_p, strlen(msg_p));
        }
        if(!won) return;
        buf.add('\0');
        sendservmsg(buf.getbuf());
    }
};

bool isracemode()
{
    extern raceservmode racemode;
    return smode==&racemode;
}

#endif // Z_RACEMODE_H
