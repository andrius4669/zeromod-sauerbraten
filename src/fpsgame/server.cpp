#include "game.h"

namespace game
{
    void parseoptions(vector<const char *> &args)
    {
        loopv(args)
#ifndef STANDALONE
            if(!game::clientoption(args[i]))
#endif
            if(!server::serveroption(args[i]))
                conoutf(CON_ERROR, "unknown command-line option: %s", args[i]);
    }

    const char *gameident() { return "fps"; }
}

VAR(regenbluearmour, 0, 1, 1);

namespace server
{
    struct server_entity            // server side version of "entity" type
    {
        int type;
        int spawntime;
        bool spawned;
    };

    static const int DEATHMILLIS = 300;

    struct clientinfo;

    struct gameevent
    {
        virtual ~gameevent() {}

        virtual bool flush(clientinfo *ci, int fmillis);
        virtual void process(clientinfo *ci) {}

        virtual bool keepable() const { return false; }
    };

    struct timedevent : gameevent
    {
        int millis;

        bool flush(clientinfo *ci, int fmillis);
    };

    struct hitinfo
    {
        int target;
        int lifesequence;
        int rays;
        float dist;
        vec dir;
    };

    struct shotevent : timedevent
    {
        int id, gun;
        vec from, to;
        vector<hitinfo> hits;

        void process(clientinfo *ci);
    };

    struct explodeevent : timedevent
    {
        int id, gun;
        vector<hitinfo> hits;

        bool keepable() const { return true; }

        void process(clientinfo *ci);
    };

    struct suicideevent : gameevent
    {
        void process(clientinfo *ci);
    };

    struct pickupevent : gameevent
    {
        int ent;

        void process(clientinfo *ci);
    };

    template <int N>
    struct projectilestate
    {
        int projs[N];
        int numprojs;

        projectilestate() : numprojs(0) {}

        void reset() { numprojs = 0; }

        void add(int val)
        {
            if(numprojs>=N) numprojs = 0;
            projs[numprojs++] = val;
        }

        bool remove(int val)
        {
            loopi(numprojs) if(projs[i]==val)
            {
                projs[i] = projs[--numprojs];
                return true;
            }
            return false;
        }
    };

    struct gamestate : fpsstate
    {
        vec o;
        int state, editstate;
        int lastdeath, deadflush, lastspawn, lifesequence;
        int lastshot;
        projectilestate<8> rockets, grenades;
        int frags, flags, deaths, teamkills, shotdamage, damage, tokens;
        int suicides, explosivedamage, hits, misses, shots; // zeromod
        int lasttimeplayed, timeplayed;
        float effectiveness;
        // zeromod
        int stolen, returned, maxstreak;
        int lastkill, multikills, rampage;
        hashset<teaminfo> *teaminfos;
        int statemillis; // totalmillis when state changed to current value

        gamestate() : state(CS_DEAD), editstate(CS_DEAD), lifesequence(0), teaminfos(NULL), statemillis(totalmillis) {}
        ~gamestate() { delete teaminfos; }

        bool isalive(int gamemillis)
        {
            return state==CS_ALIVE || (state==CS_DEAD && gamemillis - lastdeath <= DEATHMILLIS);
        }

        bool waitexpired(int gamemillis)
        {
            return gamemillis - lastshot >= gunwait;
        }

        void reset()
        {
            if(state!=CS_SPECTATOR) state = editstate = CS_DEAD;
            maxhealth = 100;
            rockets.reset();
            grenades.reset();

            timeplayed = 0;
            effectiveness = 0;
            frags = flags = deaths = suicides = teamkills = shotdamage = explosivedamage = damage = hits = misses = shots = tokens = 0;

            lastdeath = 0;
            stolen = returned = maxstreak = 0;
            DELETEP(teaminfos);
            statemillis = totalmillis;

            respawn();
        }

        void respawn()
        {
            fpsstate::respawn();
            o = vec(-1e10f, -1e10f, -1e10f);
            deadflush = 0;
            lastspawn = -1;
            lastshot = 0;
            tokens = 0;
            lastkill = multikills = rampage = 0;
        }

        void reassign()
        {
            respawn();
            rockets.reset();
            grenades.reset();
        }
    };

    #include "z_tree.h"
    #include "z_tools.h"

    struct savedscore
    {
        uint ip;
        string name;
        int frags, flags, deaths, teamkills, shotdamage, damage;
#ifdef OLDPROTO
        int maxhealth;
#endif
        int suicides, explosivedamage, hits, misses, shots;
        int timeplayed;
        float effectiveness;
        int stolen, returned, maxstreak;
        hashset<teaminfo> *teaminfos;
        savedscore(): teaminfos(NULL) {}
        ~savedscore() { if(teaminfos) delete teaminfos; }

        void save(gamestate &gs)
        {
#ifdef OLDPROTO
            maxhealth = gs.maxhealth;
#endif
            frags = gs.frags;
            flags = gs.flags;
            deaths = gs.deaths;
            suicides = gs.suicides;
            teamkills = gs.teamkills;
            shotdamage = gs.shotdamage;
            explosivedamage = gs.explosivedamage;
            damage = gs.damage;
            timeplayed = gs.timeplayed;
            effectiveness = gs.effectiveness;
            stolen = gs.stolen;
            returned = gs.returned;
            maxstreak = gs.maxstreak;
            z_setteaminfos(teaminfos, gs.teaminfos);
            hits = gs.hits;
            misses = gs.misses;
            shots = gs.shots;
        }

        void restore(gamestate &gs)
        {
#ifdef OLDPROTO
            if(gs.health==gs.maxhealth) gs.health = maxhealth;
            gs.maxhealth = maxhealth;
#endif
            gs.frags = frags;
            gs.flags = flags;
            gs.deaths = deaths;
            gs.suicides = suicides;
            gs.teamkills = teamkills;
            gs.shotdamage = shotdamage;
            gs.explosivedamage = explosivedamage;
            gs.damage = damage;
            gs.timeplayed = timeplayed;
            gs.effectiveness = effectiveness;
            gs.stolen = stolen;
            gs.returned = returned;
            gs.maxstreak = maxstreak;
            z_setteaminfos(gs.teaminfos, teaminfos);
            gs.hits = hits;
            gs.misses = misses;
            gs.shots = shots;
        }
    };

    extern int gamemillis, nextexceeded;

    #include "z_geoipstate.h"
    #include "z_queue.h"
    #include "z_extrainfo.h"

    struct clientinfo
    {
        int clientnum, ownernum, connectmillis, sessionid, overflow;
        string name, team, mapvote;
        int playermodel;
        int modevote;
        int privilege;
        bool connected, local, timesync;
        int gameoffset, lastevent, pushed, exceeded;
        gamestate state;
        vector<gameevent *> events;
        vector<uchar> position, messages;
        uchar *wsdata;
        int wslen;
        vector<clientinfo *> bots;
        int ping, aireinit;
        string clientmap;
        int mapcrc;
        bool warned, gameclip;
        ENetPacket *getdemo, *getmap, *clipboard;
        int lastclipboard, needclipboard;
        int connectauth;
        uint authreq;
        int authmaster;
        string authname, authdesc;
        void *authchallenge;
        int authkickvictim;
        char *authkickreason;

        z_extrainfo xi;
        bool spy;

        clientinfo() : getdemo(NULL), getmap(NULL), clipboard(NULL), authchallenge(NULL), authkickreason(NULL) { reset(); }
        ~clientinfo() { events.deletecontents(); cleanclipboard(); cleanauth(); }

        void addevent(gameevent *e)
        {
            if(state.state==CS_SPECTATOR || events.length()>100) delete e;
            else events.add(e);
        }

        enum
        {
            PUSHMILLIS = 3000
        };

        int calcpushrange()
        {
            ENetPeer *peer = getclientpeer(ownernum);
            return PUSHMILLIS + (peer ? peer->roundTripTime + peer->roundTripTimeVariance : ENET_PEER_DEFAULT_ROUND_TRIP_TIME);
        }

        bool checkpushed(int millis, int range)
        {
            return millis >= pushed - range && millis <= pushed + range;
        }

        void scheduleexceeded()
        {
            if(state.state!=CS_ALIVE || !exceeded) return;
            int range = calcpushrange();
            if(!nextexceeded || exceeded + range < nextexceeded) nextexceeded = exceeded + range;
        }

        void setexceeded()
        {
            if(state.state==CS_ALIVE && !exceeded && !checkpushed(gamemillis, calcpushrange())) exceeded = gamemillis;
            scheduleexceeded(); 
        }
            
        void setpushed()
        {
            pushed = max(pushed, gamemillis);
            if(exceeded && checkpushed(exceeded, calcpushrange())) exceeded = 0;
        }
        
        bool checkexceeded()
        {
            return state.state==CS_ALIVE && exceeded && gamemillis > exceeded + calcpushrange();
        }

        void mapchange()
        {
            mapvote[0] = 0;
            modevote = INT_MAX;
            state.reset();
            events.deletecontents();
            overflow = 0;
            timesync = false;
            lastevent = 0;
            exceeded = 0;
            pushed = 0;
            clientmap[0] = '\0';
            mapcrc = 0;
            warned = false;
            gameclip = false;
            xi.mapchange();
        }

        void reassign()
        {
            state.reassign();
            events.deletecontents();
            timesync = false;
            lastevent = 0;
        }

        void cleanclipboard(bool fullclean = true)
        {
            if(clipboard) { if(--clipboard->referenceCount <= 0) enet_packet_destroy(clipboard); clipboard = NULL; }
            if(fullclean) lastclipboard = 0;
        }

        void cleanauthkick()
        {
            authkickvictim = -1;
            DELETEA(authkickreason);
        }

        void cleanauth(bool full = true)
        {
            authreq = 0;
            authmaster = -1;
            if(authchallenge) { freechallenge(authchallenge); authchallenge = NULL; }
            if(full) cleanauthkick();
        }

        void reset()
        {
            name[0] = team[0] = 0;
            playermodel = -1;
            privilege = PRIV_NONE;
            connected = local = false;
            connectauth = 0;
            position.setsize(0);
            messages.setsize(0);
            ping = 0;
            aireinit = 0;
            needclipboard = 0;
            cleanclipboard();
            cleanauth();
            xi.reset();
            spy = false;
            mapchange();
        }

        int geteventmillis(int servmillis, int clientmillis)
        {
            if(!timesync || (events.empty() && state.waitexpired(servmillis)))
            {
                timesync = true;
                gameoffset = servmillis - clientmillis;
                return servmillis;
            }
            else return gameoffset + clientmillis;
        }
    };

    enum { BAN_KICK = 0, BAN_TEAMKILL, BAN_SPECTATE, BAN_MUTE };
    struct ban
    {
        int time, expire;
        uint ip;
        int type;
        char *reason;   // unique pointer
        ban(): reason(NULL) {}
        ban(const ban &b): reason(NULL) { *this = b; }
        ~ban() { delete[] reason; }
        ban &operator =(const ban &b)
        {
            if(&b != this) {
                time = b.time;
                expire = b.expire;
                ip = b.ip;
                type = b.type;
                delete[] reason;
                reason = b.reason;
                ((ban *)&b)->reason = NULL;    // ugly hack
            }
            return *this;
        }
    };

    namespace aiman
    {
        extern void removeai(clientinfo *ci);
        extern void clearai();
        extern void checkai();
        extern void reqadd(clientinfo *ci, int skill);
        extern void reqdel(clientinfo *ci);
        extern void setbotlimit(clientinfo *ci, int limit);
        extern void setbotbalance(clientinfo *ci, bool balance);
        extern void changemap();
        extern void addclient(clientinfo *ci);
        extern void changeteam(clientinfo *ci);
    }

    #define MM_MODE 0xF
    #define MM_AUTOAPPROVE 0x1000
    #define MM_PRIVSERV (MM_MODE | MM_AUTOAPPROVE)
    #define MM_PUBSERV ((1<<MM_OPEN) | (1<<MM_VETO))
    #define MM_COOPSERV (MM_AUTOAPPROVE | MM_PUBSERV | (1<<MM_LOCKED))

    bool notgotitems = true;        // true when map has changed and waiting for clients to send item
    int gamemode = 0;
    int gamemillis = 0, gamelimit = 0, nextexceeded = 0, gamespeed = 100;
    bool gamepaused = false, shouldstep = true;

    string smapname = "";
    int interm = 0;
    enet_uint32 lastsend = 0;
    int mastermode = MM_OPEN, mastermask = MM_PRIVSERV;
    stream *mapdata = NULL;

    vector<uint> allowedips;
    vector<ban> bannedips;

    void addban(uint ip, int expire, int type = BAN_KICK, const char *reason = NULL)
    {
        allowedips.removeobj(ip);
        ban b;
        b.time = totalmillis;
        b.expire = totalmillis + expire;
        b.ip = ip;
        b.type = type;
        if(reason && reason[0]) b.reason = newstring(reason);
        loopv(bannedips) if(bannedips[i].expire - b.expire > 0) { bannedips.insert(i, b); return; }
        bannedips.add(b);
    }

    vector<clientinfo *> connects, clients, bots;

    void kickclients(uint ip, clientinfo *actor = NULL, int priv = PRIV_NONE)
    {
        loopvrev(clients)
        {
            clientinfo &c = *clients[i];
            if(c.state.aitype != AI_NONE || c.privilege >= PRIV_ADMIN || c.local) continue;
            if(actor && ((c.privilege > priv && !actor->local) || c.clientnum == actor->clientnum)) continue;
            if(getclientip(c.clientnum) == ip) disconnect_client(c.clientnum, DISC_KICK);
        }
    }
 
    struct maprotation
    {
        static int exclude;
        int modes;
        string map;
        
        int calcmodemask() const { return modes&(1<<NUMGAMEMODES) ? modes & ~exclude : modes; }
        bool hasmode(int mode, int offset = STARTGAMEMODE) const { return (calcmodemask() & (1 << (mode-offset))) != 0; }

        int findmode(int mode) const
        {
            if(!hasmode(mode)) loopi(NUMGAMEMODES) if(hasmode(i, 0)) return i+STARTGAMEMODE;
            return mode;
        }

        bool match(int reqmode, const char *reqmap) const
        {
            return hasmode(reqmode) && (!map[0] || !reqmap[0] || !strcmp(map, reqmap));
        }

        bool includes(const maprotation &rot) const
        {
            return rot.modes == modes ? rot.map[0] && !map[0] : (rot.modes & modes) == rot.modes;
        }
    };
    int maprotation::exclude = 0;
    vector<maprotation> maprotations;
    int curmaprotation = 0;

    VAR(lockmaprotation, 0, 0, 2);
    VAR(restrictmapvote, 0, 0, 2);

    void maprotationreset()
    {
        maprotations.setsize(0);
        curmaprotation = 0;
        maprotation::exclude = 0;
    }

    #include "z_format.h"
    #include "z_servercommands.h"
    #include "z_maprotation.h"
    void nextmaprotation()
    {
        if(z_nextmaprotation()) return;
        curmaprotation++;
        if(maprotations.inrange(curmaprotation) && maprotations[curmaprotation].modes) return;
        do curmaprotation--;
        while(maprotations.inrange(curmaprotation) && maprotations[curmaprotation].modes);
        curmaprotation++;
    }

    int findmaprotation(int mode, const char *map)
    {
        // try finding map in current slice starting from current maprotation pointer
        for(int i = max(curmaprotation, 0); i < maprotations.length(); i++)
        {
            maprotation &rot = maprotations[i];
            if(!rot.modes) break;
            if(rot.match(mode, map)) return i;
        }
        // seek back to slice start
        int start;
        for(start = clamp(curmaprotation, 0, maprotations.length()) - 1; start >= 0; start--) if(!maprotations[start].modes) break;
        start++;
        // search from begining of slice to current maprotation pointer
        for(int i = start; i < min(curmaprotation, maprotations.length()); i++)
        {
            maprotation &rot = maprotations[i];
            if(!rot.modes) break;
            if(rot.match(mode, map)) return i;
        }
        // if still not done, try finding best matching map/mode combo in whole rotation
        int best = -1;
        loopv(maprotations)
        {
            maprotation &rot = maprotations[i];
            if(rot.match(mode, map) && (best < 0 || maprotations[best].includes(rot))) best = i;
        }
        return best;
    }

    bool searchmodename(const char *haystack, const char *needle)
    {
        if(!needle[0]) return true;
        do
        {
            if(needle[0] != '.')
            {
                haystack = strchr(haystack, needle[0]);
                if(!haystack) break;
                haystack++;
            }
            const char *h = haystack, *n = needle+1;
            for(; *h && *n; h++)
            {
                if(*h == *n) n++;
                else if(*h != ' ') break; 
            }
            if(!*n) return true;
            if(*n == '.') return !*h;
        } while(needle[0] != '.');
        return false;
    }

    int genmodemask(vector<char *> &modes)
    {
        int modemask = 0;
        loopv(modes)
        {
            const char *mode = modes[i];
            int op = mode[0];
            switch(mode[0])
            {
                case '*':
                    modemask |= 1<<NUMGAMEMODES;
                    loopk(NUMGAMEMODES) if(m_checknot(k+STARTGAMEMODE, M_DEMO|M_EDIT|M_LOCAL)) modemask |= 1<<k;
                    continue;
                case '!':
                    mode++;
                    if(mode[0] != '?') break;
                case '?':
                    mode++;
                    loopk(NUMGAMEMODES) if(searchmodename(gamemodes[k].name, mode))
                    {
                        if(op == '!') modemask &= ~(1<<k);
                        else modemask |= 1<<k;
                    }
                    continue;
            }
            int modenum = INT_MAX;
            if(isdigit(mode[0])) modenum = atoi(mode);
            else loopk(NUMGAMEMODES) if(searchmodename(gamemodes[k].name, mode)) { modenum = k+STARTGAMEMODE; break; }
            if(!m_valid(modenum)) continue;
            switch(op)
            {
                case '!': modemask &= ~(1 << (modenum - STARTGAMEMODE)); break;
                default: modemask |= 1 << (modenum - STARTGAMEMODE); break;
            }
        }
        return modemask;
    }
         
