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
    vector<int> race_winners;
    vector<z_raceendinfo> raceends;

    raceservmode() { reset(); }

    void cleanup()
    {
        reset();
    }

    void reset()
    {
        state = ST_NONE;
        statemillis = gamemillis;
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
            while(race_winners.length() <= place) race_winners.add(-1);
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
            while(race_winners.length() <= r.place) race_winners.add(-1);
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
            if(race_winners[i] >= 0) numfinished++;
            else numavaiable++;
        }
        int numplayers = numclients(-1, true, false);
        if(numavaiable <= 0 || numplayers-numfinished <= 0)
        {
            state = ST_FINISHED;
            statemillis = countermillis = totalmillis;
        }
    }

    void moved(clientinfo *ci, const vec &oldpos, bool oldclip, const vec &newpos, bool newclip)
    {
        if(state != ST_STARTED)
        {
            if(gamepaused) update();    /* ugly hack: rely on N_POS messages since update isn't executed normally if paused */
            return;
        }
        int avaiable_place = -1;
        loopv(race_winners)
        {
            int cn = race_winners[i];
            if(cn == ci->clientnum) return;
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
            race_winners[avaiable_place] = ci->clientnum;
            sendservmsgf("\f6race: \f7%s \f2won \f6%s PLACE!!", colorname(ci), placename(avaiable_place));
            for(int j = race_winners.length()-1; j > avaiable_place; j--) if(race_winners[j] == ci->clientnum)
            {
                race_winners[j] = -1;
            }
            break;
        }
        checkplaces();
    }

    void leavegame(clientinfo *ci, bool disconnecting = false)
    {
        if(disconnecting)
        {
            loopvrev(race_winners) if(race_winners[i] == ci->clientnum)
            {
                sendservmsgf("\f6race: \f7%s \f2left \f6%s PLACE!!", colorname(ci), placename(i));
                race_winners[i] = -1;
            }
        }
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

    void update()
    {
        switch(state)
        {
            case ST_NONE:
                pausegame(true, NULL);
                if(smapname[0]) z_loadmap(smapname);
                sendmaptoclients();
                state = ST_WAITMAP;
                statemillis = totalmillis;
                break;

            case ST_WAITMAP:
                if(totalmillis-statemillis>=15000 || canstartrace())
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
                    int secsleft = (5000 - (totalmillis - statemillis) + 500)/1000;
                    if(secsleft > 0) sendservmsgf("\f6race: \f2starting race in %d...", secsleft);
                }
                if(totalmillis-statemillis>=5000)
                {
                    sendservmsg("\f6race: \f7START!!");
                    state = ST_STARTED;
                    pausegame(false, NULL);
                    loopv(clients) if(clients[i]->state.state != CS_SPECTATOR) sendspawn(clients[i]);
                }
                break;

            case ST_STARTED:
                /* nothing to do here */
                break;

            case ST_FINISHED:
                if(totalmillis-countermillis >= 0)
                {
                    countermillis += 1000;
                    int secsleft = (5000 - (totalmillis - statemillis) + 500)/1000;
                    if(secsleft > 0) sendservmsgf("\f6race: \f2ending race in %d...", secsleft);
                }
                if(totalmillis-statemillis >= 5000)
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

        loopv(race_winners) if(race_winners[i] >= 0)
        {
            clientinfo *ci = getinfo(race_winners[i]);
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
        }
        if(!won) return;
        buf.add('\0');
        sendservmsg(buf.getbuf());
    }
};

#endif // Z_RACEMODE_H
