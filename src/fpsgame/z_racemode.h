#ifdef Z_RACEMODE_H
#error "already z_racemode.h"
#endif
#define Z_RACEMODE_H

#ifndef Z_SENDMAP_H
#error "want z_sendmap.h"
#endif
#ifndef Z_AUTOSENDMAP_H
#error "want z_autosendmap.h"
#endif
#ifndef Z_LOADMAP_H
#error "want z_loadmap.h"
#endif
#ifndef Z_FORMAT_H
#error "want z_format.h"
#endif
#ifndef Z_RECORDS_H
#error "want z_records.h"
#endif

enum { Z_RC_NONE = 0, Z_RC_EDITMODE, Z_RC_EDITING };

#define RACE_DEFAULT_RADIUS     32
#define RACE_MAXPLACES          256

bool z_racemode = false;
VARF(racemode_enabled, 0, 0, 1,
{
    if(!racemode_enabled) z_racemode = false;
});
VARF(racemode_default, 0, 0, 1,
{
    if(!racemode_enabled) z_racemode = false;
    else if(clients.empty()) z_racemode = !!racemode_default;
});
static void z_racemode_trigger(int type)
{
    z_enable_command("racemode", !!racemode_enabled);
    z_racemode = racemode_enabled && racemode_default;
}
Z_TRIGGER(z_racemode_trigger, Z_TRIGGER_STARTUP);
Z_TRIGGER(z_racemode_trigger, Z_TRIGGER_NOCLIENTS);

void z_servcmd_racemode(int argc, char **argv, int sender)
{
    if(argc < 2) { sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("racemode is %s", z_racemode ? "enabled" : "disabled")); return; }
    z_racemode = atoi(argv[1]) > 0;
    sendservmsgf("racemode %s", z_racemode ? "enabled" : "disabled");
}
SCOMMANDA(racemode, PRIV_ADMIN, z_servcmd_racemode, 1);

VAR(racemode_allowcheat, 0, 0, 1);
VAR(racemode_allowedit, 0, 0, 1);
VAR(racemode_alloweditmode, 0, 1, 1);
VAR(racemode_allowmultiplaces, 0, 1, 1);
VAR(racemode_hideeditors, 0, 0, 1);
VAR(racemode_strict, 0, 0, 1);              // prefer strict logic to most convenient behavour

VAR(racemode_waitmap, 0, 10000, INT_MAX);
VAR(racemode_sendspectators, 0, 1, 1);
VAR(racemode_startmillis, 0, 5000, INT_MAX);
VAR(racemode_gamelimit, 0, 0, INT_MAX);
VAR(racemode_winnerwait, 0, 30000, INT_MAX);
VAR(racemode_finishmillis, 0, 5000, INT_MAX);
VAR(racemode_maxents, 0, 1000, MAXENTS);
VAR(racemode_minplaces, 1, 1, RACE_MAXPLACES);



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
    e.rend.place = clamp(*p, racemode_minplaces, RACE_MAXPLACES) - 1;   /* in command specified place starts from 1, in struct it starts from 0 */
});

VARF(racemode_record, 0, 0, 1,
{
    if(!racemode_record) z_clearrecords(M_EDIT);
});

VAR(racemode_record_atstart, 0, 1, 1); // whether to announce current record holder at start of race
VAR(racemode_record_atend, 0, 1, 1); // whether to announce current record holder at end of race
SVAR(racemode_record_style_atstart, "\f6RECORD HOLDER: \f7%O (%o)");
SVAR(racemode_record_style_atend, "\f6RECORD HOLDER: \f7%O (%o)");

static void z_racemode_showcurrentrecord(const char *fmt)
{
    z_findcurrentrecord();
    if(z_currentrecord < 0) return;
    z_record &r = z_records[z_currentrecord];

    z_formattemplate ft[] =
    {
        { 'O', "%s", (const void *)r.name },
        { 'o', "%s", (const void *)formatmillisecs(r.time) },
        { 0, NULL, NULL }
    };
    string buf;
    z_format(buf, sizeof(buf), fmt, ft);
    if(*buf) sendservmsg(buf);
}