    bool addmaprotation(int modemask, const char *map)
    {
        if(!map[0]) loopk(NUMGAMEMODES) if(modemask&(1<<k) && !m_check(k+STARTGAMEMODE, M_EDIT)) modemask &= ~(1<<k);
        if(!modemask) return false;
        if(!(modemask&(1<<NUMGAMEMODES))) maprotation::exclude |= modemask;
        maprotation &rot = maprotations.add();
        rot.modes = modemask;
        copystring(rot.map, map);
        return true;
    }
        
    void addmaprotations(tagval *args, int numargs)
    {
        vector<char *> modes, maps;
        for(int i = 0; i + 1 < numargs; i += 2)
        {
            explodelist(args[i].getstr(), modes);
            explodelist(args[i+1].getstr(), maps);
            int modemask = genmodemask(modes);
            if(maps.length()) loopvj(maps) addmaprotation(modemask, maps[j]);
            else addmaprotation(modemask, "");
            modes.deletearrays();
            maps.deletearrays();
        }
        if(maprotations.length() && maprotations.last().modes)
        {
            maprotation &rot = maprotations.add();
            rot.modes = 0;
            rot.map[0] = '\0';
        }
    }
    
    COMMAND(maprotationreset, "");
    COMMANDN(maprotation, addmaprotations, "ss2V");

    struct demofile
    {
        string info;
        uchar *data;
        int len;
    };

    vector<demofile> demos;

    bool demonextmatch = false;
    bool demonextmatch_set = false;

    stream *demotmp = NULL, *demorecord = NULL, *demoplayback = NULL;
    int nextplayback = 0;

    VAR(maxdemos, 0, 5, 25);
    VAR(maxdemosize, 0, 16, 31);
    VAR(restrictdemos, 0, 1, 1);
    VAR(autorecorddemo, 0, 0, 1);

    VAR(restrictpausegame, 0, 1, 1);
    VAR(restrictgamespeed, 0, 1, 1);

    SVARF(serverdesc, "", z_serverdescchanged());
    SVAR(serverpass, "");
    SVAR(adminpass, "");
    VARF(publicserver, 0, 0, 2, {
		switch(publicserver)
		{
			case 0: default: mastermask = MM_PRIVSERV; break;
			case 1: mastermask = MM_PUBSERV; break;
			case 2: mastermask = MM_COOPSERV; break;
		}
	});
    SVAR(servermotd, "");

    // old alias
    VARF(serverrecorddemo, 0, 0, 1, autorecorddemo = serverrecorddemo);

    static inline bool demonextmatch_get()
    {
        if (!demonextmatch_set)
        {
            demonextmatch = autorecorddemo;
            demonextmatch_set = true;
        }
        return demonextmatch;
    }

    struct teamkillkick
    {
        int modes, limit, ban;

        bool match(int mode) const
        {
            return (modes&(1<<(mode-STARTGAMEMODE)))!=0;
        }

        bool includes(const teamkillkick &tk) const
        {
            return tk.modes != modes && (tk.modes & modes) == tk.modes;
        }
    };
    vector<teamkillkick> teamkillkicks;

    void teamkillkickreset()
    {
        teamkillkicks.setsize(0);
    }

    void addteamkillkick(char *modestr, int *limit, int *ban)
    {
        vector<char *> modes;
        explodelist(modestr, modes);
        teamkillkick &kick = teamkillkicks.add();
        kick.modes = genmodemask(modes);
        kick.limit = *limit;
        kick.ban = *ban > 0 ? *ban*60000 : (*ban < 0 ? 0 : 30*60000); 
        modes.deletearrays();
    }

    COMMAND(teamkillkickreset, "");
    COMMANDN(teamkillkick, addteamkillkick, "sii");

    struct teamkillinfo
    {
        uint ip;
        int teamkills;
    };
    vector<teamkillinfo> teamkills;
    bool shouldcheckteamkills = false;

    void addteamkill(clientinfo *actor, clientinfo *victim, int n)
    {
        if(!m_timed || actor->state.aitype != AI_NONE || actor->local || actor->privilege || (victim && victim->state.aitype != AI_NONE)) return;
        shouldcheckteamkills = true;
        uint ip = getclientip(actor->clientnum);
        loopv(teamkills) if(teamkills[i].ip == ip) 
        { 
            teamkills[i].teamkills += n;
            return;
        }
        teamkillinfo &tk = teamkills.add();
        tk.ip = ip;
        tk.teamkills = n;
    }

    #include "z_bans.h"

    void checkteamkills()
    {
        teamkillkick *kick = NULL;
        if(m_timed) loopv(teamkillkicks) if(teamkillkicks[i].match(gamemode) && (!kick || kick->includes(teamkillkicks[i])))
            kick = &teamkillkicks[i];
        if(kick) loopvrev(teamkills)
        {
            teamkillinfo &tk = teamkills[i];
            if(tk.teamkills >= kick->limit)
            {
                if(kick->ban > 0) addban(tk.ip, kick->ban, BAN_TEAMKILL);
                z_kickteamkillers(tk.ip);
                teamkills.removeunordered(i);
            }
        }
        shouldcheckteamkills = false;
    }

    void *newclientinfo() { return new clientinfo; }
    void deleteclientinfo(void *ci) { delete (clientinfo *)ci; }

    clientinfo *getinfo(int n)
    {
        if(n < MAXCLIENTS) return (clientinfo *)getclientinfo(n);
        n -= MAXCLIENTS;
        return bots.inrange(n) ? bots[n] : NULL;
    }

    uint mcrc = 0;
    vector<entity> ments;
    vector<server_entity> sents;
    vector<savedscore> scores;

    int msgsizelookup(int msg)
    {
        static int sizetable[NUMMSG] = { -1 };
        if(sizetable[0] < 0)
        {
            memset(sizetable, -1, sizeof(sizetable));
            for(const int *p = msgsizes; *p >= 0; p += 2) sizetable[p[0]] = p[1];
        }
        return msg >= 0 && msg < NUMMSG ? sizetable[msg] : -1;
    }

    const char *modename(int n, const char *unknown)
    {
        if(m_valid(n)) return gamemodes[n - STARTGAMEMODE].name;
        return unknown;
    }

    const char *mastermodename(int n, const char *unknown)
    {
        return (n>=MM_START && size_t(n-MM_START)<sizeof(mastermodenames)/sizeof(mastermodenames[0])) ? mastermodenames[n-MM_START] : unknown;
    }

    const char *privname(int type)
    {
        switch(type)
        {
            case PRIV_ADMIN: return "admin";
            case PRIV_AUTH: return "auth";
            case PRIV_MASTER: return "master";
            default: return "unknown";
        }
    }

    void sendservmsg(const char *s) { sendf(-1, 1, "ris", N_SERVMSG, s); }
    void sendservmsgf(const char *fmt, ...)
    {
         defvformatstring(s, fmt, fmt);
         sendf(-1, 1, "ris", N_SERVMSG, s);
    }

    void resetitems()
    {
        mcrc = 0;
        ments.setsize(0);
        sents.setsize(0);
        //cps.reset();
    }

    bool serveroption(const char *arg)
    {
        if(arg[0]=='-') switch(arg[1])
        {
            case 'n': setsvar("serverdesc", &arg[2]); return true;
            case 'y': setsvar("serverpass", &arg[2]); return true;
            case 'p': setsvar("adminpass", &arg[2]); return true;
            case 'o': setvar("publicserver", atoi(&arg[2])); return true;
        }
        return false;
    }

    #include "z_triggers.h"
    void serverinit()
    {
        smapname[0] = '\0';
        resetitems();
        z_exectrigger(Z_TRIGGER_STARTUP);
    }

    int numspies(int exclude = -1, bool nospec = true, bool noai = true, bool priv = false)
    {
        int n = 0;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(!ci->spy) continue;
            if(ci->clientnum!=exclude && (!nospec || ci->state.state!=CS_SPECTATOR || (priv && (ci->privilege || ci->local))) && (!noai || ci->state.aitype == AI_NONE)) n++;
        }
        return n;
    }

    int numclients(int exclude = -1, bool nospec = true, bool noai = true, bool priv = false)
    {
        int n = 0;
        loopv(clients) 
        {
            clientinfo *ci = clients[i];
            if(ci->spy) continue;
            if(ci->clientnum!=exclude &&
                (!nospec || ci->state.state!=CS_SPECTATOR ||
                    (priv && (ci->privilege || ci->local))) &&
                (!noai || ci->state.aitype == AI_NONE))
            {
                n++;
            }
        }
        return n;
    }

    // count connected peers. skip bots and talkbots. include spies.
    int actualclientslen()
    {
        int n = 0;
        loopv(clients) if(clients[i]->state.aitype == AI_NONE) ++n;
        return n;
    }

    // count all clients visible in scoreboard. include bots/talkbots/specs. don't include spies
    int visibleclientslen()
    {
        return numclients(-1, false, false);
    }

    bool duplicatename(clientinfo *ci, const char *name)
    {
        if(!name) name = ci->name;
        loopv(clients) if(clients[i]!=ci && !clients[i]->spy && !strcmp(name, clients[i]->name)) return true;
        return false;
    }

    const char *colorname(clientinfo *ci, const char *name)
    {
        if(!name) name = ci->name;
        if(name[0] && !ci->spy && !duplicatename(ci, name) && ci->state.aitype == AI_NONE) return name;
        static string cname[3];
        static int cidx = 0;
        cidx = (cidx+1)%3;
        formatstring(cname[cidx], ci->state.aitype == AI_NONE ? "%s \fs\f5(%d)\fr" : "%s \fs\f5[%d]\fr", name, ci->clientnum);
        return cname[cidx];
    }

    struct servmode
    {
        virtual ~servmode() {}

        virtual void entergame(clientinfo *ci) {}
        virtual void leavegame(clientinfo *ci, bool disconnecting = false) {}

        virtual void moved(clientinfo *ci, const vec &oldpos, bool oldclip, const vec &newpos, bool newclip) {}
        virtual bool canspawn(clientinfo *ci, bool connecting = false) { return true; }
        virtual void spawned(clientinfo *ci) {}
        virtual int fragvalue(clientinfo *victim, clientinfo *actor)
        {
            if(victim==actor || isteam(victim->team, actor->team)) return -1;
            return 1;
        }
        virtual void died(clientinfo *victim, clientinfo *actor) {}
        virtual bool canchangeteam(clientinfo *ci, const char *oldteam, const char *newteam) { return true; }
        virtual void changeteam(clientinfo *ci, const char *oldteam, const char *newteam) {}
        virtual void initclient(clientinfo *ci, packetbuf &p, bool connecting) {}
        virtual void update() {}
        virtual void cleanup() {}
        virtual void setup() {}
        virtual void newmap() {}
        virtual void intermission() {}
        virtual bool hidefrags() { return false; }
        virtual int getteamscore(const char *team) { return 0; }
        virtual void getteamscores(vector<teamscore> &scores) {}
        virtual bool extinfoteam(const char *team, ucharbuf &p) { return false; }

        // zeromod
        virtual bool autoteam() { return false; }
        virtual void pausedupdate() {}
        virtual bool holdpause() { return false; }
        virtual int timeleft() const { return -1; }
        virtual bool shouldblockgameplay(clientinfo *ci) { return false; }
        virtual bool shouldhidepos(clientinfo *ci) { return shouldblockgameplay(ci); }
    };

    #define SERVMODE 1
    #include "capture.h"
    #include "ctf.h"
    #include "collect.h"

    captureservmode capturemode;
    ctfservmode ctfmode;
    collectservmode collectmode;
    servmode *smode = NULL;

    bool canspawnitem(int type) { return !m_noitems && (type>=I_SHELLS && type<=I_QUAD && (!m_noammo || type<I_SHELLS || type>I_CARTRIDGES)); }

    int spawntime(int type)
    {
        if(m_classicsp) return INT_MAX;
        int np = numclients(-1, true, false);
        np = np<3 ? 4 : (np>4 ? 2 : 3);         // spawn times are dependent on number of players
        int sec = 0;
        switch(type)
        {
            case I_SHELLS:
            case I_BULLETS:
            case I_ROCKETS:
            case I_ROUNDS:
            case I_GRENADES:
            case I_CARTRIDGES: sec = np*4; break;
            case I_HEALTH: sec = np*5; break;
            case I_GREENARMOUR: sec = 20; break;
            case I_YELLOWARMOUR: sec = 30; break;
            case I_BOOST: sec = 60; break;
            case I_QUAD: sec = 70; break;
        }
        return sec*1000;
    }

    bool delayspawn(int type)
    {
        switch(type)
        {
            case I_GREENARMOUR:
            case I_YELLOWARMOUR:
                return !m_classicsp;
            case I_BOOST:
            case I_QUAD:
                return true;
            default:
                return false;
        }
    }
 
    bool pickup(int i, int sender)         // server side item pickup, acknowledge first client that gets it
    {
        if((m_timed && gamemillis>=gamelimit) || !sents.inrange(i) || !sents[i].spawned) return false;
        clientinfo *ci = getinfo(sender);
        if(!ci) return false;
        if(!ci->local && !ci->state.canpickup(sents[i].type))
        {
#ifndef OLDPROTO
            sendf(sender, 1, "ri3", N_ITEMACC, i, -1);
#endif
            return false;
        }
        sents[i].spawned = false;
        sents[i].spawntime = spawntime(sents[i].type);
        sendf(-1, 1, "ri3", N_ITEMACC, i, sender);
        ci->state.pickup(sents[i].type);
        return true;
    }

    static hashset<teaminfo> teaminfos;

    void clearteaminfo()
    {
        teaminfos.clear();
    }

    bool teamhasplayers(const char *team) { loopv(clients) if(!strcmp(clients[i]->team, team)) return true; return false; }

    bool pruneteaminfo()
    {
        int oldteams = teaminfos.numelems;
        enumerate(teaminfos, teaminfo, old,
            if(!old.frags && !teamhasplayers(old.team)) teaminfos.remove(old.team);
        );
        return teaminfos.numelems < oldteams;
    }

    teaminfo *addteaminfo(const char *team)
    {
        teaminfo *t = teaminfos.access(team);
        if(!t)
        {
            if(teaminfos.numelems >= MAXTEAMS && !pruneteaminfo()) return NULL;
            t = &teaminfos[team];
            copystring(t->team, team, sizeof(t->team));
            t->frags = 0;
        }
        return t;
    }

    clientinfo *choosebestclient(float &bestrank)
    {
        clientinfo *best = NULL;
        bestrank = -1;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.timeplayed<0) continue;
            float rank = ci->state.state!=CS_SPECTATOR ? ci->state.effectiveness/max(ci->state.timeplayed, 1) : -1;
            if(!best || rank > bestrank) { best = ci; bestrank = rank; }
        }
        return best;
    }

    #include "z_persistteams.h"

#if 0
    void autoteam()
    {
        static const char * const teamnames[2] = {"good", "evil"};
        vector<clientinfo *> team[2];
        float teamrank[2] = {0, 0};
        for(int round = 0, remaining = clients.length(); remaining>=0; round++)
        {
            int first = round&1, second = (round+1)&1, selected = 0;
            while(teamrank[first] <= teamrank[second])
            {
                float rank;
                clientinfo *ci = choosebestclient(rank);
                if(!ci) break;
                if(smode && smode->hidefrags()) rank = 1;
                else if(selected && rank<=0) break;
                ci->state.timeplayed = -1;
                team[first].add(ci);
                if(rank>0) teamrank[first] += rank;
                selected++;
                if(rank<=0) break;
            }
            if(!selected) break;
            remaining -= selected;
        }
        loopi(sizeof(team)/sizeof(team[0]))
        {
            addteaminfo(teamnames[i]);
            loopvj(team[i])
            {
                clientinfo *ci = team[i][j];
                if(!strcmp(ci->team, teamnames[i])) continue;
                if(persistteams && ci->team[0] && (!smode || smode->canchangeteam(ci, teamnames[i], ci->team)))
                {
                    addteaminfo(ci->team);
                    continue;
                }
                copystring(ci->team, teamnames[i], MAXTEAMLEN+1);
                sendf(-1, 1, "riisi", N_SETTEAM, ci->clientnum, teamnames[i], -1);
            }
        }
    }
#endif

    struct teamrank
    {
        const char *name;
        float rank;
        int clients;

        teamrank(const char *name) : name(name), rank(0), clients(0) {}
    };

    const char *chooseworstteam(const char *suggest = NULL, clientinfo *exclude = NULL)
    {
        teamrank teamranks[2] = { teamrank("good"), teamrank("evil") };
        const int numteams = sizeof(teamranks)/sizeof(teamranks[0]);
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci==exclude || ci->state.aitype!=AI_NONE || ci->state.state==CS_SPECTATOR || !ci->team[0]) continue;
            ci->state.timeplayed += lastmillis - ci->state.lasttimeplayed;
            ci->state.lasttimeplayed = lastmillis;

            loopj(numteams) if(!strcmp(ci->team, teamranks[j].name))
            {
                teamrank &ts = teamranks[j];
                ts.rank += ci->state.effectiveness/max(ci->state.timeplayed, 1);
                ts.clients++;
                break;
            }
        }
        teamrank *worst = &teamranks[numteams-1];
        loopi(numteams-1)
        {
            teamrank &ts = teamranks[i];
            if(smode && smode->hidefrags())
            {
                if(ts.clients < worst->clients || (ts.clients == worst->clients && ts.rank < worst->rank)) worst = &ts;
            }
            else if(ts.rank < worst->rank || (ts.rank == worst->rank && ts.clients < worst->clients)) worst = &ts;
        }
        return worst->name;
    }

    void prunedemos(int extra = 0)
    {
        int n = clamp(demos.length() + extra - maxdemos, 0, demos.length());
        if(n <= 0) return;
        loopi(n) delete[] demos[i].data;
        demos.remove(0, n);
    }
 
    void adddemo()
    {
        if(!demotmp) return;
        int len = (int)min(demotmp->size(), stream::offset((maxdemosize<<20) + 0x10000));
        demofile &d = demos.add();
        time_t t = time(NULL);
        string timestr;
        z_formatdate(timestr, sizeof(timestr), t);
        formatstring(d.info, "%s: %s, %s, %.2f%s", timestr, modename(gamemode), smapname, len > 1024*1024 ? len/(1024*1024.f) : len/1024.0f, len > 1024*1024 ? "MB" : "kB");
        sendservmsgf("demo \"%s\" recorded", d.info);
        d.data = new uchar[len];
        d.len = len;
        demotmp->seek(0, SEEK_SET);
        demotmp->read(d.data, len);
        DELETEP(demotmp);
    }
        
    void enddemorecord()
    {
        if(!demorecord) return;

        DELETEP(demorecord);

        if(!demotmp) return;
        if(!maxdemos || !maxdemosize) { DELETEP(demotmp); return; }

        prunedemos(1);
        adddemo();
    }

    void writedemo(int chan, void *data, int len)
    {
        if(!demorecord) return;
        int stamp[3] = { gamemillis, chan, len };
        lilswap(stamp, 3);
        demorecord->write(stamp, sizeof(stamp));
        demorecord->write(data, len);
        if(demorecord->rawtell() >= (maxdemosize<<20)) enddemorecord();
    }

    void recordpacket(int chan, void *data, int len)
    {
        writedemo(chan, data, len);
    }

    int welcomepacket(packetbuf &p, clientinfo *ci);
    void sendwelcome(clientinfo *ci);

    void setupdemorecord()
    {
        if(!m_recordable) return;

        demotmp = opentempfile("demorecord", "w+b");
        if(!demotmp) return;

        stream *f = opengzfile(NULL, "wb", demotmp);
        if(!f) { DELETEP(demotmp); return; }

        sendservmsg("recording demo");

        demorecord = f;

        demoheader hdr;
        memcpy(hdr.magic, DEMO_MAGIC, sizeof(hdr.magic));
        hdr.version = DEMO_VERSION;
        hdr.protocol = PROTOCOL_VERSION;
        lilswap(&hdr.version, 2);
        demorecord->write(&hdr, sizeof(demoheader));

        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        welcomepacket(p, NULL);
        writedemo(1, p.buf, p.len);
    }

    void listdemos(int cn)
    {
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putint(p, N_SENDDEMOLIST);
        putint(p, demos.length());
        loopv(demos) sendstring(demos[i].info, p);
        sendpacket(cn, 1, p.finalize());
    }

    void cleardemos(int n)
    {
        if(!n)
        {
            loopv(demos) delete[] demos[i].data;
            demos.shrink(0);
            sendservmsg("cleared all demos");
        }
        else if(demos.inrange(n-1))
        {
            delete[] demos[n-1].data;
            demos.remove(n-1);
            sendservmsgf("cleared demo %d", n);
        }
    }

    static void freegetmap(ENetPacket *packet)
    {
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->getmap == packet) ci->xi.maploaded = 0;
            if(ci->getmap == packet) ci->getmap = NULL;
        }
    }

    static void freegetdemo(ENetPacket *packet)
    {
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->getdemo == packet) ci->getdemo = NULL;
        }
    }

    void senddemo(clientinfo *ci, int num, int tag)
    {
        if(ci->getdemo) return;
        if(!num) num = demos.length();
        if(!demos.inrange(num-1)) return;
        demofile &d = demos[num-1];
#ifndef OLDPROTO
        if((ci->getdemo = sendf(ci->clientnum, 2, "riim", N_SENDDEMO, tag, d.len, d.data)))
            ci->getdemo->freeCallback = freegetdemo;
#else
        if((ci->getdemo = sendf(ci->clientnum, 2, "rim", N_SENDDEMO, d.len, d.data)))
            ci->getdemo->freeCallback = freegetdemo;
#endif
    }

    void enddemoplayback()
    {
        if(!demoplayback) return;
        DELETEP(demoplayback);

        loopv(clients) sendf(clients[i]->clientnum, 1, "ri3", N_DEMOPLAYBACK, 0, clients[i]->clientnum);

        sendservmsg("demo playback finished");

        loopv(clients) sendwelcome(clients[i]);
    }

    SVARP(demodir, "demo");

    const char *getdemofile(const char *file, bool init)
    {
        if(!demodir[0]) return NULL;
        static string buf;
        copystring(buf, demodir);
        int dirlen = strlen(buf);
        if(buf[dirlen] != '/' && buf[dirlen] != '\\' && dirlen+1 < (int)sizeof(buf)) { buf[dirlen++] = '/'; buf[dirlen] = '\0'; }
        if(init)
        {
            const char *dir = findfile(buf, "w");
            if(!fileexists(dir, "w")) createdir(dir);
        }
        concatstring(buf, file);
        return buf;
    }

    void setupdemoplayback()
    {
        if(demoplayback) return;
        demoheader hdr;
        string msg;
        msg[0] = '\0';
        string file;
        copystring(file, smapname);
        int len = strlen(file);
        if(len < 4 || strcasecmp(&file[len-4], ".dmo")) concatstring(file, ".dmo");
        if(const char *buf = getdemofile(file, false)) demoplayback = opengzfile(buf, "rb");
        if(!demoplayback) demoplayback = opengzfile(file, "rb");
        if(!demoplayback) formatstring(msg, "could not read demo \"%s\"", file);
        else if(demoplayback->read(&hdr, sizeof(demoheader))!=sizeof(demoheader) || memcmp(hdr.magic, DEMO_MAGIC, sizeof(hdr.magic)))
            formatstring(msg, "\"%s\" is not a demo file", file);
        else
        {
            lilswap(&hdr.version, 2);
            if(hdr.version!=DEMO_VERSION) formatstring(msg, "demo \"%s\" requires an %s version of Cube 2: Sauerbraten", file, hdr.version<DEMO_VERSION ? "older" : "newer");
            else if(hdr.protocol!=PROTOCOL_VERSION) formatstring(msg, "demo \"%s\" requires an %s version of Cube 2: Sauerbraten", file, hdr.protocol<PROTOCOL_VERSION ? "older" : "newer");
        }
        if(msg[0])
        {
            DELETEP(demoplayback);
            sendservmsg(msg);
            return;
        }

        sendservmsgf("playing demo \"%s\"", file);

        sendf(-1, 1, "ri3", N_DEMOPLAYBACK, 1, -1);

        if(demoplayback->read(&nextplayback, sizeof(nextplayback))!=sizeof(nextplayback))
        {
            enddemoplayback();
            return;
        }
        lilswap(&nextplayback, 1);
    }

    void readdemo()
    {
        if(!demoplayback) return;
        while(gamemillis>=nextplayback)
        {
            int chan, len;
            if(demoplayback->read(&chan, sizeof(chan))!=sizeof(chan) ||
               demoplayback->read(&len, sizeof(len))!=sizeof(len))
            {
                enddemoplayback();
                return;
            }
            lilswap(&chan, 1);
            lilswap(&len, 1);
            ENetPacket *packet = enet_packet_create(NULL, len+1, 0);
            if(!packet || demoplayback->read(packet->data+1, len)!=size_t(len))
            {
                if(packet) enet_packet_destroy(packet);
                enddemoplayback();
                return;
            }
            packet->data[0] = N_DEMOPACKET;
            sendpacket(-1, chan, packet);
            if(!packet->referenceCount) enet_packet_destroy(packet);
            if(!demoplayback) break;
            if(demoplayback->read(&nextplayback, sizeof(nextplayback))!=sizeof(nextplayback))
            {
                enddemoplayback();
                return;
            }
            lilswap(&nextplayback, 1);
        }
    }

    void timeupdate(int secs)
    {
        if(!demoplayback) return;
        if(secs <= 0) interm = -1;
        else gamelimit = max(gamelimit, nextplayback + secs*1000);
    }

    void seekdemo(char *t)
    {
        if(!demoplayback) return;
        bool rev = *t == '-';
        if(rev) t++;
        int mins = strtoul(t, &t, 10), secs = 0, millis = 0;
        if(*t == ':') secs = strtoul(t+1, &t, 10);
        else { secs = mins; mins = 0; }
        if(*t == '.') millis = strtoul(t+1, &t, 10);
        int offset = max(millis + (mins*60 + secs)*1000, 0), prevmillis = gamemillis;
        if(rev) while(gamelimit - offset > gamemillis)
        {
            gamemillis = gamelimit - offset;
            readdemo();
        }
        else if(offset > gamemillis)
        {
            gamemillis = offset;
            readdemo();
        }
        if(gamemillis > prevmillis)
        {
            if(!interm) sendf(-1, 1, "ri2", N_TIMEUP, max((gamelimit - gamemillis)/1000, 1));
#ifndef STANDALONE
            cleardamagescreen();
#endif
        }
    }

    ICOMMAND(seekdemo, "sN$", (char *t, int *numargs, ident *id),
    {
        if(*numargs > 0) seekdemo(t);
        else
        {
            int secs = gamemillis/1000;
            defformatstring(str, "%d:%02d.%03d", secs/60, secs%60, gamemillis%1000);
            if(*numargs < 0) result(str);
            else printsvar(id, str);
        }
    });

    void stopdemo()
    {
        if(m_demo) enddemoplayback();
        else enddemorecord();
    }

    void pausegame(bool val, clientinfo *ci = NULL)
    {
        if(gamepaused==val) return;
        gamepaused = val;
        sendf(-1, 1, "riii", N_PAUSEGAME, gamepaused ? 1 : 0, ci ? ci->clientnum : -1);
    }

    void checkpausegame()
    {
        if(!gamepaused || (smode && smode->holdpause())) return;
        int admins = 0;
        loopv(clients) if(clients[i]->privilege >= (restrictpausegame ? PRIV_ADMIN : PRIV_MASTER) || clients[i]->local) admins++;
        if(!admins) pausegame(false);
    }

    void forcepaused(bool paused)
    {
        pausegame(paused);
    }

    bool ispaused() { return gamepaused; }

    void changegamespeed(int val, clientinfo *ci = NULL)
    {
        val = clamp(val, 10, 1000);
        if(gamespeed==val) return;
        gamespeed = val;
        sendf(-1, 1, "riii", N_GAMESPEED, gamespeed, ci ? ci->clientnum : -1);
    }

    void forcegamespeed(int speed)
    {
        changegamespeed(speed);
    }

    int scaletime(int t) { return t*gamespeed; }

    SVAR(serverauth, "");

    struct userkey
    {
        char *name;
        char *desc;
        
        userkey() : name(NULL), desc(NULL) {}
        userkey(char *name, char *desc) : name(name), desc(desc) {}
    };

    static inline uint hthash(const userkey &k) { return ::hthash(k.name); }
    static inline bool htcmp(const userkey &x, const userkey &y) { return !strcmp(x.name, y.name) && !strcmp(x.desc, y.desc); }

    struct userinfo : userkey
    {
        void *pubkey;
        int privilege;

        userinfo() : pubkey(NULL), privilege(PRIV_NONE) {}
        ~userinfo() { delete[] name; delete[] desc; if(pubkey) freepubkey(pubkey); }
    };
    hashset<userinfo> users;

    void adduser(char *name, char *desc, char *pubkey, char *priv)
    {
        userkey key(name, desc);
        userinfo &u = users[key];
        if(u.pubkey) { freepubkey(u.pubkey); u.pubkey = NULL; }
        if(!u.name) u.name = newstring(name);
        if(!u.desc) u.desc = newstring(desc);
        u.pubkey = parsepubkey(pubkey);
        switch(priv[0])
        {
            case 'a': case 'A': u.privilege = PRIV_ADMIN; break;
            case 'm': case 'M': default: u.privilege = PRIV_AUTH; break;
            case 'n': case 'N': u.privilege = PRIV_NONE; break;
        }
    }
    COMMAND(adduser, "ssss");

    void clearusers()
    {
        users.clear();
    }
    COMMAND(clearusers, "");

    void hashpassword(int cn, int sessionid, const char *pwd, char *result, int maxlen)
    {
        char buf[2*sizeof(string)];
        formatstring(buf, "%d %d ", cn, sessionid);
        concatstring(buf, pwd, sizeof(buf));
        if(!hashstring(buf, result, maxlen)) *result = '\0';
    }

    bool checkpassword(clientinfo *ci, const char *wanted, const char *given)
    {
        string hash;
        hashpassword(ci->clientnum, ci->sessionid, wanted, hash, sizeof(hash));
        return !strcmp(hash, given);
    }

    void revokemaster(clientinfo *ci)
    {
        ci->privilege = PRIV_NONE;
        if(ci->state.state==CS_SPECTATOR && !ci->local) aiman::removeai(ci);
    }

    extern void connected(clientinfo *ci);

    #include "z_log.h"
    #include "z_invpriv.h"
    #include "z_setmaster_override.h"
#if 0
    bool setmaster(clientinfo *ci, bool val, const char *pass = "", const char *authname = NULL, const char *authdesc = NULL, int authpriv = PRIV_MASTER, bool force = false, bool trial = false)
    {
        if(authname && !val) return false;
        const char *name = "";
        if(val)
        {
            bool haspass = adminpass[0] && checkpassword(ci, adminpass, pass);
            int wantpriv = ci->local || haspass ? PRIV_ADMIN : authpriv;
            if(wantpriv <= ci->privilege) return true;
            else if(wantpriv <= PRIV_MASTER && !force)
            {
                if(ci->state.state==CS_SPECTATOR) 
                {
                    sendf(ci->clientnum, 1, "ris", N_SERVMSG, "Spectators may not claim master.");
                    return false;
                }
                loopv(clients) if(ci!=clients[i] && clients[i]->privilege)
                {
                    sendf(ci->clientnum, 1, "ris", N_SERVMSG, "Master is already claimed.");
                    return false;
                }
                if(!authname && !(mastermask&MM_AUTOAPPROVE) && !ci->privilege && !ci->local)
                {
                    sendf(ci->clientnum, 1, "ris", N_SERVMSG, "This server requires you to use the \"/auth\" command to claim master.");
                    return false;
                }
            }
            if(trial) return true;
            ci->privilege = wantpriv;
            name = privname(ci->privilege);
        }
        else
        {
            if(!ci->privilege) return false;
            if(trial) return true;
            name = privname(ci->privilege);
            revokemaster(ci);
        }
        bool hasmaster = false;
        loopv(clients) if(clients[i]->local || clients[i]->privilege >= PRIV_MASTER) hasmaster = true;
        if(!hasmaster)
        {
            mastermode = MM_OPEN;
            allowedips.shrink(0);
        }
        string msg;
        if(val && authname) 
        {
            if(authdesc && authdesc[0]) formatstring(msg, "%s claimed %s as '\fs\f5%s\fr' [\fs\f0%s\fr]", colorname(ci), name, authname, authdesc);
            else formatstring(msg, "%s claimed %s as '\fs\f5%s\fr'", colorname(ci), name, authname);
        } 
        else formatstring(msg, "%s %s %s", colorname(ci), val ? "claimed" : "relinquished", name);
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putint(p, N_SERVMSG);
        sendstring(msg, p);
        putint(p, N_CURRENTMASTER);
        putint(p, mastermode);
        loopv(clients) if(clients[i]->privilege >= PRIV_MASTER)
        {
            putint(p, clients[i]->clientnum);
            putint(p, clients[i]->privilege);
        }
        putint(p, -1);
        sendpacket(-1, 1, p.finalize());
        checkpausegame();
        return true;
    }