// styling values
SVAR(racemode_style_winners, "\f6RACE WINNERS: %W");
SVAR(racemode_style_winplace, "\f2%P PLACE: \f7%C (%t)");
vector<char *> racemode_placestrings;
void racemode_style_places(tagval *args, int numargs)
{
    loopv(racemode_placestrings) delete[] racemode_placestrings[i];
    racemode_placestrings.setsize(0);
    loopi(numargs) racemode_placestrings.add(newstring(args[i].getstr()));
}
COMMAND(racemode_style_places, "sV");
SVAR(racemode_style_enterplace, "\f6race: \f7%C \f2won \f6%P PLACE!! \f7(%t)");
SVAR(racemode_style_leaveplace, "\f6race: \f7%C \f2left \f6%P PLACE!!");
SVAR(racemode_style_starting, "\f6race: \f2starting race in %T...");
SVAR(racemode_style_start, "\f6race: \f7START!!");
SVAR(racemode_style_timeleft, "\f2time left: %T");
SVAR(racemode_style_ending, "\f6race: \f2ending race in %T...");

SVAR(racemode_message_gotmap, "since you got the map, you may continue racing now");
SVAR(racemode_message_respawn, "since you respawned, you may continue racing");
SVAR(racemode_message_editmode, "edit mode is not allowed in racemode. please suicide to continue racing");
SVAR(racemode_message_editing, "editing map is not allowed in racemode. please getmap or reconnect to continue racing");

struct raceservmode: servmode
{
    enum { ST_NONE = 0, ST_WAITMAP, ST_READY, ST_STARTED, ST_FINISHED, ST_INT };

    int state;
    int statemillis;
    int countermillis;
    int minraceend;     // used when only non-first places are specified
    struct winner
    {
        int cn, racemillis, lifesequence;
        winner(): cn(-1), racemillis(0) {}
    };
    vector<winner> race_winners;
    vector<z_raceendinfo> raceends;

    raceservmode() { reset(); }

    void cleanup()
    {
        reset();
        loopv(clients) clients[i]->state.flags = clients[i]->state.frags = 0;
    }

    void initclient(clientinfo *ci, packetbuf &p, bool connecting)
    {
        // TODO: possibly notify client about modified mode
        if(ci)
        {
            if(connecting) ci->state.flags = 0;
        }
    }

    bool canspawn(clientinfo *ci, bool connecting = false)
    {
        return state >= ST_STARTED && state <= ST_INT;
    }