#endif

    bool trykick(clientinfo *ci, int victim, const char *reason = NULL, const char *authname = NULL, const char *authdesc = NULL, int authpriv = PRIV_NONE, bool trial = false)
    {
        int priv = ci->privilege;
        if(authname)
        {
            if(priv >= authpriv || ci->local) authname = authdesc = NULL;
            else priv = authpriv;
        }
        if((priv || ci->local) && ci->clientnum!=victim)
        {
            clientinfo *vinfo = (clientinfo *)getclientinfo(victim);
            if(vinfo && vinfo->connected && (priv >= vinfo->privilege || ci->local) && vinfo->privilege < PRIV_ADMIN && !vinfo->local)
            {
                if(trial) return true;
                string kicker;
                if(authname)
                {
                    if(authdesc && authdesc[0]) formatstring(kicker, "%s as '\fs\f5%s\fr' [\fs\f0%s\fr]", colorname(ci), authname, authdesc);
                    else formatstring(kicker, "%s as '\fs\f5%s\fr'", colorname(ci), authname);
                }
                else copystring(kicker, colorname(ci));
                z_showkick(kicker, ci, vinfo, reason);
                z_log_kick(ci, authname, authdesc, priv, vinfo, reason);
                uint ip = getclientip(victim);
                addban(ip, 4*60*60000, BAN_KICK, reason);
                kickclients(ip, ci, priv);
                z_log_kickdone();
            }
        }
        return false;
    }

    savedscore *findscore(clientinfo *ci, bool insert)
    {
        uint ip = getclientip(ci->clientnum);
        if(!ip && !ci->local) return 0;
        if(!insert)
        {
            loopv(clients)
            {
                clientinfo *oi = clients[i];
                if(oi->clientnum != ci->clientnum && getclientip(oi->clientnum) == ip && !strcmp(oi->name, ci->name))
                {
                    oi->state.timeplayed += lastmillis - oi->state.lasttimeplayed;
                    oi->state.lasttimeplayed = lastmillis;
                    static savedscore curscore;
                    curscore.save(oi->state);
                    return &curscore;
                }
            }
        }
        loopv(scores)
        {
            savedscore &sc = scores[i];
            if(sc.ip == ip && !strcmp(sc.name, ci->name)) return &sc;
        }
        if(!insert) return 0;
        savedscore &sc = scores.add();
        sc.ip = ip;
        copystring(sc.name, ci->name);
        return &sc;
    }

    void savescore(clientinfo *ci)
    {
        savedscore *sc = findscore(ci, true);
        if(sc) sc->save(ci->state);
    }

    static struct msgfilter
    {
        uchar msgmask[NUMMSG];

        msgfilter(int msg, ...)
        {
            memset(msgmask, 0, sizeof(msgmask));
            va_list msgs;
            va_start(msgs, msg);
            for(uchar val = 1; msg < NUMMSG; msg = va_arg(msgs, int))
            {
                if(msg < 0) val = uchar(-msg);
                else msgmask[msg] = val;
            }
            va_end(msgs);
        }

        uchar operator[](int msg) const { return msg >= 0 && msg < NUMMSG ? msgmask[msg] : 0; }
    } msgfilter(-1, N_SERVINFO, N_INITCLIENT, N_WELCOME, N_MAPCHANGE,
                N_SERVMSG, N_DAMAGE, N_HITPUSH, N_SHOTFX, N_EXPLODEFX, N_DIED,
                N_SPAWNSTATE, N_FORCEDEATH, N_TEAMINFO, N_ITEMACC, N_ITEMSPAWN,
                N_TIMEUP, N_CDIS, N_CURRENTMASTER, N_PONG, N_RESUME,
                N_BASESCORE, N_BASEINFO, N_BASEREGEN, N_ANNOUNCE,
                N_SENDDEMOLIST, N_SENDDEMO, N_DEMOPLAYBACK, N_SENDMAP,
                N_DROPFLAG, N_SCOREFLAG, N_RETURNFLAG, N_RESETFLAG, N_INVISFLAG,
                N_CLIENT, N_AUTHCHAL, N_INITAI,
                N_EXPIRETOKENS, N_DROPTOKENS, N_STEALTOKENS, N_DEMOPACKET,
                -2, N_REMIP, N_NEWMAP, N_GETMAP, N_SENDMAP, N_CLIPBOARD,
                -3, N_EDITENT, N_EDITF, N_EDITT, N_EDITM, N_FLIP, N_COPY, N_PASTE,
                N_ROTATE, N_REPLACE, N_DELCUBE, N_EDITVAR,
#ifndef OLDPROTO
                N_EDITVSLOT, N_UNDO, N_REDO,
#endif
                -4, N_CONNECT, N_POS,
                -5, N_TELEPORT, N_JUMPPAD, N_ITEMPICKUP,
                NUMMSG),
      connectfilter(-1, N_CONNECT, -2, N_AUTHANS, -3, N_PING, -4, N_AUTHTRY, NUMMSG);

    int checktype(int type, clientinfo *ci)
    {
        if(ci)
        {
            if(!ci->connected) switch(connectfilter[type])
            {
                // allow only before authconnect
                case 1: return !ci->connectauth ? type : -1;
                // allow only during authconnect
                case 2: return ci->connectauth ? type : -1;
                // always allow
                case 3: return type;
                // allow only before server intiated authconnect
                case 4: return ci->connectauth && !ci->authreq ? type : -1;
                // never allow
                default: return -1;
            }
            if(ci->local) return type;
        }
        switch(msgfilter[type])
        {
            // server-only messages
            case 1: return ci ? -1 : type;
            // only allowed in coop-edit
            case 2: if(m_edit) break; return -1;
            // only allowed in coop-edit, no overflow check
            case 3: return m_edit ? type : -1;
            // no overflow check
            case 4: return type;
            // no overflow check only in coop-edit
            case 5: if(!m_edit) break; return type;
        }
        if(ci && ++ci->overflow >= 200) return -2;
        return type;
    }

    struct worldstate
    {
        int uses, len;
        uchar *data;

        worldstate() : uses(0), len(0), data(NULL) {}

        void setup(int n) { len = n; data = new uchar[n]; }
        void cleanup() { DELETEA(data); len = 0; }
        bool contains(const uchar *p) const { return p >= data && p < &data[len]; }
    };
    vector<worldstate> worldstates;
    bool reliablemessages = false;

    void cleanworldstate(ENetPacket *packet)
    {
        loopv(worldstates)
        {
            worldstate &ws = worldstates[i];
            if(!ws.contains(packet->data)) continue;
            ws.uses--;
            if(ws.uses <= 0)
            {
                ws.cleanup();
                worldstates.removeunordered(i);
            }
            break;
        }
    }

    void flushclientposition(clientinfo &ci)
    {
        if(ci.position.empty() || (!hasnonlocalclients() && !demorecord)) return;
        packetbuf p(ci.position.length(), 0);
        p.put(ci.position.getbuf(), ci.position.length());
        ci.position.setsize(0);
        sendpacket(-1, 0, p.finalize(), ci.ownernum);
    }

    static void sendpositions(worldstate &ws, ucharbuf &wsbuf)
    {
        if(wsbuf.empty()) return;
        int wslen = wsbuf.length();
        recordpacket(0, wsbuf.buf, wslen);
        wsbuf.put(wsbuf.buf, wslen);
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            if(ci.state.aitype != AI_NONE) continue;
            uchar *data = wsbuf.buf;
            int size = wslen;
            if(ci.wsdata >= wsbuf.buf) { data = ci.wsdata + ci.wslen; size -= ci.wslen; }
            if(size <= 0) continue;
            ENetPacket *packet = enet_packet_create(data, size, ENET_PACKET_FLAG_NO_ALLOCATE);
            sendpacket(ci.clientnum, 0, packet);
            if(packet->referenceCount) { ws.uses++; packet->freeCallback = cleanworldstate; }
            else enet_packet_destroy(packet);
        }
        wsbuf.offset(wsbuf.length());
    }

    static inline void addposition(worldstate &ws, ucharbuf &wsbuf, int mtu, clientinfo &bi, clientinfo &ci)
    {
        if(bi.position.empty()) return;
        if(wsbuf.length() + bi.position.length() > mtu) sendpositions(ws, wsbuf);
        int offset = wsbuf.length();
        wsbuf.put(bi.position.getbuf(), bi.position.length());
        bi.position.setsize(0);
        int len = wsbuf.length() - offset;
        if(ci.wsdata < wsbuf.buf) { ci.wsdata = &wsbuf.buf[offset]; ci.wslen = len; }
        else ci.wslen += len;
    }

    static void sendmessages(worldstate &ws, ucharbuf &wsbuf)
    {
        if(wsbuf.empty()) return;
        int wslen = wsbuf.length();
        recordpacket(1, wsbuf.buf, wslen);
        wsbuf.put(wsbuf.buf, wslen);
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            if(ci.state.aitype != AI_NONE) continue;
            uchar *data = wsbuf.buf;
            int size = wslen;
            if(ci.wsdata >= wsbuf.buf) { data = ci.wsdata + ci.wslen; size -= ci.wslen; }
            if(size <= 0) continue;
            ENetPacket *packet = enet_packet_create(data, size, (reliablemessages ? ENET_PACKET_FLAG_RELIABLE : 0) | ENET_PACKET_FLAG_NO_ALLOCATE);
            sendpacket(ci.clientnum, 1, packet);
            if(packet->referenceCount) { ws.uses++; packet->freeCallback = cleanworldstate; }
            else enet_packet_destroy(packet);
        }
        wsbuf.offset(wsbuf.length());
    }

    static inline void addmessages(worldstate &ws, ucharbuf &wsbuf, int mtu, clientinfo &bi, clientinfo &ci)
    {
        if(bi.messages.empty()) return;
        if(wsbuf.length() + 10 + bi.messages.length() > mtu) sendmessages(ws, wsbuf);
        int offset = wsbuf.length();
        putint(wsbuf, N_CLIENT);
        putint(wsbuf, bi.clientnum);
        putuint(wsbuf, bi.messages.length());
        wsbuf.put(bi.messages.getbuf(), bi.messages.length());
        bi.messages.setsize(0);
        int len = wsbuf.length() - offset;
        if(ci.wsdata < wsbuf.buf) { ci.wsdata = &wsbuf.buf[offset]; ci.wslen = len; }
        else ci.wslen += len;
    }

    bool buildworldstate()
    {
        int wsmax = 0;
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            ci.overflow = 0;
            ci.xi.clearws();
            ci.wsdata = NULL;
            wsmax += ci.position.length();
            if(ci.messages.length()) wsmax += 10 + ci.messages.length();
        }
        if(wsmax <= 0)
        {
            reliablemessages = false;
            return false;
        }
        worldstate &ws = worldstates.add();
        ws.setup(2*wsmax);
        int mtu = getservermtu() - 100;
        if(mtu <= 0) mtu = ws.len;
        ucharbuf wsbuf(ws.data, ws.len);
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            if(ci.state.aitype != AI_NONE) continue;
            addposition(ws, wsbuf, mtu, ci, ci);
            loopvj(ci.bots) addposition(ws, wsbuf, mtu, *ci.bots[j], ci);
        }
        sendpositions(ws, wsbuf);
        loopv(clients)
        {
            clientinfo &ci = *clients[i];
            if(ci.state.aitype != AI_NONE) continue;
            addmessages(ws, wsbuf, mtu, ci, ci);
            loopvj(ci.bots) addmessages(ws, wsbuf, mtu, *ci.bots[j], ci);
        }
        sendmessages(ws, wsbuf);
        reliablemessages = false;
        if(ws.uses) return true;
        ws.cleanup();
        worldstates.drop();
        return false;
    }

    bool sendpackets(bool force)
    {
        if(clients.empty() || (!hasnonlocalclients() && !demorecord)) return false;
        enet_uint32 curtime = enet_time_get()-lastsend;
        if(curtime<33 && !force) return false;
        bool flush = buildworldstate();
        lastsend += curtime - (curtime%33);
        return flush;
    }

    template<class T>
    void sendstate(gamestate &gs, T &p)
    {
        putint(p, gs.lifesequence);
        putint(p, gs.health);
        putint(p, gs.maxhealth);
        putint(p, gs.armour);
        putint(p, gs.armourtype);
        putint(p, gs.gunselect);
        loopi(GUN_PISTOL-GUN_SG+1) putint(p, gs.ammo[GUN_SG+i]);
    }

    void spawnstate(clientinfo *ci)
    {
        gamestate &gs = ci->state;
        gs.spawnstate(gamemode);
        gs.lifesequence = (gs.lifesequence + 1)&0x7F;
    }

    void sendspawn(clientinfo *ci)
    {
        gamestate &gs = ci->state;
        spawnstate(ci);
        sendf(ci->ownernum, 1, "rii7v", N_SPAWNSTATE, ci->clientnum, gs.lifesequence,
            gs.health, gs.maxhealth,
            gs.armour, gs.armourtype,
            gs.gunselect, GUN_PISTOL-GUN_SG+1, &gs.ammo[GUN_SG]);
        gs.lastspawn = gamemillis;
    }

    void sendwelcome(clientinfo *ci)
    {
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        int chan = welcomepacket(p, ci);
        sendpacket(ci->clientnum, chan, p.finalize());
    }

    void putinitclient(clientinfo *ci, packetbuf &p)
    {
        if(ci->state.aitype != AI_NONE)
        {
            putint(p, N_INITAI);
            putint(p, ci->clientnum);
            putint(p, ci->ownernum);
            putint(p, ci->state.aitype);
            putint(p, ci->state.skill);
            putint(p, ci->playermodel);
            sendstring(ci->name, p);
            sendstring(ci->team, p);
        }
        else
        {
            putint(p, N_INITCLIENT);
            putint(p, ci->clientnum);
            sendstring(ci->name, p);
            sendstring(ci->team, p);
            putint(p, ci->playermodel);
        }
    }

    void welcomeinitclient(packetbuf &p, int exclude = -1)
    {
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->spy) continue;
            if(!ci->connected || ci->clientnum == exclude) continue;

            putinitclient(ci, p);
        }
    }

    bool hasmap(clientinfo *ci)
    {
        return (m_edit && (actualclientslen() > 0 || ci->local)) ||
               (smapname[0] && (!m_timed || gamemillis < gamelimit || (ci->state.state==CS_SPECTATOR && !ci->privilege && !ci->local) || numclients(ci->clientnum, true, true, true)));
    }

    int welcomepacket(packetbuf &p, clientinfo *ci)
    {
        putint(p, N_WELCOME);
        putint(p, N_MAPCHANGE);
        sendstring(smapname, p);
        putint(p, gamemode);
        putint(p, notgotitems ? 1 : 0);
        if(!ci || (m_timed && smapname[0]))
        {
            putint(p, N_TIMEUP);
            putint(p, gamemillis < gamelimit && !interm ? max((gamelimit - gamemillis)/1000, 1) : 0);
        }
        if(!notgotitems)
        {
            putint(p, N_ITEMLIST);
            loopv(sents) if(sents[i].spawned)
            {
                putint(p, i);
                putint(p, sents[i].type);
            }
            putint(p, -1);
        }
        bool hasmaster = false;
        if(mastermode != MM_OPEN)
        {
            putint(p, N_CURRENTMASTER);
            putint(p, mastermode);
            hasmaster = true;
        }
        loopv(clients) if(clients[i]->privilege >= PRIV_MASTER && z_canseemypriv(clients[i], ci))
        {
            if(!hasmaster)
            {
                putint(p, N_CURRENTMASTER);
                putint(p, mastermode);
                hasmaster = true;
            }
            putint(p, clients[i]->clientnum);
            putint(p, clients[i]->privilege);
        }
        if(hasmaster) putint(p, -1);
        if(gamepaused)
        {
            putint(p, N_PAUSEGAME);
            putint(p, 1);
            putint(p, -1);
        }
        if(gamespeed != 100)
        {
            putint(p, N_GAMESPEED);
            putint(p, gamespeed);
            putint(p, -1);
        }
        if(m_teammode)
        {
            putint(p, N_TEAMINFO);
            enumerate(teaminfos, teaminfo, t,
                if(t.frags) { sendstring(t.team, p); putint(p, t.frags); }
            );
            sendstring("", p);
        } 
        if(ci)
        {
            putint(p, N_SETTEAM);
            putint(p, ci->clientnum);
            sendstring(ci->team, p);
            putint(p, -1);
        }
        if(ci && (m_demo || m_mp(gamemode)) && ci->state.state!=CS_SPECTATOR)
        {
            if(smode && !smode->canspawn(ci, true))
            {
                ci->state.state = CS_DEAD;
                ci->state.statemillis = totalmillis;
                putint(p, N_FORCEDEATH);
                putint(p, ci->clientnum);
                sendf(-1, 1, "ri2x", N_FORCEDEATH, ci->clientnum, ci->clientnum);
            }
            else
            {
                gamestate &gs = ci->state;
                spawnstate(ci);
                putint(p, N_SPAWNSTATE);
                putint(p, ci->clientnum);
                sendstate(gs, p);
                gs.lastspawn = gamemillis;
            }
        }
        if(ci && ci->state.state==CS_SPECTATOR)
        {
            putint(p, N_SPECTATOR);
            putint(p, ci->clientnum);
            putint(p, 1);
            sendf(-1, 1, "ri3x", N_SPECTATOR, ci->clientnum, 1, ci->clientnum);
        }
        if(!ci || visibleclientslen()>1)
        {
            putint(p, N_RESUME);
            loopv(clients)
            {
                clientinfo *oi = clients[i];
                if(oi->spy) continue;
                if(ci && oi->clientnum==ci->clientnum) continue;
                putint(p, oi->clientnum);
                putint(p, oi->state.state);
                putint(p, oi->state.frags);
                putint(p, oi->state.flags);
#ifndef OLDPROTO
                putint(p, oi->state.deaths);
#endif
                putint(p, oi->state.quadmillis);
                sendstate(oi->state, p);
            }
            putint(p, -1);
            welcomeinitclient(p, ci ? ci->clientnum : -1);
        }
        if(smode) smode->initclient(ci, p, true);
        return 1;
    }

    bool restorescore(clientinfo *ci)
    {
        //if(ci->local) return false;
        savedscore *sc = findscore(ci, false);
        if(sc)
        {
            sc->restore(ci->state);
            return true;
        }
        return false;
    }

    void sendresume(clientinfo *ci)
    {
        gamestate &gs = ci->state;
#ifndef OLDPROTO
        sendf(-1, 1, "ri3i4i6vi", N_RESUME, ci->clientnum, gs.state,
            gs.frags, gs.flags, gs.deaths, gs.quadmillis,
#else
        sendf(-1, 1, "ri3i9vi", N_RESUME, ci->clientnum,
            gs.state, gs.frags, gs.flags, gs.quadmillis,
#endif
            gs.lifesequence,
            gs.health, gs.maxhealth,
            gs.armour, gs.armourtype,
            gs.gunselect, GUN_PISTOL-GUN_SG+1, &gs.ammo[GUN_SG], -1);
    }

    void sendinitclient(clientinfo *ci)
    {
        packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
        putinitclient(ci, p);
        sendpacket(-1, 1, p.finalize(), ci->clientnum);
    }

    #include "z_servcmd.h"
    #include "z_sendmap.h"
    #include "z_autosendmap.h"
    #include "z_loadmap.h"
    #include "z_records.h"
    #include "z_racemode.h"
    raceservmode racemode;

    // 0 - never load ents/crc from file
    // 1 - load if gamemode wants (aka not plain edit. racemode wants too)
    // 2 - always load
    VAR(server_load_ents, 0, 1, 2);

    void loaditems()
    {
        resetitems();
        notgotitems = true;
        if(server_load_ents == 0 || (server_load_ents == 1 && !m_wantents) || !loadents(smapname, ments, &mcrc))
            return;
        loopv(ments) if(canspawnitem(ments[i].type))
        {
            server_entity se = { NOTUSED, 0, false };
            while(sents.length()<=i) sents.add(se);
            sents[i].type = ments[i].type;
            if(m_mp(gamemode) && delayspawn(sents[i].type)) sents[i].spawntime = spawntime(sents[i].type);
            else sents[i].spawned = true;
        }
        notgotitems = false;
    }
        
    VARN(gamelimit, servergamelimit, 1, 600, 3600); // in seconds
    VAR(gamelimit_overtime, 0, 0, 1);   // use m_overtime?
    VAR(persistbots, 0, 0, 1);

    void changemap(const char *s, int mode)
    {
        stopdemo();
        pausegame(false);
        changegamespeed(100);
        if(smode) smode->cleanup();
        if(!persistbots) aiman::clearai();

        gamemode = mode;
        gamemillis = 0;
        gamelimit = ((m_overtime && gamelimit_overtime) ? 1500 : 1000)*servergamelimit;
        interm = 0;
        nextexceeded = 0;
        copystring(smapname, s);
        z_addmaptohist();
        z_resetcurrentrecord();
        loaditems();
        scores.shrink(0);
        shouldcheckteamkills = false;
        teamkills.shrink(0);
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            ci->state.timeplayed += lastmillis - ci->state.lasttimeplayed;
        }

        if(!m_mp(gamemode)) kicknonlocalclients(DISC_LOCAL);

        sendf(-1, 1, "risii", N_MAPCHANGE, smapname, gamemode, 1);

        if(m_capture) smode = &capturemode;
        else if(m_ctf) smode = &ctfmode;
        else if(m_collect) smode = &collectmode;
        else if(m_edit && z_racemode && smapname[0]) smode = &racemode;
        else smode = NULL;

        clearteaminfo();
        if(m_teammode) autoteam();

        if(m_timed && smapname[0]) sendf(-1, 1, "ri2", N_TIMEUP, gamemillis < gamelimit && !interm ? max((gamelimit - gamemillis)/1000, 1) : 0);
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            ci->mapchange();
            ci->state.lasttimeplayed = lastmillis;
            if(m_mp(gamemode) && ci->state.state!=CS_SPECTATOR) sendspawn(ci);
        }

        if(persistbots) loopv(bots) if(bots[i] && bots[i]->aireinit < 1) bots[i]->aireinit = 1;
        aiman::changemap();

        if(m_demo)
        {
            if(actualclientslen()) setupdemoplayback();
        }
        else if(demonextmatch_get())
        {
            setupdemorecord();
            demonextmatch = autorecorddemo!=0;
        }

        if(smode) smode->setup();
    }

    void rotatemap(bool next)
    {
        if(next) 
        {
            curmaprotation = findmaprotation(gamemode, smapname);
            if(curmaprotation >= 0) nextmaprotation();
            else if(smapname[0]) curmaprotation = findmaprotation(gamemode, "");
        }
        else if(!maprotations.inrange(curmaprotation) || !maprotations[curmaprotation].modes) curmaprotation = 0;
        if(maprotations.inrange(curmaprotation) && maprotations[curmaprotation].modes)
        {
            maprotation &rot = maprotations[curmaprotation];
            changemap(rot.map, rot.findmode(gamemode));
        }
        else
        {
            if(smapname[0]) changemap(smapname, gamemode);
            else changemap("", 1);
        }
    }
    
    struct votecount
    {
        char *map;
        int mode, count;
        votecount() {}
        votecount(char *s, int n) : map(s), mode(n), count(0) {}
    };

    void checkvotes(bool force = false)
    {
        vector<votecount> votes;
        int maxvotes = 0;
        loopv(clients)
        {
            clientinfo *oi = clients[i];
            if(oi->state.state==CS_SPECTATOR && !oi->privilege && !oi->local) continue;
            if(oi->state.aitype!=AI_NONE) continue;
            maxvotes++;
            if(!m_valid(oi->modevote)) continue;
            votecount *vc = NULL;
            loopvj(votes) if(!strcmp(oi->mapvote, votes[j].map) && oi->modevote==votes[j].mode)
            {
                vc = &votes[j];
                break;
            }
            if(!vc) vc = &votes.add(votecount(oi->mapvote, oi->modevote));
            vc->count++;
        }
        votecount *best = NULL;
        loopv(votes) if(!best || votes[i].count > best->count || (votes[i].count == best->count && rnd(2))) best = &votes[i];
        if(force || (best && best->count > maxvotes/2))
        {
            sendpackets(true);
            if(demorecord) enddemorecord();
            if(best && (best->count > (force ? 1 : maxvotes/2)))
            {
                sendservmsg(force ? "vote passed by default" : "vote passed by majority");
                changemap(best->map, best->mode);
            }
            else rotatemap(true);
        }
    }

    void forcemap(const char *map, int mode)
    {
        stopdemo();
        if(!map[0] && !m_check(mode, M_EDIT)) 
        {
            int idx = findmaprotation(mode, smapname);
            if(idx < 0 && smapname[0]) idx = findmaprotation(mode, "");
            if(idx < 0) return;
            map = maprotations[idx].map;
        }
        if(hasnonlocalclients()) sendservmsgf("local player forced %s on map %s", modename(mode), map[0] ? map : "[new map]");
        changemap(map, mode);
    }

    void vote(const char *map, int reqmode, int sender)
    {
        clientinfo *ci = getinfo(sender);
        if(!ci || (ci->state.state==CS_SPECTATOR && !ci->privilege && !ci->local) || (!ci->local && !m_mp(reqmode))) return;
        if(!m_valid(reqmode)) return;
        if(!map[0] && !m_check(reqmode, M_EDIT)) 
        {
            int idx = findmaprotation(reqmode, smapname);
            if(idx < 0 && smapname[0]) idx = findmaprotation(reqmode, "");
            if(idx < 0) return;
            map = maprotations[idx].map;
        }
        if(!ci->local &&
            ((restrictmapvote && ci->privilege < (restrictmapvote > 1 ? PRIV_ADMIN : PRIV_MASTER)) ||
            (lockmaprotation && ci->privilege < (lockmaprotation > 1 ? PRIV_ADMIN : PRIV_MASTER) && findmaprotation(reqmode, map) < 0)))
        {
            sendf(sender, 1, "ris", N_SERVMSG, "This server has locked the map rotation.");
            return;
        }
        copystring(ci->mapvote, map);
        ci->modevote = reqmode;
        if(ci->local || (ci->privilege && mastermode>=MM_VETO))
        {
            sendpackets(true);
            if(demorecord) enddemorecord();
            if(!ci->local || hasnonlocalclients())
                sendservmsgf("%s forced %s on map %s", colorname(ci), modename(ci->modevote), ci->mapvote[0] ? ci->mapvote : "[new map]");
            changemap(ci->mapvote, ci->modevote);
        }
        else
        {
            if(z_maproatation_onvote(ci)) return;
            sendservmsgf("%s suggests %s on map %s (select map to vote)", colorname(ci), modename(reqmode), map[0] ? map : "[new map]");
            checkvotes();
        }
    }

    #include "z_awards.h"

    VAR(serverintermission, 1, 10, 3600);


    VAR(overtime, 0, 0, 1);

    bool checkovertime()
    {
        if(!m_timed || !overtime) return false;
        const char* topteam = NULL;
        int topscore = INT_MIN;
        bool tied = false;
        if(m_teammode)
        {
            vector<teamscore> scores;
            if(smode && smode->hidefrags()) smode->getteamscores(scores);
            loopv(clients)
            {
                clientinfo *ci = clients[i];
                if(ci->state.state==CS_SPECTATOR || !ci->team[0]) continue;
                int score = 0;
                if(smode && smode->hidefrags())
                {
                    int idx = scores.htfind(ci->team);
                    if(idx >= 0) score = scores[idx].score;
                }
                else if(teaminfo *ti = teaminfos.access(ci->team)) score = ti->frags;
                if(!topteam || score > topscore) { topteam = ci->team; topscore = score; tied = false; }
                else if(score == topscore && strcmp(ci->team, topteam)) tied = true;
            }
        }
        else
        {
            loopv(clients)
            {
                clientinfo *ci = clients[i];
                if(ci->state.state==CS_SPECTATOR) continue;
                int score = ci->state.frags;
                if(score > topscore) { topscore = score; tied = false; }
                else if(score == topscore) tied = true;
            }
        }
        if(!tied) return false;
        sendservmsg("the game is tied with overtime");
        gamelimit = max(gamemillis, gamelimit) + 2*60000;
        sendf(-1, 1, "ri2", N_TIMEUP, max((gamelimit - gamemillis)/1000, 1));
        return true;
    }

    void checkintermission(bool force = false)
    {
        if(gamemillis >= gamelimit && !interm && (force || !checkovertime()))
        {
            sendf(-1, 1, "ri2", N_TIMEUP, 0);
            z_awards();
            if(smode) smode->intermission();
            changegamespeed(100);
            interm = gamemillis + serverintermission*1000;
            z_maprotation_intermission();
        }
    }

    void startintermission() { gamelimit = min(gamelimit, gamemillis); checkintermission(true); }

    #include "z_ghost.h"
    #include "z_nodamage.h"
    #include "z_announcekills.h"
    #include "z_protectteamscores.h"
    #include "z_checkpos.h"

    void dodamage(clientinfo *target, clientinfo *actor, int damage, int gun, const vec &hitpush = vec(0, 0, 0))
    {
        gamestate &ts = target->state;
        int hnd = z_hasnodamage(target, actor);
        if(hnd > 2) return;
        if(!hnd) ts.dodamage(damage);
        if(target!=actor && !isteam(target->team, actor->team)) actor->state.damage += damage;
        sendf(-1, 1, "ri6", N_DAMAGE, target->clientnum, actor->clientnum, damage, ts.armour, ts.health);
        if(target==actor) target->setpushed();
        else if(!hitpush.iszero() && hnd < 2)
        {
            ivec v(vec(hitpush).rescale(DNF));
            sendf(ts.health<=0 ? -1 : target->ownernum, 1, "ri7", N_HITPUSH, target->clientnum, gun, damage, v.x, v.y, v.z);
            target->setpushed();
        }
        if(ts.health<=0)
        {
            target->state.deaths++;
            target->state.suicides += actor==target;
            int fragvalue = smode ? smode->fragvalue(target, actor) : (target==actor || isteam(target->team, actor->team) ? -1 : 1);
            actor->state.frags += fragvalue;
            if(fragvalue>0)
            {
                int friends = 0, enemies = 0; // note: friends also includes the fragger
                if(m_teammode)
                {
                    loopv(clients) if(clients[i]->state.state != CS_SPECTATOR)
                    {
                        if(strcmp(clients[i]->team, actor->team)) enemies++;
                        else friends++;
                    }
                }
                else { friends = 1; enemies = numclients(-1, true, false)-1; }
                actor->state.effectiveness += fragvalue*friends/float(max(enemies, 1));
            }
            teaminfo *t = m_teammode ? teaminfos.access(actor->team) : NULL;
            if(t && z_acceptfragval(actor, fragvalue)) t->frags += fragvalue;
            sendf(-1, 1, "ri5", N_DIED, target->clientnum, actor->clientnum, actor->state.frags, t ? t->frags : 0);
            z_announcekill(actor, target, fragvalue);
            target->position.setsize(0);
            if(smode) smode->died(target, actor);
            ts.state = CS_DEAD;
            ts.statemillis = totalmillis;
            z_clientdied(target);
            ts.lastdeath = gamemillis;
            if(actor!=target && isteam(actor->team, target->team)) 
            {
                actor->state.teamkills++;
                addteamkill(actor, target, 1);
            }
            ts.deadflush = ts.lastdeath + DEATHMILLIS;
            // don't issue respawn yet until DEATHMILLIS has elapsed
            // ts.respawn();
        }
    }

    void suicide(clientinfo *ci)
    {
        gamestate &gs = ci->state;
        if(gs.state!=CS_ALIVE) return;
        int fragvalue = smode ? smode->fragvalue(ci, ci) : -1;
        ci->state.frags += fragvalue;
        ci->state.deaths++;
        ci->state.suicides++;
        teaminfo *t = m_teammode ? teaminfos.access(ci->team) : NULL;
        if(t && z_acceptfragval(ci, fragvalue)) t->frags += fragvalue;
        sendf(-1, 1, "ri5", N_DIED, ci->clientnum, ci->clientnum, gs.frags, t ? t->frags : 0);
        ci->position.setsize(0);
        if(smode) smode->died(ci, NULL);
        gs.state = CS_DEAD;
        gs.statemillis = totalmillis;
        z_clientdied(ci);
        gs.lastdeath = gamemillis;
        gs.respawn();
    }

    void suicideevent::process(clientinfo *ci)
    {
        if(!z_hasnodamage(ci, NULL)) suicide(ci);
    }

    void explodeevent::process(clientinfo *ci)
    {
        gamestate &gs = ci->state;
        switch(gun)
        {
            case GUN_RL:
                if(!gs.rockets.remove(id)) return;
                break;

            case GUN_GL:
                if(!gs.grenades.remove(id)) return;
                break;

            default:
                return;
        }
        if(z_shouldblockgameplay(ci)) return;
        if(ci->xi.slay) { suicide(ci); return; }
        gs.explosivedamage += guns[gun].damage * (gs.quadmillis ? 4 : 1);
        sendf(-1, 1, "ri4x", N_EXPLODEFX, ci->clientnum, gun, id, ci->ownernum);
        loopv(hits)
        {
            hitinfo &h = hits[i];
            clientinfo *target = getinfo(h.target);
            if(!target || target->state.state!=CS_ALIVE || h.lifesequence!=target->state.lifesequence || h.dist<0 || h.dist>guns[gun].exprad) continue;

            bool dup = false;
            loopj(i) if(hits[j].target==h.target) { dup = true; break; }
            if(dup) continue;

            gs.hits += (ci != target ? 1 : 0);

            int damage = guns[gun].damage;
            if(gs.quadmillis) damage *= 4;
            damage = int(damage*(1-h.dist/EXP_DISTSCALE/guns[gun].exprad));
            if(target==ci) damage /= EXP_SELFDAMDIV;
            dodamage(target, ci, damage, gun, h.dir);
        }
    }

    void shotevent::process(clientinfo *ci)
    {
        gamestate &gs = ci->state;
        int wait = millis - gs.lastshot;
        if(!gs.isalive(gamemillis) ||
           wait<gs.gunwait ||
           gun<GUN_FIST || gun>GUN_PISTOL ||
           gs.ammo[gun]<=0 || (guns[gun].range && from.dist(to) > guns[gun].range + 1))
            return;
        if(gun!=GUN_FIST) gs.ammo[gun]--;
        gs.lastshot = millis;
        gs.gunwait = guns[gun].attackdelay;
        if(z_shouldblockgameplay(ci)) return;
        if(ci->xi.slay) { suicide(ci); return; }
#ifdef OLDPROTO
        // non-moving grenades cause vanilla collect clients to shit themselves and segfault LOL
        // rip vanilla clients if we don't filter that. 0/0 operation is spooky
        if(from == to) return;
#endif
        sendf(-1, 1, "rii9x", N_SHOTFX, ci->clientnum, gun, id,
                int(from.x*DMF), int(from.y*DMF), int(from.z*DMF),
                int(to.x*DMF), int(to.y*DMF), int(to.z*DMF),
                ci->ownernum);
        gs.shotdamage += guns[gun].damage*(gs.quadmillis ? 4 : 1)*guns[gun].rays;
        gs.shots++;
        int oldhits = gs.hits;
        switch(gun)
        {
            case GUN_RL: gs.rockets.add(id); break;
            case GUN_GL: gs.grenades.add(id); break;
            default:
            {
                int totalrays = 0, maxrays = guns[gun].rays;
                loopv(hits)
                {
                    hitinfo &h = hits[i];
                    clientinfo *target = getinfo(h.target);
                    if(!target || target->state.state!=CS_ALIVE || h.lifesequence!=target->state.lifesequence || h.rays<1 || h.dist > guns[gun].range + 1) continue;

                    totalrays += h.rays;
                    if(totalrays>maxrays) continue;
                    gs.hits += (ci != target ? 1 : 0);
                    int damage = h.rays*guns[gun].damage;
                    if(gs.quadmillis) damage *= 4;
                    dodamage(target, ci, damage, gun, h.dir);
                }
                break;
            }
        }
        gs.misses += (gs.hits - oldhits == 0);
    }

    void pickupevent::process(clientinfo *ci)
    {
        gamestate &gs = ci->state;
        if(m_mp(gamemode) && !gs.isalive(gamemillis)) return;
        if(z_shouldblockgameplay(ci)) return;
        pickup(ent, ci->clientnum);
    }

    bool gameevent::flush(clientinfo *ci, int fmillis)
    {
        process(ci);
        return true;
    }

    bool timedevent::flush(clientinfo *ci, int fmillis)
    {
        if(millis > fmillis) return false;
        else if(millis >= ci->lastevent)
        {
            ci->lastevent = millis;
            process(ci);
        }
        return true;
    }

    void clearevent(clientinfo *ci)
    {
        delete ci->events.remove(0);
    }

    void flushevents(clientinfo *ci, int millis)
    {
        while(ci->events.length())
        {
            gameevent *ev = ci->events[0];
            if(ev->flush(ci, millis)) clearevent(ci);
            else break;
        }
    }

    void processevents()
    {
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(curtime>0 && ci->state.quadmillis) ci->state.quadmillis = max(ci->state.quadmillis-curtime, 0);
            flushevents(ci, gamemillis);
        }
    }

    void cleartimedevents(clientinfo *ci)
    {
        int keep = 0;
        loopv(ci->events)
        {
            if(ci->events[i]->keepable())
            {
                if(keep < i)
                {
                    for(int j = keep; j < i; j++) delete ci->events[j];
                    ci->events.remove(keep, i - keep);
                    i = keep;
                }
                keep = i+1;
                continue;
            }
        }
        while(ci->events.length() > keep) delete ci->events.pop();
        ci->timesync = false;
    }

    #include "z_gbans_override.h"

    void serverupdate()
    {
        if(shouldstep && !gamepaused)
        {
            gamemillis += curtime;

            if(m_demo) readdemo();
            else if(!m_timed || gamemillis < gamelimit)
            {
                processevents();
                if(curtime)
                {
                    loopv(sents) if(sents[i].spawntime) // spawn entities when timer reached
                    {
                        int oldtime = sents[i].spawntime;
                        sents[i].spawntime -= curtime;
                        if(sents[i].spawntime<=0)
                        {
                            sents[i].spawntime = 0;
                            sents[i].spawned = true;
                            sendf(-1, 1, "ri2", N_ITEMSPAWN, i);
                        }
                        else if(sents[i].spawntime<=10000 && oldtime>10000 && (sents[i].type==I_QUAD || sents[i].type==I_BOOST))
                        {
                            sendf(-1, 1, "ri2", N_ANNOUNCE, sents[i].type);
                        }
                    }
                }
                aiman::checkai();
                if(smode) smode->update();
            }
        }
        else if(smode) smode->pausedupdate();
        if(nextsbanscheck && nextsbanscheck-totalmillis < 0) checkexpiredsbans();

        while(bannedips.length() && bannedips[0].expire-totalmillis <= 0) bannedips.remove(0);
        loopv(connects) if(totalmillis-connects[i]->connectmillis>15000) disconnect_client(connects[i]->clientnum, DISC_TIMEOUT);

        if(nextexceeded && gamemillis > nextexceeded && (!m_timed || gamemillis < gamelimit))
        {
            nextexceeded = 0;
            loopvrev(clients) 
            {
                clientinfo &c = *clients[i];
                if(c.state.aitype != AI_NONE) continue;
                if(c.checkexceeded()) disconnect_client(c.clientnum, DISC_MSGERR);
                else c.scheduleexceeded();
            }
        }
        z_checkexceeded();

        if(shouldcheckteamkills) checkteamkills();

        if(shouldstep && !gamepaused)
        {
            if(m_timed && smapname[0] && gamemillis-curtime>0) checkintermission();
            if(interm > 0 && gamemillis>interm)
            {
                if(demorecord) enddemorecord();
                interm = -1;
                if(z_forcenextmap)
                {
                    sendpackets(true);
                    changemap(z_forcenextmap->map, z_forcenextmap->mode);
                    DELETEP(z_forcenextmap);
                }
                else checkvotes(true);
            }
        }

        z_checksleep();

        shouldstep = actualclientslen() > 0;
    }

    void forcespectator(clientinfo *ci)
    {
        if(ci->state.state==CS_ALIVE) suicide(ci);
        if(smode) smode->leavegame(ci);
        ci->state.state = CS_SPECTATOR;
        ci->state.statemillis = totalmillis;
        ci->state.timeplayed += lastmillis - ci->state.lasttimeplayed;
        if(!ci->local && (!ci->privilege || ci->warned)) aiman::removeai(ci);
        sendf(-1, 1, "ri3", N_SPECTATOR, ci->clientnum, 1);
    }

    struct crcinfo
    {
        int crc, matches;

        crcinfo() {}
        crcinfo(int crc, int matches) : crc(crc), matches(matches) {}

        static bool compare(const crcinfo &x, const crcinfo &y) { return x.matches > y.matches; }
    };

    VAR(modifiedmapspectator, 0, 1, 2);

    void checkmaps(int req = -1)
    {
        if((m_edit && z_autosendmap < 2) || !smapname[0]) return;
        vector<crcinfo> crcs;
        int total = 0, unsent = 0, invalid = 0;
        if(mcrc) crcs.add(crcinfo(mcrc, clients.length() + 1));
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.state==CS_SPECTATOR || ci->state.aitype != AI_NONE) continue;
            total++;
            if(!ci->clientmap[0])
            {
                if(ci->mapcrc < 0) invalid++;
                else if(!ci->mapcrc) unsent++;
            }
            else
            {
                crcinfo *match = NULL;
                loopvj(crcs) if(crcs[j].crc == ci->mapcrc) { match = &crcs[j]; break; }
                if(!match) crcs.add(crcinfo(ci->mapcrc, 1));
                else match->matches++;
            }
        }
        if(!mcrc && total - unsent < min(total, 4)) return;
        crcs.sort(crcinfo::compare);
        string msg;
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if((!m_edit && ci->state.state==CS_SPECTATOR) || ci->state.aitype != AI_NONE || ci->clientmap[0] || ci->mapcrc >= 0 || (req < 0 && ci->warned)) continue;
            formatstring(msg, "%s has modified map \"%s\"", colorname(ci), smapname);
            if(!m_edit || req >= 0) sendf(req, 1, "ris", N_SERVMSG, msg);
            if(req < 0) ci->warned = true;
            if(req < 0 && m_edit && !ci->xi.mapsent) z_sendmap(ci, NULL, NULL, true);
        }
        if(crcs.length() >= 2) loopv(crcs)
        {
            crcinfo &info = crcs[i];
            if(i || info.matches <= crcs[i+1].matches) loopvj(clients)
            {
                clientinfo *ci = clients[j];
                if((!m_edit && ci->state.state==CS_SPECTATOR) || ci->state.aitype != AI_NONE || !ci->clientmap[0] || ci->mapcrc != info.crc || (req < 0 && ci->warned)) continue;
                formatstring(msg, "%s has modified map \"%s\"", colorname(ci), smapname);
                if(!m_edit || req >= 0) sendf(req, 1, "ris", N_SERVMSG, msg);
                if(req < 0) ci->warned = true;
                if(req < 0 && m_edit && !ci->xi.mapsent) z_sendmap(ci, NULL, NULL, true);
            }
        }
        if(m_edit) return;
        if(req < 0 && modifiedmapspectator && (mcrc || modifiedmapspectator > 1)) loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(!ci->local && ci->warned && ci->state.state != CS_SPECTATOR) forcespectator(ci);
        }
    }

    bool shouldspectate(clientinfo *ci)
    {
        if(m_edit) return false;
        return !ci->local && ci->warned && modifiedmapspectator && (mcrc || modifiedmapspectator > 1);
    }

    void unspectate(clientinfo *ci)
    {
        if(shouldspectate(ci)) return;
        ci->state.state = CS_DEAD;
        ci->state.statemillis = totalmillis;
        ci->state.respawn();
        ci->state.lasttimeplayed = lastmillis;
        aiman::addclient(ci);
        sendf(-1, 1, "ri3", N_SPECTATOR, ci->clientnum, 0);
        if(ci->clientmap[0] || ci->mapcrc) checkmaps();
        if(!hasmap(ci)) rotatemap(true);
    }

    void sendservinfo(clientinfo *ci)
    {
        sendf(ci->clientnum, 1, "ri5ss", N_SERVINFO, ci->clientnum, PROTOCOL_VERSION, ci->sessionid, serverpass[0] ? 1 : 0, z_serverdesc(false), serverauth);
    }

    void noclients()
    {
        z_clearbans(false);
        aiman::clearai();
        z_exectrigger(Z_TRIGGER_NOCLIENTS);
    }

    void localconnect(int n)
    {
        clientinfo *ci = getinfo(n);
        ci->clientnum = ci->ownernum = n;
        ci->connectmillis = totalmillis;
        ci->sessionid = (rnd(0x1000000)*((totalmillis%10000)+1))&0xFFFFFF;
        ci->local = true;

        logoutf("connect: client %d (%s) connected", n, getclienthostname(n));

        connects.add(ci);
        sendservinfo(ci);
    }

    void localdisconnect(int n)
    {
        if(m_demo) enddemoplayback();
        clientdisconnect(n);
    }

    #include "z_geoip.h"
    #include "z_geoipserver.h"

    int clientconnect(int n, uint ip)
    {
        clientinfo *ci = getinfo(n);
        ci->clientnum = ci->ownernum = n;
        ci->connectmillis = totalmillis;
        ci->sessionid = (rnd(0x1000000)*((totalmillis%10000)+1))&0xFFFFFF;

        logoutf("connect: client %d (%s) connected", n, getclienthostname(n));

        connects.add(ci);
        if(!m_mp(gamemode)) return DISC_LOCAL;
        sendservinfo(ci);
        return DISC_NONE;
    }

    void clientdisconnect(int n, bool forced, int reason)
    {
        clientinfo *ci = getinfo(n);
        const char *msg = disconnectreason(reason);
        loopv(clients) if(clients[i]->authkickvictim == ci->clientnum) clients[i]->cleanauth(); 
        if(ci->connected)
        {
            if(ci->privilege) setmaster(ci, false);
            if(smode) smode->leavegame(ci, true);
            ci->state.timeplayed += lastmillis - ci->state.lasttimeplayed;
            savescore(ci);
            sendf(!ci->spy ? -1 : n, 1, "ri2", N_CDIS, n);
            clients.removeobj(ci);
            aiman::removeai(ci);
            if(!numclients(-1, false, true) && !numspies(-1, false, true)) noclients(); // bans clear when server empties
            loopv(clients)
            {
                clients[i]->xi.ignores.removeobj((uchar)n);
                clients[i]->xi.allows.removeobj((uchar)n);
            }
            if(ci->local) checkpausegame();
            logoutf("disconnect: %s (%d) left", ci->name, ci->clientnum);
        }
        else connects.removeobj(ci);
        z_discmsg(ci, n, msg, forced);
        if(forced)
        {
            if(msg) logoutf("disconnect: client %d (%s) disconnected by server because: %s", n, getclienthostname(n), msg);
            else logoutf("disconnect: client %d (%s) disconnected by server", n, getclienthostname(n));
        }
        else logoutf("disconnect: client %d (%s) disconnected", n, getclienthostname(n));
    }

    int reserveclients() { return 3; }

#if 0

    extern void verifybans();

    struct banlist
    {
        vector<ipmask> bans;

        void clear() { bans.shrink(0); }

        bool check(uint ip)
        {
            loopv(bans) if(bans[i].check(ip)) return true;
            return false;
        }

        void add(const char *ipname)
        {
            ipmask ban;
            ban.parse(ipname);
            bans.add(ban);

            verifybans();
        }
    } ipbans, gbans; 

    bool checkbans(uint ip)
    {
        loopv(bannedips) if(bannedips[i].ip==ip) return true;
        return ipbans.check(ip) || gbans.check(ip);
    }

    void verifybans()
    {
        loopvrev(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.aitype != AI_NONE || ci->local || ci->privilege >= PRIV_ADMIN) continue;
            if(checkbans(getclientip(ci->clientnum))) disconnect_client(ci->clientnum, DISC_IPBAN);
        }
    }

    ICOMMAND(clearipbans, "", (), ipbans.clear());
    ICOMMAND(ipban, "s", (const char *ipname), ipbans.add(ipname));

#endif
       
    int allowconnect(clientinfo *ci, const char *pwd = "")
    {
        if(ci->local) return DISC_NONE;
        if(!m_mp(gamemode)) return DISC_LOCAL;
        uint ip = getclientip(ci->clientnum);
        z_geoip_resolveclient(ci->xi.geoip, ip);
        if(serverpass[0])
        {
            if(!checkpassword(ci, serverpass, pwd)) return DISC_PASSWORD;
            return DISC_NONE;
        }
        if(adminpass[0] && checkpassword(ci, adminpass, pwd)) return DISC_NONE;
        if(numclients(-1, false, true)>=maxclients) return DISC_MAXCLIENTS;
        if(z_checkban(ip, ci)) return DISC_IPBAN;
        if(checkbans(ip, ci, true)) return DISC_IPBAN;
        if(checkgeoipban(ci)) return DISC_IPBAN;
        if(mastermode>=MM_PRIVATE && allowedips.find(ip)<0) return DISC_PRIVATE;
        return DISC_NONE;
    }

    bool allowbroadcast(int n)
    {
        clientinfo *ci = getinfo(n);
        return ci && ci->connected;
    }

    #include "z_ms_gameserver_override.h"