    bool hidefrags() { return true; }
    int fragvalue(clientinfo *victim, clientinfo *actor) { return 0; }

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
            r.place = clamp(-e.attr2, racemode_minplaces, RACE_MAXPLACES) - 1;
            while(race_winners.length() <= r.place) race_winners.add();
            if(r.place < minraceend) minraceend = r.place;
            if(racemode_maxents && raceends.length()>=racemode_maxents)
            {
                logoutf("WARNING: map \"%s\" has too much race end entities. only first %d are processed.", smapname, raceends.length());
                break;
            }
        }
    }

    static inline bool reached_raceend(const z_raceendinfo &r, const vec &o)
    {
        float dx = (r.o.x-o.x), dy = (r.o.y-o.y), dz = (r.o.z-o.z);
        return dx*dx + dy*dy <= r.radius*r.radius && fabs(dz) <= r.radius;
    }

    static const char *placename(int n)
    {
        static string buf;
        if(racemode_placestrings.empty())
        {
            // fill with default set of values
            racemode_placestrings.add(newstring("FIRST"));
            racemode_placestrings.add(newstring("SECOND"));
            racemode_placestrings.add(newstring("THIRD"));
            racemode_placestrings.add(newstring("%pth"));
        }
        const char *fs = racemode_placestrings.inrange(n) ? racemode_placestrings[n] : racemode_placestrings.last();
        z_formattemplate tmp[] =
        {
            { 'p', "%d", (const void *)(intptr_t)(n+1) },
            { 0, NULL, NULL }
        };
        z_format(buf, sizeof(buf), fs, tmp);
        return buf;
    }

    static void updateclientfragsnum(clientinfo &ci, int val)
    {
        ci.state.frags = val;
        sendresume(&ci);
    }

    int hasplace(clientinfo &ci)
    {
        loopv(race_winners) if(race_winners[i].cn == ci.clientnum) return race_winners.length() - i;
        return 0;
    }

    void checkplaces()
    {
        // start intermission 5 seconds after last place has been taken (or its too less clients to take any more places)
        uint cl_finished = 0, cl_unfinished = 0;
        loopv(clients) if(clients[i]->state.state!=CS_SPECTATOR)
        {
            // state.frags is reused for place number
            if(clients[i]->state.frags > 0) cl_finished++;
            else cl_unfinished++;
        }
        uint pl_finished = 0, pl_available = 0;
        loopv(race_winners)
        {
            if(race_winners[i].cn >= 0) pl_finished++;
            else pl_available++;
        }
        if((cl_finished + cl_unfinished) <= 0) return;  // don't end race if everyone leaves
        if(cl_unfinished <= 0 || pl_available <= 0)
        {
            // enter finished state if there are no places left or everyone already finished race
            state = ST_FINISHED;
            statemillis = countermillis = totalmillis;
        }
        else if(racemode_winnerwait && cl_finished > 0 && (!statemillis || statemillis-totalmillis > racemode_winnerwait))
        {
            // finish race after racemode_winnerwait
            statemillis = totalmillis + racemode_winnerwait;
            if(!statemillis) statemillis = 1;
            countermillis = totalmillis;
        }
    }

    static bool clientready(clientinfo *ci)
    {
        if(z_autosendmap == 2) return ci->mapcrc && !ci->getmap && ci->xi.maploaded && totalmillis-ci->xi.maploaded > 1000 && !ci->warned;
        if(z_autosendmap == 1) return !ci->getmap && ci->xi.maploaded && totalmillis-ci->xi.maploaded > 1000;
        return ci->mapcrc && ci->xi.maploaded && totalmillis-ci->xi.maploaded > 1000;
    }

    void moved(clientinfo *ci, const vec &oldpos, bool oldclip, const vec &newpos, bool newclip)
    {
        if(state != ST_STARTED && state != ST_FINISHED) return;
        if(ci->state.flags || !clientready(ci)) return; /* flags are reused for race cheating info */
        int available_place = -1;
        loopv(race_winners)
        {
            int cn = race_winners[i].cn;
            if(cn == ci->clientnum && (!racemode_allowmultiplaces || race_winners[i].lifesequence == ci->state.lifesequence)) break;
            if(cn < 0)
            {
                available_place = i;
                break;
            }
        }
        if(available_place >= 0) loopv(raceends)
        {
            z_raceendinfo &r = raceends[i];
            if(r.place > available_place && r.place > minraceend) continue;  /* don't skip if place is already minimal */
            if(!reached_raceend(r, newpos)) continue;
            int racemillis = 0;
            // leave lower places to allow others to win
            // remark: this is still illogical sometimes...
            for(int j = race_winners.length()-1; j > available_place; j--)
            {
                if(race_winners[j].cn == ci->clientnum && (!racemode_strict || !racemode_allowmultiplaces || race_winners[j].lifesequence == ci->state.lifesequence))
                {
                    race_winners[j].cn = -1;
                    // don't increase player's racetime
                    racemillis = race_winners[j].racemillis;
                }
            }
            if(!racemillis) racemillis = totalmillis - ci->state.lastdeath;      /* lastdeath is reused for spawntime */
            race_winners[available_place].cn = ci->clientnum;
            race_winners[available_place].racemillis = racemillis;
            race_winners[available_place].lifesequence = ci->state.lifesequence;

            z_formattemplate ft[] =
            {
                { 'C', "%s", (const void *)colorname(ci) },
                { 'c', "%s", (const void *)ci->name },
                { 'n', "%d", (const void *)(intptr_t)ci->clientnum },
                { 'P', "%s", (const void *)placename(available_place) },
                { 'p', "%d", (const void *)(intptr_t)(available_place+1) },
                { 't', "%s", (const void *)formatmillisecs(racemillis) },
                { 0, NULL, NULL }
            };
            string buf;
            z_format(buf, sizeof(buf), racemode_style_enterplace, ft);
            if(buf[0]) sendservmsg(buf);

            int plv = race_winners.length() - available_place;
            if(ci->state.frags < plv) updateclientfragsnum(*ci, plv);

            // possibly record it
            if(racemode_record) z_newrecord(ci, racemillis);

            break;
        }
        if(state == ST_STARTED) checkplaces();
    }

    void setracecheat(clientinfo &ci, int val)
    {
        bool changed = val != ci.state.flags;
        ci.state.flags = val;
        if(changed && !hasplace(ci)) updateclientfragsnum(ci, -val);
    }

    static void warnracecheat(clientinfo *ci)
    {
        if(ci->state.flags != Z_RC_EDITMODE && ci->state.flags != Z_RC_EDITING) return;
        sendf(ci->clientnum, 1, "ris", N_SERVMSG, ci->state.flags == Z_RC_EDITMODE ? racemode_message_editmode : racemode_message_editing);
    }

    void racecheat(clientinfo *ci, int type)
    {
        if(racemode_allowcheat) { setracecheat(*ci, Z_RC_NONE); return; }
        if(!racemode_alloweditmode && type == Z_RC_EDITMODE) type = Z_RC_EDITING;       // treat editmode as editing, if not allowed
        if((state < ST_STARTED && type < Z_RC_EDITING) || state > ST_FINISHED) return;  // editmode is allright before match starts
        if(ci->state.flags < type)
        {
            setracecheat(*ci, type);
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
            ci->state.frags = 0;
            int npl = race_winners.length();
            int leftplace = -1;
            for(int i = npl - 1; i >= 0; i--) if(race_winners[i].cn == ci->clientnum)
            {
                if(state < ST_INT)
                {
                    z_formattemplate ft[] =
                    {
                        { 'C', "%s", (const void *)colorname(ci) },
                        { 'c', "%s", (const void *)ci->name },
                        { 'n', "%d", (const void *)(intptr_t)ci->clientnum },
                        { 'P', "%s", (const void *)placename(i) },
                        { 'p', "%d", (const void *)(intptr_t)(i+1) },
                        { 't', "%s", (const void *)formatmillisecs(race_winners[i].racemillis) },
                        { 0, NULL, NULL }
                    };
                    string buf;
                    z_format(buf, sizeof(buf), racemode_style_leaveplace, ft);
                    if(*buf) sendservmsg(buf);
                }
                race_winners[i].cn = -1;
                leftplace = i;
            }
            if(leftplace >= 0)
            {
                // silently shift places
                for(int i = leftplace; i < npl; i++)
                {
                    if(racemode_strict && i > minraceend) break;
                    if(race_winners[i].cn < 0)
                    {
                        int rpl = -1;
                        for(int j = i + 1; j < npl; j++)
                        {
                            if(racemode_strict && j > minraceend) break;
                            if(race_winners[j].cn >= 0)
                            {
                                rpl = j;
                                break;
                            }
                        }
                        if(rpl < 0) break;
                        // cn racemillis lifesequence
                        race_winners[i] = race_winners[rpl];
                        race_winners[rpl].cn = -1;
                        clientinfo *ci = getinfo(race_winners[i].cn);
                        if(ci)
                        {
                            int plv = npl - i;
                            if(ci->state.frags < plv) updateclientfragsnum(*ci, plv);
                        }
                    }
                }
            }
        }
        if(state == ST_STARTED) checkplaces();
    }

    void spawned(clientinfo *ci)
    {
        if(ci->state.flags == Z_RC_EDITMODE)
        {
            setracecheat(*ci, Z_RC_NONE);
            if(ci->state.aitype==AI_NONE) sendf(ci->clientnum, 1, "ris", N_SERVMSG, racemode_message_respawn);
        }
        else if(ci->state.flags == Z_RC_EDITING && ci->state.aitype==AI_NONE) warnracecheat(ci);
    }

    static void sendmaptoclients()
    {
        switch(z_autosendmap)
        {
            case 0:
                sendservmsg("[waiting for clients to load map]");
                return;
            case 1:
                sendservmsg("[sending map to clients]");
                loopv(clients) if(clients[i]->state.aitype==AI_NONE && (racemode_sendspectators || clients[i]->state.state!=CS_SPECTATOR))
                {
                    z_sendmap(clients[i], NULL, NULL, true, false);
                    clients[i]->xi.mapsent = totalmillis ? totalmillis : 1;
                }
                return;
            case 2:
                sendservmsg("[waiting for clients to load map]");
                return;
        }
    }

    static bool canstartrace()
    {
        if(z_autosendmap != 1 && !smapname[0]) return true;
        loopv(clients) if(clients[i]->state.aitype == AI_NONE && clients[i]->state.state != CS_SPECTATOR)
        {
            if(!clientready(clients[i])) return false;
        }
        return true;
    }

    static bool shouldshowtimer(int secs)
    {
        return secs >= 60 ? (secs % 60 == 0) : secs >= 30 ? (secs % 10 == 0) : secs >= 5 ? (secs % 5 == 0) : (secs > 0);
    }

    bool holdpause() { return state == ST_WAITMAP || state == ST_READY; }

    void pausedupdate() { update(); }

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
                if(clients.empty())
                {
                    statemillis = totalmillis;
                    break;
                }
                if((racemode_waitmap && totalmillis-statemillis>=racemode_waitmap) || canstartrace())
                {
                    state = ST_READY;
                    statemillis = totalmillis;
                    countermillis = totalmillis;
                }
                else break;
                /* FALL THROUGH */
            case ST_READY:
                if(clients.empty())
                {
                    statemillis = countermillis = totalmillis;
                    state = ST_WAITMAP;
                    break;
                }
                if(totalmillis-countermillis >= 0)
                {
                    countermillis += 1000;
                    int secsleft = (racemode_startmillis - (totalmillis - statemillis) + 500)/1000;
                    if(shouldshowtimer(secsleft))
                    {
                        vector<char> timebuf;
                        z_formatsecs(timebuf, (uint)secsleft);
                        timebuf.add(0);
                        z_formattemplate ft[] =
                        {
                            { 'T', "%s", (const void *)timebuf.getbuf() },
                            { 0, NULL, NULL }
                        };
                        string buf;
                        z_format(buf, sizeof(buf), racemode_style_starting, ft);
                        if(buf[0]) sendservmsg(buf);
                    }
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
                    if(racemode_style_start[0]) sendservmsg(racemode_style_start);
                    loopv(clients) if(clients[i]->state.state != CS_SPECTATOR)
                    {
                        if(clients[i]->state.state!=CS_EDITING) sendspawn(clients[i]);
                        else racecheat(clients[i], 1);
                    }
                    if(racemode_record && racemode_record_atstart) z_racemode_showcurrentrecord(racemode_record_style_atstart);
                }
                break;

            case ST_STARTED:
                if(statemillis)
                {
                    if(totalmillis-countermillis >= 0)
                    {
                        countermillis += 1000;
                        int secsleft = (statemillis - totalmillis + 500)/1000;
                        if(shouldshowtimer(secsleft))
                        {
                            vector<char> timebuf;
                            z_formatsecs(timebuf, (uint)secsleft);
                            timebuf.add(0);
                            z_formattemplate ft[] =
                            {
                                { 'T', "%s", (const void *)timebuf.getbuf() },
                                { 0, NULL, NULL }
                            };
                            string buf;
                            z_format(buf, sizeof(buf), racemode_style_timeleft, ft);
                            if(buf[0]) sendservmsg(buf);
                        }
                    }
                    if(statemillis-totalmillis <= 0)
                    {
                        /* jump straight into intermission */
                        startintermission();
                    }
                }
                break;

            case ST_FINISHED:
                if(totalmillis-countermillis >= 0)
                {
                    countermillis += 1000;
                    int secsleft = (racemode_finishmillis - (totalmillis - statemillis) + 500)/1000;
                    if(shouldshowtimer(secsleft))
                    {
                        vector<char> timebuf;
                        z_formatsecs(timebuf, (uint)secsleft);
                        timebuf.add(0);
                        z_formattemplate ft[] =
                        {
                            { 'T', "%s", (const void *)timebuf.getbuf() },
                            { 0, NULL, NULL }
                        };
                        string buf;
                        z_format(buf, sizeof(buf), racemode_style_ending, ft);
                        if(buf[0]) sendservmsg(buf);
                    }
                }
                if(totalmillis-statemillis >= racemode_finishmillis) startintermission();
                break;

            case ST_INT:
                /* nothing to do here */
                break;
        }
    }

    int timeleft() const
    {
        return (state == ST_STARTED && statemillis) ? max((statemillis - totalmillis + 500)/1000, 0) : 0;
    }

    void intermission()
    {
        if(state < ST_STARTED) pausegame(false, NULL);
        state = ST_INT;
        DELETEP(mapdata);

        string tmp;
        vector<char> buf;

        loopv(race_winners) if(race_winners[i].cn >= 0)
        {
            clientinfo *ci = getinfo(race_winners[i].cn);
            if(!ci) continue;

            z_formattemplate style_tmp[] =
            {
                { 'p', "%d", (const void *)(intptr_t)(i+1) },
                { 'P', "%s", (const void *)placename(i) },
                { 'C', "%s", (const void *)colorname(ci) },
                { 'c', "%s", (const void *)ci->name },
                { 'n', "%d", (const void *)(intptr_t)ci->clientnum },
                { 't', "%s", (const void *)formatmillisecs(race_winners[i].racemillis) },
                { 0, NULL, NULL }
            };

            if(!buf.empty()) buf.add(' ');  // separate win entries by space
            z_format(tmp, sizeof(tmp), racemode_style_winplace, style_tmp);
            buf.put(tmp, strlen(tmp));
        }

        if(buf.empty()) return; // no winners

        buf.add(0);
        z_formattemplate style_tmp[] =
        {
            { 'W', "%s", (const void *)buf.getbuf() },
            { 0, NULL, NULL }
        };
        z_format(tmp, sizeof(tmp), racemode_style_winners, style_tmp);

        if(tmp[0]) sendservmsg(tmp);

        if(racemode_record && racemode_record_atend) z_racemode_showcurrentrecord(racemode_record_style_atend);
    }

    bool shouldblockgameplay(clientinfo *ci)
    {
        return racemode_hideeditors && ci->state.flags;
    }
};

bool isracemode()
{
    extern raceservmode racemode;
    return smode==&racemode;
}

void race_maploaded(clientinfo *ci)
{
    extern raceservmode racemode;
    if(smode==&racemode)
    {
        if(ci->state.flags > Z_RC_NONE && !ci->warned)
        {
            if(!racemode_allowcheat) racemode.setracecheat(*ci, ci->state.state!=CS_EDITING ? 0 : racemode_alloweditmode ? Z_RC_EDITMODE : Z_RC_EDITING);
            else racemode.setracecheat(*ci, Z_RC_NONE);
            if(ci->state.flags) raceservmode::warnracecheat(ci);
            else sendf(ci->clientnum, 1, "ris", N_SERVMSG, racemode_message_gotmap);
        }
        else if(!ci->state.flags && ci->warned)
        {
            if(!racemode_allowcheat) racemode.setracecheat(*ci, Z_RC_EDITING);
            else racemode.setracecheat(*ci, Z_RC_NONE);
            if(ci->state.flags) raceservmode::warnracecheat(ci);
        }
    }
}