#if 0
    clientinfo *findauth(uint id)
    {
        loopv(clients) if(clients[i]->authreq == id) return clients[i];
        return NULL;
    }


    void authfailed(clientinfo *ci)
    {
        if(!ci) return;
        ci->cleanauth();
        if(ci->connectauth) disconnect_client(ci->clientnum, ci->connectauth);
    }

    void authfailed(uint id)
    {
        authfailed(findauth(id));
    }

    void authsucceeded(uint id)
    {
        clientinfo *ci = findauth(id);
        if(!ci) return;
        ci->cleanauth(ci->connectauth!=0);
        if(ci->connectauth) connected(ci);
        if(ci->authkickvictim >= 0)
        {
            if(setmaster(ci, true, "", ci->authname, NULL, PRIV_AUTH, false, true))
                trykick(ci, ci->authkickvictim, ci->authkickreason, ci->authname, NULL, PRIV_AUTH);    
            ci->cleanauthkick();
        }
        else setmaster(ci, true, "", ci->authname, NULL, PRIV_AUTH);
    }

    void authchallenged(uint id, const char *val, const char *desc = "")
    {
        clientinfo *ci = findauth(id);
        if(!ci) return;
        sendf(ci->clientnum, 1, "risis", N_AUTHCHAL, desc, id, val);
    }

    uint nextauthreq = 0;

    bool tryauth(clientinfo *ci, const char *user, const char *desc)
    {
        ci->cleanauth();
        if(!nextauthreq) nextauthreq = 1;
        ci->authreq = nextauthreq++;
        filtertext(ci->authname, user, false, false, 100);
        copystring(ci->authdesc, desc);
        if(ci->authdesc[0])
        {
            userinfo *u = users.access(userkey(ci->authname, ci->authdesc));
            if(u) 
            {
                uint seed[3] = { ::hthash(serverauth) + detrnd(size_t(ci) + size_t(user) + size_t(desc), 0x10000), uint(totalmillis), randomMT() };
                vector<char> buf;
                ci->authchallenge = genchallenge(u->pubkey, seed, sizeof(seed), buf);
                sendf(ci->clientnum, 1, "risis", N_AUTHCHAL, desc, ci->authreq, buf.getbuf());
            }
            else ci->cleanauth();
        }
        else if(!requestmasterf("reqauth %u %s\n", ci->authreq, ci->authname))
        {
            ci->cleanauth();
            sendf(ci->clientnum, 1, "ris", N_SERVMSG, "not connected to authentication server");
        }
        if(ci->authreq) return true;
        if(ci->connectauth) disconnect_client(ci->clientnum, ci->connectauth);
        return false;
    }

    bool answerchallenge(clientinfo *ci, uint id, char *val, const char *desc)
    {
        if(ci->authreq != id || strcmp(ci->authdesc, desc)) 
        {
            ci->cleanauth();
            return !ci->connectauth;
        }
        for(char *s = val; *s; s++)
        {
            if(!isxdigit(*s)) { *s = '\0'; break; }
        }
        if(desc[0])
        {
            if(ci->authchallenge && checkchallenge(val, ci->authchallenge))
            {
                userinfo *u = users.access(userkey(ci->authname, ci->authdesc));
                if(u) 
                {
                    if(ci->connectauth) connected(ci);
                    if(ci->authkickvictim >= 0)
                    {
                        if(setmaster(ci, true, "", ci->authname, ci->authdesc, u->privilege, false, true))
                            trykick(ci, ci->authkickvictim, ci->authkickreason, ci->authname, ci->authdesc, u->privilege);
                    }
                    else setmaster(ci, true, "", ci->authname, ci->authdesc, u->privilege);
                }
            }
            ci->cleanauth(); 
        } 
        else if(!requestmasterf("confauth %u %s\n", id, val))
        {
            ci->cleanauth();
            sendf(ci->clientnum, 1, "ris", N_SERVMSG, "not connected to authentication server");
        }
        return ci->authreq || !ci->connectauth;
    }

    void masterconnected()
    {
    }

    void masterdisconnected()
    {
        loopvrev(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->authreq) authfailed(ci); 
        }
    }

    void processmasterinput(const char *cmd, int cmdlen, const char *args)
    {
        uint id;
        string val;
        if(sscanf(cmd, "failauth %u", &id) == 1)
            authfailed(id);
        else if(sscanf(cmd, "succauth %u", &id) == 1)
            authsucceeded(id);
        else if(sscanf(cmd, "chalauth %u %255s", &id, val) == 2)
            authchallenged(id, val);
        else if(matchstring(cmd, cmdlen, "cleargbans"))
            gbans.clear();
        else if(sscanf(cmd, "addgban %100s", val) == 1)
            gbans.add(val);
    }
#endif

    #include "z_rename.h"
    #include "z_mutes.h"

    VAR(maxsendmap, 0, 4, 31);

    void receivefile(int sender, uchar *data, int len)
    {
        if(!m_edit || len <= 0) return;
        if(len > maxsendmap << 20) { sendf(sender, 1, "ris", N_SERVMSG, "map is too big"); return; }
        clientinfo *ci = getinfo(sender);
        if(ci->state.state==CS_SPECTATOR && !ci->privilege && !ci->local) return;
        if(z_iseditmuted(ci)) { sendf(sender, 1, "ris", N_SERVMSG, "your sendmap was muted"); return; }
        if(mapdata) DELETEP(mapdata);
        mapdata = opentempfile("mapdata", "w+b");
        if(!mapdata) { sendf(sender, 1, "ris", N_SERVMSG, "failed to open temporary file for map"); return; }
        if(mapdata->write(data, len) < size_t(len))
        {
            DELETEP(mapdata);
            sendf(sender, 1, "ris", N_SERVMSG, "failed to write to temporary file for map");
            return;
        }
        sendservmsgf("[%s sent a map to server, \"/getmap\" to receive it]", colorname(ci));
    }

    void sendclipboard(clientinfo *ci)
    {
        if(!ci->lastclipboard || !ci->clipboard) return;
        bool flushed = false;
        loopv(clients)
        {
            clientinfo &e = *clients[i];
            if(e.clientnum != ci->clientnum && e.needclipboard - ci->lastclipboard >= 0)
            {
                if(!flushed) { flushserver(true); flushed = true; }
                sendpacket(e.clientnum, 1, ci->clipboard);
            }
        }
    }

    void connected(clientinfo *ci)
    {
        if(m_demo) enddemoplayback();

        if(!hasmap(ci)) rotatemap(false);

        shouldstep = true;

        connects.removeobj(ci);
        clients.add(ci);

        ci->connectauth = 0;
        ci->connected = true;
        ci->needclipboard = totalmillis ? totalmillis : 1;
        if(mastermode>=MM_LOCKED || z_applyspecban(ci, true))
        {
            ci->state.state = CS_SPECTATOR;
            ci->state.statemillis = totalmillis;
        }
        ci->state.lasttimeplayed = lastmillis;

        const char *worst = m_teammode ? chooseworstteam(NULL, ci) : NULL;
        copystring(ci->team, worst ? worst : "good", MAXTEAMLEN+1);

        sendwelcome(ci);
        if(restorescore(ci)) sendresume(ci);
        sendinitclient(ci);

        aiman::addclient(ci);

        logoutf("connect: %s (%d) joined", ci->name, ci->clientnum);
        z_geoip_show(ci);

        if(m_demo) setupdemoplayback();

        if(servermotd[0]) sendf(ci->clientnum, 1, "ris", N_SERVMSG, servermotd);

        if(m_edit && z_autosendmap == 1) z_sendmap(ci, NULL);
        ci->xi.editmute = z_autoeditmute;
        ci->xi.nodamage = z_nodamage;
    }

    #include "z_msgfilter.h"
    #include "z_patchmap.h"
    #include "z_maploaded.h"
    #include "z_antiflood.h"

    VAR(serverautomaster, 0, 0, 2);

    void parsepacket(int sender, int chan, packetbuf &p)     // has to parse exactly each byte of the packet
    {
        if(sender<0 || p.packet->flags&ENET_PACKET_FLAG_UNSEQUENCED || chan > 2) return;
        char text[MAXTRANS];
        int type;
        clientinfo *ci = sender>=0 ? getinfo(sender) : NULL, *cq = ci, *cm = ci;
        if(ci && !ci->connected)
        {
            if(chan==0) return;
            else if(chan!=1) { disconnect_client(sender, DISC_MSGERR); return; }
            else while(p.length() < p.maxlen) switch(checktype(getint(p), ci))
            {
                case N_CONNECT:
                {
                    getstring(text, p);
                    filtertext(text, text, false, false, MAXNAMELEN);
                    if(!text[0]) copystring(text, "unnamed");
                    copystring(ci->name, text, MAXNAMELEN+1);
                    ci->playermodel = getint(p);

                    string password, authdesc, authname;
                    getstring(password, p, sizeof(password));
                    getstring(authdesc, p, sizeof(authdesc));
                    getstring(authname, p, sizeof(authname));
                    int disc = allowconnect(ci, password);
                    z_geoip_log(ci);
                    if(disc)
                    {
                        if(ci->xi.disc_reason) sendf(ci->clientnum, 1, "ris", N_SERVMSG, ci->xi.disc_reason);
                        // whether to allow connectauth or not
                        bool allowauth = z_allowauthconnect() || (ci->xi.wlauth && !strcmp(ci->xi.wlauth, serverauth));
                        if(disc == DISC_LOCAL || !serverauth[0] || strcmp(serverauth, authdesc) || !allowauth || !tryauth(ci, authname, authdesc))
                        {
                            // whether to send reqauth or not
                            bool trywhitelist = ci->xi.wlauth && strcmp(ci->xi.wlauth, serverauth);
                            // whether to allow authing anymore or not
                            if(disc != DISC_LOCAL && ((anyauthconnect && z_allowauthconnect()) || trywhitelist))
                            {
                                sendf(ci->clientnum, 1, "ri", N_WELCOME);   // allow client to send N_AUTHTRY
                                if(trywhitelist) sendf(ci->clientnum, 1, "ris", N_REQAUTH, ci->xi.wlauth);
                            }
                            else
                            {
                                disconnect_client(sender, disc, ci->xi.disc_reason!=NULL);
                                return;
                            }
                        }
                        ci->connectauth = disc;
                    }
                    else
                    {
                        connected(ci);
                        if(serverautomaster && actualclientslen()<=1) setmaster(ci, true, "", NULL, NULL, serverautomaster>1 ? PRIV_AUTH : PRIV_MASTER, true);
                    }
                    break;
                }

                case N_AUTHANS:
                {
                    string desc, ans;
                    getstring(desc, p, sizeof(desc));
                    uint id = (uint)getint(p);
                    getstring(ans, p, sizeof(ans));
                    if(!answerchallenge(ci, id, ans, desc)) 
                    {
                        disconnect_client(sender, ci->connectauth);
                        return;
                    }
                    break;
                }

                case N_AUTHTRY:
                {
                    string desc, name;
                    getstring(desc, p, sizeof(desc));
                    getstring(name, p, sizeof(name));
                    bool allowauth = anyauthconnect || (ci->xi.wlauth && !strcmp(ci->xi.wlauth, desc));
                    if(!allowauth || !tryauth(ci, name, desc))
                    {
                        disconnect_client(sender, ci->connectauth);
                        return;
                    }
                    break;
                }

                case N_PING:
                    getint(p);
                    break;

                default:
                    disconnect_client(sender, DISC_MSGERR);
                    return;
            }
            return;
        }
        else if(chan==2)
        {
            receivefile(sender, p.buf, p.maxlen);
            return;
        }

        if(p.packet->flags&ENET_PACKET_FLAG_RELIABLE) reliablemessages = true;
        #define QUEUE_AI clientinfo *cm = cq;
        #define QUEUE_MSG { if(cm && (!cm->local || demorecord || hasnonlocalclients())) while(curmsg<p.length()) cm->messages.add(p.buf[curmsg++]); }
        #define QUEUE_BUF(body) { \
            if(cm && (!cm->local || demorecord || hasnonlocalclients())) \
            { \
                curmsg = p.length(); \
                { body; } \
            } \
        }
        #define QUEUE_INT(n) QUEUE_BUF(putint(cm->messages, n))
        #define QUEUE_UINT(n) QUEUE_BUF(putuint(cm->messages, n))
        #define QUEUE_STR(text) QUEUE_BUF(sendstring(text, cm->messages))
        int curmsg;
        while((curmsg = p.length()) < p.maxlen) switch(type = checktype(getint(p), ci))
        {
            case N_POS:
            {
                int pcn = getuint(p); 
                p.get(); 
                uint flags = getuint(p);
                clientinfo *cp = getinfo(pcn);
                if(cp && pcn != sender && cp->ownernum != sender) cp = NULL;
                vec pos;
                loopk(3)
                {
                    int n = p.get(); n |= p.get()<<8; if(flags&(1<<k)) { n |= p.get()<<16; if(n&0x800000) n |= ~0U<<24; }
                    pos[k] = n/DMF;
                }
                loopk(3) p.get();
                int mag = p.get(); if(flags&(1<<3)) mag |= p.get()<<8;
                int dir = p.get(); dir |= p.get()<<8;
                vec vel = vec((dir%360)*RAD, (clamp(dir/360, 0, 180)-90)*RAD).mul(mag/DVELF);
                if(flags&(1<<4))
                {
                    p.get(); if(flags&(1<<5)) p.get();
                    if(flags&(1<<6)) loopk(2) p.get();
                }
                if(cp)
                {
                    if((!ci->local || demorecord || hasnonlocalclients()) && (cp->state.state==CS_ALIVE || cp->state.state==CS_EDITING))
                    {
                        if(!ci->local && !m_edit && max(vel.magnitude2(), (float)fabs(vel.z)) >= 180)
                            cp->setexceeded();
                        cp->position.setsize(0);
                        while(curmsg<p.length()) cp->position.add(p.buf[curmsg++]);
                    }
                    if(!ci->xi.maploaded && cp->state.state==CS_ALIVE && (!cp->xi.mapsent || totalmillis-cp->xi.mapsent > 1000)) z_maploaded(ci);
                    if(smode && cp->state.state==CS_ALIVE && !z_shouldhidepos(cp)) smode->moved(cp, cp->state.o, cp->gameclip, pos, (flags&0x80)!=0);
                    cp->state.o = pos;
                    cp->gameclip = (flags&0x80)!=0;
                    z_processpos(ci, cp);
                }
                break;
            }

            case N_TELEPORT:
            {
                int pcn = getint(p), teleport = getint(p), teledest = getint(p);
                clientinfo *cp = getinfo(pcn);
                if(cp && pcn != sender && cp->ownernum != sender) cp = NULL;
                if(ci && ++ci->xi.tele_overflow >= 200) break;
                if(cp && z_shouldhidepos(cp)) break;
                if(cp && (!ci->local || demorecord || hasnonlocalclients()) && (cp->state.state==CS_ALIVE || cp->state.state==CS_EDITING))
                {
                    flushclientposition(*cp);
                    sendf(-1, 0, "ri4x", N_TELEPORT, pcn, teleport, teledest, cp->ownernum); 
                }
                break;
            }

            case N_JUMPPAD:
            {
                int pcn = getint(p), jumppad = getint(p);
                clientinfo *cp = getinfo(pcn);
                if(cp && pcn != sender && cp->ownernum != sender) cp = NULL;
                if(ci && ++ci->xi.jump_overflow >= 200) break;
                if(cp && (!ci->local || demorecord || hasnonlocalclients()) && (cp->state.state==CS_ALIVE || cp->state.state==CS_EDITING))
                {
                    cp->setpushed();
                    if(z_shouldhidepos(cp)) break;
                    flushclientposition(*cp);
                    sendf(-1, 0, "ri3x", N_JUMPPAD, pcn, jumppad, cp->ownernum);
                }
                break;
            }
                
            case N_FROMAI:
            {
                int qcn = getint(p);
                if(qcn < 0) cq = ci;
                else
                {
                    cq = getinfo(qcn);
                    if(cq && qcn != sender && cq->ownernum != sender) cq = NULL;
                }
                break;
            }

            case N_EDITMODE:
            {
                int val = getint(p);
                if(!ci->local && !m_edit)
                {
                    if(ci->xi.maploaded) { disconnect_client(sender, DISC_MSGERR); return; }
                    break;
                }
                if(val ? ci->state.state!=CS_ALIVE && ci->state.state!=CS_DEAD : ci->state.state!=CS_EDITING) break;
                if(!allowmsg(ci, ci, type)) break;
                if(smode)
                {
                    if(val) smode->leavegame(ci);
                    else smode->entergame(ci);
                }
                if(val)
                {
                    ci->state.editstate = ci->state.state;
                    ci->state.state = CS_EDITING;
                    ci->state.statemillis = totalmillis;
                    ci->events.setsize(0);
                    ci->state.rockets.reset();
                    ci->state.grenades.reset();
                }
                else
                {
                    ci->state.state = ci->state.editstate;
                    ci->state.statemillis = totalmillis;
                }
                QUEUE_MSG;
                break;
            }

            case N_MAPCRC:
            {
                getstring(text, p);
                int crc = getint(p);
                if(!ci) break;
                if(strcmp(text, smapname))
                {
                    if(ci->clientmap[0])
                    {
                        ci->clientmap[0] = '\0';
                        ci->mapcrc = 0;
                    }
                    else if(ci->mapcrc > 0) ci->mapcrc = 0;
                    break;
                }
                copystring(ci->clientmap, text);
                ci->mapcrc = text[0] ? crc : 1;
                if(m_edit) ci->warned = false;  // in coop edit players can load map more than once
                checkmaps();
                z_maploaded(ci);
                if(cq && cq != ci && cq->ownernum != ci->clientnum) cq = NULL;
                break;
            }

            case N_CHECKMAPS:
                checkmaps(sender);
                break;

            case N_TRYSPAWN:
                if(!ci || !cq || cq->state.state!=CS_DEAD || cq->state.lastspawn>=0 || (smode && !smode->canspawn(cq))) break;
                if(!ci->xi.maploaded) z_maploaded(ci);
                if(!ci->clientmap[0] && !ci->mapcrc)
                {
                    ci->mapcrc = -1;
                    checkmaps();
                    if(ci == cq) { if(ci->state.state != CS_DEAD) break; }
                    else if(cq->ownernum != ci->clientnum) { cq = NULL; break; }
                }
                if(cq->state.deadflush)
                {
                    flushevents(cq, cq->state.deadflush);
                    cq->state.respawn();
                }
                cleartimedevents(cq);
                sendspawn(cq);
                break;

            case N_GUNSELECT:
            {
                int gunselect = getint(p);
                if(!cq || cq->state.state!=CS_ALIVE) break;
                cq->state.gunselect = gunselect >= GUN_FIST && gunselect <= GUN_PISTOL ? gunselect : GUN_FIST;
                if(z_shouldblockgameplay(cq)) break;
                QUEUE_AI;
                QUEUE_MSG;
                break;
            }

            case N_SPAWN:
            {
                int ls = getint(p), gunselect = getint(p);
                if(!cq || (cq->state.state!=CS_ALIVE && cq->state.state!=CS_DEAD && cq->state.state!=CS_EDITING) || ls!=cq->state.lifesequence || cq->state.lastspawn<0) break;
                cq->state.lastspawn = -1;
                cq->state.state = CS_ALIVE;
                cq->state.statemillis = totalmillis;
                cq->state.lastdeath = totalmillis ? totalmillis : 1; // reuse lastdeath to know spawntime
                cq->state.gunselect = gunselect >= GUN_FIST && gunselect <= GUN_PISTOL ? gunselect : GUN_FIST;
                cq->exceeded = 0;
                if(smode) smode->spawned(cq);
                if(!ci->xi.maploaded) z_maploaded(ci);
                QUEUE_AI;
                QUEUE_BUF({
                    putint(cm->messages, N_SPAWN);
                    sendstate(cq->state, cm->messages);
                });
                break;
            }

            case N_SUICIDE:
            {
                if(cq) cq->addevent(new suicideevent);
                break;
            }

            case N_SHOOT:
            {
                shotevent *shot = new shotevent;
                shot->id = getint(p);
                shot->millis = cq ? cq->geteventmillis(gamemillis, shot->id) : 0;
                shot->gun = getint(p);
                loopk(3) shot->from[k] = getint(p)/DMF;
                loopk(3) shot->to[k] = getint(p)/DMF;
                int hits = getint(p);
                loopk(hits)
                {
                    if(p.overread()) break;
                    hitinfo &hit = shot->hits.add();
                    hit.target = getint(p);
                    hit.lifesequence = getint(p);
                    hit.dist = getint(p)/DMF;
                    hit.rays = getint(p);
                    loopk(3) hit.dir[k] = getint(p)/DNF;
                }
                if(cq)
                {
                    cq->addevent(shot);
                    cq->setpushed();
                }
                else delete shot;
                break;
            }

            case N_EXPLODE:
            {
                explodeevent *exp = new explodeevent;
                int cmillis = getint(p);
                exp->millis = cq ? cq->geteventmillis(gamemillis, cmillis) : 0;
                exp->gun = getint(p);
                exp->id = getint(p);
                int hits = getint(p);
                loopk(hits)
                {
                    if(p.overread()) break;
                    hitinfo &hit = exp->hits.add();
                    hit.target = getint(p);
                    hit.lifesequence = getint(p);
                    hit.dist = getint(p)/DMF;
                    hit.rays = getint(p);
                    loopk(3) hit.dir[k] = getint(p)/DNF;
                }
                if(cq) cq->addevent(exp);
                else delete exp;
                break;
            }

            case N_ITEMPICKUP:
            {
                int n = getint(p);
                if(!cq) break;
                pickupevent *pickup = new pickupevent;
                pickup->ent = n;
                cq->addevent(pickup);
                break;
            }

            case N_TEXT:
            {
                getstring(text, p);
                char *tp = text;
                if(z_servcmd_check(tp)) { z_servcmd_parse(sender, tp); break; }
                Z_ANTIFLOOD(ci, N_TEXT);
                if(!allowmsg(ci, cq, type)) break;
                filtertext(text, text, true, true);
                z_maprotation_ontext(ci, tp);
                if(cq) z_log_say(cq, tp);
                if(cq && ci->spy) { sendservmsgf("\fs\f1[\f4spy\f1]\fr %s: \f0%s", cq->name, tp); break; }
                QUEUE_AI;
                QUEUE_INT(type);
                QUEUE_STR(tp);
                break;
            }

            case N_SAYTEAM:
            {
                getstring(text, p);
                if(!ci || !cq || (cq->state.state!=CS_SPECTATOR && !m_teammode) || !cq->team[0]) break;
                Z_ANTIFLOOD(ci, N_SAYTEAM);
                if(!allowmsg(ci, cq, type)) break;
                filtertext(text, text, true, true);
                if(cq->spy)
                {
                    const char *msg = tempformatstring("\fs\f1[\f4spy\f1]\fr %s: \f1%s", cq->name, text);
                    loopv(clients)
                        if(clients[i]!=cq && clients[i]->spy && clients[i]->state.aitype==AI_NONE)
                            sendf(clients[i]->clientnum, 1, "ris", N_SERVMSG, msg);
                    z_log_sayteam(cq, text, "spychat");
                    break;
                }
                if(cq->state.state==CS_SPECTATOR)
                {
                    loopv(clients)
                        if(clients[i]!=cq && clients[i]->state.state==CS_SPECTATOR && clients[i]->state.aitype==AI_NONE)
                            sendf(clients[i]->clientnum, 1, "riis", N_SAYTEAM, cq->clientnum, text);
                    z_log_sayteam(cq, text, "spectator");
                    break;
                }
                loopv(clients)
                {
                    clientinfo *t = clients[i];
                    if(t==cq || t->state.state==CS_SPECTATOR || t->state.aitype != AI_NONE || strcmp(cq->team, t->team)) continue;
                    sendf(t->clientnum, 1, "riis", N_SAYTEAM, cq->clientnum, text);
                }
                z_log_sayteam(cq, text, cq->team);
                break;
            }

            case N_SWITCHNAME:
            {
                getstring(text, p);
                Z_ANTIFLOOD(ci, N_SWITCHNAME);
                if(!allowmsg(ci, ci, type)) break;
                filtertext(text, text, false, false, MAXNAMELEN);
                if(!text[0]) copystring(text, "unnamed");
                if(!strcmp(ci->name, text)) break;
                z_log_rename(ci, text);
                copystring(ci->name, text);
                if(ci->spy) break;
                QUEUE_INT(type);
                QUEUE_STR(ci->name);
                break;
            }

            case N_SWITCHMODEL:
            {
                ci->playermodel = getint(p);
                if(ci->spy) break;
                QUEUE_MSG;
                break;
            }

            case N_SWITCHTEAM:
            {
                getstring(text, p);
                filtertext(text, text, false, false, MAXTEAMLEN);
                Z_ANTIFLOOD(ci, N_SWITCHTEAM);
                if(m_teammode && text[0] && strcmp(ci->team, text) && (!smode || smode->canchangeteam(ci, ci->team, text)) && addteaminfo(text))
                {
                    if(!allowmsg(ci, ci, type)) break;
                    if(ci->state.state==CS_ALIVE) suicide(ci);
                    copystring(ci->team, text);
                    aiman::changeteam(ci);
                    sendf(ci->spy ? sender : -1, 1, "riisi", N_SETTEAM, sender, ci->team, ci->state.state==CS_SPECTATOR ? -1 : 0);
                }
                break;
            }

            case N_MAPVOTE:
            {
                getstring(text, p);
                filtertext(text, text, false);
                fixmapname(text);
                int reqmode = getint(p);
                vote(text, reqmode, sender);
                break;
            }

            case N_ITEMLIST:
            {
                if((ci->state.state==CS_SPECTATOR && !ci->privilege && !ci->local) || !notgotitems || strcmp(ci->clientmap, smapname)) { while(getint(p)>=0 && !p.overread()) getint(p); break; }
                int n;
                while((n = getint(p))>=0 && n<MAXENTS && !p.overread())
                {
                    server_entity se = { NOTUSED, 0, false };
                    while(sents.length()<=n) sents.add(se);
                    sents[n].type = getint(p);
                    if(canspawnitem(sents[n].type))
                    {
                        if(m_mp(gamemode) && delayspawn(sents[n].type)) sents[n].spawntime = spawntime(sents[n].type);
                        else sents[n].spawned = true;
                    }
                }
                notgotitems = false;
                break;
            }

            case N_EDITENT:
            {
                int i = getint(p);
                loopk(3) getint(p);
                int type = getint(p);
                loopk(5) getint(p);
                if(!ci || ci->state.state==CS_SPECTATOR) break;
                if(!allowmsg(ci, ci, N_EDITENT)) break;
                QUEUE_MSG;
                bool canspawn = canspawnitem(type);
                if(i<MAXENTS && (sents.inrange(i) || canspawnitem(type)))
                {
                    server_entity se = { NOTUSED, 0, false };
                    while(sents.length()<=i) sents.add(se);
                    sents[i].type = type;
                    if(canspawn ? !sents[i].spawned : (sents[i].spawned || sents[i].spawntime))
                    {
                        sents[i].spawntime = canspawn ? 1 : 0;
                        sents[i].spawned = false;
                    }
                }
                break;
            }

            case N_EDITVAR:
            {
                int type = getint(p);
                getstring(text, p);
                switch(type)
                {
                    case ID_VAR: getint(p); break;
                    case ID_FVAR: getfloat(p); break;
                    case ID_SVAR: getstring(text, p);
                }
                if(ci && ci->state.state!=CS_SPECTATOR && allowmsg(ci, ci, N_EDITVAR)) QUEUE_MSG;
                break;
            }

            case N_PING:
                sendf(sender, 1, "i2", N_PONG, getint(p));
                break;

            case N_CLIENTPING:
            {
                int ping = getint(p);
                if(ci)
                {
                    ci->ping = ping;
                    loopv(ci->bots) ci->bots[i]->ping = ping;
                    if(ci->spy) break;
                }
                QUEUE_MSG;
                break;
            }

            case N_MASTERMODE:
            {
                int mm = getint(p);
                if((ci->privilege || ci->local) && mm>=MM_OPEN && mm<=MM_PRIVATE)
                {
                    if((ci->privilege>=PRIV_ADMIN || ci->local) || (mastermask&(1<<mm)))
                    {
                        mastermode = mm;
                        allowedips.shrink(0);
                        if(mm>=MM_PRIVATE)
                        {
                            loopv(clients) allowedips.add(getclientip(clients[i]->clientnum));
                        }
                        sendf(-1, 1, "rii", N_MASTERMODE, mastermode);
                        //sendservmsgf("mastermode is now %s (%d)", mastermodename(mastermode), mastermode);
                    }
                    else
                    {
                        defformatstring(s, "mastermode %d is disabled on this server", mm);
                        sendf(sender, 1, "ris", N_SERVMSG, s);
                    }
                }
                break;
            }

            case N_CLEARBANS:
            {
                if(ci->privilege || ci->local)
                {
                    bannedips.shrink(0);
                    sendservmsg("cleared all bans");
                }
                break;
            }

            case N_KICK:
            {
                int victim = getint(p);
                getstring(text, p);
                filtertext(text, text);
                trykick(ci, victim, text);
                break;
            }

            case N_SPECTATOR:
            {
                int spectator = getint(p), val = getint(p);
                if(!ci->privilege && !ci->local && (spectator!=sender || (ci->state.state==CS_SPECTATOR && (mastermode>=MM_LOCKED || ci->xi.specmute || z_applyspecban(ci))))) break;
                clientinfo *spinfo = (clientinfo *)getclientinfo(spectator); // no bots
                if(!spinfo || !spinfo->connected || (spinfo->state.state==CS_SPECTATOR ? val : !val)) break;
                if(spinfo->spy) break;

                if (spectator != sender) z_log_spectate(ci, spinfo, val);

                if(spinfo->state.state!=CS_SPECTATOR && val) forcespectator(spinfo);
                else if(spinfo->state.state==CS_SPECTATOR && !val) unspectate(spinfo);

                if(cq && cq != ci && cq->ownernum != ci->clientnum) cq = NULL;
                break;
            }

            case N_SETTEAM:
            {
                int who = getint(p);
                getstring(text, p);
                filtertext(text, text, false, false, MAXTEAMLEN);
                if(!ci->privilege && !ci->local) break;
                clientinfo *wi = getinfo(who);
                if(!m_teammode || !text[0] || !wi || !wi->connected || !strcmp(wi->team, text)) break;
                if(wi->spy) break;
                if((!smode || smode->canchangeteam(wi, wi->team, text)) && addteaminfo(text))
                {
                    z_log_setteam(ci, wi, text);
                    if(wi->state.state==CS_ALIVE) suicide(wi);
                    copystring(wi->team, text, MAXTEAMLEN+1);
                }
                aiman::changeteam(wi);
                sendf(-1, 1, "riisi", N_SETTEAM, who, wi->team, 1);
                break;
            }

            case N_FORCEINTERMISSION:
                if(ci->local && !hasnonlocalclients()) startintermission();
                break;

            case N_RECORDDEMO:
            {
                int val = getint(p);
                if(ci->privilege < (restrictdemos ? PRIV_ADMIN : PRIV_MASTER) && !ci->local) break;
                if(!maxdemos || !maxdemosize) 
                {
                    sendf(ci->clientnum, 1, "ris", N_SERVMSG, "the server has disabled demo recording");
                    break;
                }
                demonextmatch = val!=0;
                demonextmatch_set = true;
                sendservmsgf("demo recording is %s for next match", demonextmatch ? "enabled" : "disabled");
                break;
            }

            case N_STOPDEMO:
            {
                if(ci->privilege < (restrictdemos ? PRIV_ADMIN : PRIV_MASTER) && !ci->local) break;
                stopdemo();
                break;
            }

            case N_CLEARDEMOS:
            {
                int demo = getint(p);
                if(ci->privilege < (restrictdemos ? PRIV_ADMIN : PRIV_MASTER) && !ci->local) break;
                cleardemos(demo);
                break;
            }

            case N_LISTDEMOS:
                if(!ci->privilege && !ci->local && ci->state.state==CS_SPECTATOR) break;
                listdemos(sender);
                break;

            case N_GETDEMO:
            {
#ifndef OLDPROTO
                int n = getint(p), tag = getint(p);
#else
                int n = getint(p), tag = 0;
#endif
                if(!ci->privilege && !ci->local && ci->state.state==CS_SPECTATOR) break;
                senddemo(ci, n, tag);
                break;
            }

            case N_GETMAP:
                if(!mapdata) sendf(sender, 1, "ris", N_SERVMSG, "no map to send");
                else if(ci->getmap) sendf(sender, 1, "ris", N_SERVMSG, "already sending map");
                else
                {
                    if(ci->spy) sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("[%s is getting the map]", colorname(ci)));
                    else sendservmsgf("[%s is getting the map]", colorname(ci));
                    if((ci->getmap = sendfile(sender, 2, mapdata, "ri", N_SENDMAP)))
                        ci->getmap->freeCallback = freegetmap;
                    ci->needclipboard = totalmillis ? totalmillis : 1;
                    ci->xi.mapsent = ci->needclipboard;
                }
                break;

            case N_NEWMAP:
            {
                int size = getint(p);
                if(!ci->privilege && !ci->local && ci->state.state==CS_SPECTATOR) break;
                if(!allowmsg(ci, ci, type)) break;
                if(size>=0)
                {
                    smapname[0] = '\0';
                    resetitems();
                    notgotitems = false;
                    if(smode) smode->newmap();
                }
                QUEUE_MSG;
                break;
            }

            case N_SETMASTER:
            {
                int mn = getint(p), val = getint(p);
                getstring(text, p);
                if(mn != ci->clientnum)
                {
                    if(!ci->privilege && !ci->local) break;
                    clientinfo *minfo = (clientinfo *)getclientinfo(mn);
                    if(!minfo || !minfo->connected || (!ci->local && (minfo->privilege > ci->privilege || minfo->privilege >= PRIV_ADMIN))) break;
                    setmaster(minfo, val!=0, "", NULL, NULL, clamp(val, 0, ci->privilege), true, false, false, ci);
                }
                else setmaster(ci, val!=0, text);
                // don't broadcast the master password
                break;
            }

            case N_ADDBOT:
            {
                aiman::reqadd(ci, getint(p));
                break;
            }

            case N_DELBOT:
            {
                aiman::reqdel(ci);
                break;
            }

            case N_BOTLIMIT:
            {
                int limit = getint(p);
                if(ci) aiman::setbotlimit(ci, limit);
                break;
            }

            case N_BOTBALANCE:
            {
                int balance = getint(p);
                if(ci) aiman::setbotbalance(ci, balance!=0);
                break;
            }

            case N_AUTHTRY:
            {
                string desc, name;
                getstring(desc, p, sizeof(desc));
                getstring(name, p, sizeof(name));
                tryauth(ci, name, desc);
                break;
            }

            case N_AUTHKICK:
            {
                string desc, name;
                getstring(desc, p, sizeof(desc));
                getstring(name, p, sizeof(name));
                int victim = getint(p);
                getstring(text, p);
                filtertext(text, text);
                int authpriv = PRIV_AUTH;
                {
                    userinfo *u = users.access(userkey(name, desc));
                    if(u) authpriv = u->privilege;
                }
                if(ci->local || ci->privilege >= authpriv) trykick(ci, victim, text);
                else if(trykick(ci, victim, text, name, desc, authpriv, true) && tryauth(ci, name, desc))
                {
                    ci->authkickvictim = victim;
                    ci->authkickreason = newstring(text);
                } 
                break;
            }

            case N_AUTHANS:
            {
                string desc, ans;
                getstring(desc, p, sizeof(desc));
                uint id = (uint)getint(p);
                getstring(ans, p, sizeof(ans));
                answerchallenge(ci, id, ans, desc);
                break;
            }

            case N_PAUSEGAME:
            {
                int val = getint(p);
                if(ci->privilege < (restrictpausegame ? PRIV_ADMIN : PRIV_MASTER) && !ci->local) break;
                pausegame(val > 0, ci->spy ? NULL : ci);
                break;
            }

            case N_GAMESPEED:
            {
                int val = getint(p);
                if(ci->privilege < (restrictgamespeed ? PRIV_ADMIN : PRIV_MASTER) && !ci->local) break;
                changegamespeed(val, ci->spy ? NULL : ci);
                break;
            }

            case N_COPY:
                ci->cleanclipboard();
                ci->lastclipboard = totalmillis ? totalmillis : 1;
                goto genericmsg;

            case N_PASTE:
                if(ci->state.state!=CS_SPECTATOR && allowmsg(ci, ci, type)) sendclipboard(ci);
                goto genericmsg;
    
            case N_CLIPBOARD:
            {
                int unpacklen = getint(p), packlen = getint(p); 
                ci->cleanclipboard(false);
                if(ci->state.state==CS_SPECTATOR || !allowmsg(ci, ci, type))
                {
                    if(packlen > 0) p.subbuf(packlen);
                    break;
                }
                if(packlen <= 0 || packlen > (1<<16) || unpacklen <= 0) 
                {
                    if(packlen > 0) p.subbuf(packlen);
                    packlen = unpacklen = 0;
                }
                packetbuf q(32 + packlen, ENET_PACKET_FLAG_RELIABLE);
                putint(q, N_CLIPBOARD);
                putint(q, ci->clientnum);
                putint(q, unpacklen);
                putint(q, packlen); 
                if(packlen > 0) p.get(q.subbuf(packlen).buf, packlen);
                ci->clipboard = q.finalize();
                ci->clipboard->referenceCount++;
                break;
            } 

#ifndef OLDPROTO
            case N_EDITT:
            case N_REPLACE:
            case N_EDITVSLOT:
            {
                int size = server::msgsizelookup(type);
                if(size<=0) { disconnect_client(sender, DISC_MSGERR); return; }
                loopi(size-1) getint(p);
                if(p.remaining() < 2) { disconnect_client(sender, DISC_MSGERR); return; }
                int extra = lilswap(*(const ushort *)p.pad(2));
                if(p.remaining() < extra) { disconnect_client(sender, DISC_MSGERR); return; }
                p.pad(extra);
                if(ci && ci->state.state!=CS_SPECTATOR && allowmsg(ci, ci, type)) QUEUE_MSG;
                break;
            }

            case N_UNDO:
            case N_REDO:
            {
                int unpacklen = getint(p), packlen = getint(p);
                if(!ci || ci->state.state==CS_SPECTATOR || !allowmsg(ci, ci, type) || packlen <= 0 || packlen > (1<<16) || unpacklen <= 0)
                {
                    if(packlen > 0) p.subbuf(packlen);
                    break;
                }
                if(p.remaining() < packlen) { disconnect_client(sender, DISC_MSGERR); return; }
                packetbuf q(32 + packlen, ENET_PACKET_FLAG_RELIABLE);
                putint(q, type);
                putint(q, ci->clientnum);
                putint(q, unpacklen);
                putint(q, packlen);
                if(packlen > 0) p.get(q.subbuf(packlen).buf, packlen);
                sendpacket(-1, 1, q.finalize(), ci->clientnum);
                break;
            }
#endif

            case N_CONNECT:
                // can happen because we abuse N_SERVINFO
                // discard everything
                getstring(text, p); // playername
                (void) getint(p); // playermodel
                getstring(text, p); // hashed password
                getstring(text, p); // authdesc
                getstring(text, p); // authname
                break;

            case N_SOUND:
                if(z_allowsound(ci, cq, getint(p))) { QUEUE_AI; QUEUE_MSG; }
                break;

            case N_TAUNT:
                if(allowmsg(ci, cq, N_TAUNT)) { QUEUE_AI; QUEUE_MSG; }
                break;

            case N_SERVCMD:
                getstring(text, p);
                if(allowservcmd && iscubealnum(text[0])) z_servcmd_parse(sender, text);
                break;
                     
            #define PARSEMESSAGES 1
            #include "capture.h"
            #include "ctf.h"
            #include "collect.h"
            #undef PARSEMESSAGES

            case -1:
                disconnect_client(sender, DISC_MSGERR);
                return;

            case -2:
                disconnect_client(sender, DISC_OVERFLOW);
                return;

            default: genericmsg:
            {
                int size = server::msgsizelookup(type);
                if(size<=0) { disconnect_client(sender, DISC_MSGERR); return; }
                loopi(size-1) getint(p);
                if(ci) switch(msgfilter[type])
                {
                    case 2: case 3: if(ci->state.state != CS_SPECTATOR && allowmsg(ci, ci, type)) QUEUE_MSG; break;
                    default: if(cq && (ci != cq || ci->state.state!=CS_SPECTATOR)) { QUEUE_AI; QUEUE_MSG; } break;
                }
                break;
            }
        }
    }

    int laninfoport() { return SAUERBRATEN_LANINFO_PORT; }
    int serverinfoport(int servport) { return servport < 0 ? SAUERBRATEN_SERVINFO_PORT : servport+1; }
    int serverport(int infoport) { return infoport < 0 ? SAUERBRATEN_SERVER_PORT : infoport-1; }
    const char *defaultmaster() { return "master.sauerbraten.org"; }
    int masterport() { return SAUERBRATEN_MASTER_PORT; }
    int numchannels() { return 3; }

    static bool ipinfoallowed(uint ip)
    {
        if(!serverpass[0] && m_mp(gamemode) && mastermode < MM_PRIVATE) return true;
        loopv(clients) if(clients[i]->connected && clients[i]->state.aitype == AI_NONE)
        {
            if(getclientip(clients[i]->clientnum) == ip) return true;
        }
        loopv(allowedips) if(allowedips[i] == ip) return true;
        return false;
    }

    #include "extinfo.h"

    void serverinforeply(ucharbuf &req, ucharbuf &p)
    {
        if(req.remaining() && !getint(req))
        {
            extserverinforeply(req, p);
            return;
        }

        putint(p, numclients(-1, false, true));
        putint(p, gamepaused || gamespeed != 100 ? 7 : 5);                   // number of attrs following
        putint(p, PROTOCOL_VERSION);    // generic attributes, passed back below
        putint(p, gamemode);
        int timeleft = smode ? smode->timeleft() : -1; // zeromod
        putint(p, timeleft >= 0 ? timeleft : m_timed ? max((gamelimit - gamemillis)/1000, 0) : 0);
        putint(p, maxclients);
        putint(p, serverpass[0] ? MM_PASSWORD : (!m_mp(gamemode) ? MM_PRIVATE : (mastermode || mastermask&MM_AUTOAPPROVE ? mastermode : MM_AUTH)));
        if(gamepaused || gamespeed != 100)
        {
            putint(p, gamepaused ? 1 : 0);
            putint(p, gamespeed);
        }
        sendstring(smapname, p);
        sendstring(z_serverdesc(true), p);
        sendserverinforeply(p);
    }

    bool servercompatible(char *name, char *sdec, char *map, int ping, const vector<int> &attr, int np)
    {
        return attr.length() && attr[0]==PROTOCOL_VERSION;
    }

    #include "aiman.h"

    #include "z_scripting.h"
    #include "z_genericservercommands.h"
    #include "z_mapsucks.h"
    #include "z_savemap.h"
    #include "z_slay.h"
    #include "z_spy.h"
    #include "z_talkbot.h"
    #include "z_serverdesc.h"
}

