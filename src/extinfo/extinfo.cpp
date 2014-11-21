#include "cube.h"

#include "extinfo_sauer.h"

// extinfo protocol
#define EXT_ACK                         -1
#define EXT_VERSION                     105
#define EXT_NO_ERROR                    0
#define EXT_ERROR                       1
#define EXT_PLAYERSTATS_RESP_IDS        -10
#define EXT_PLAYERSTATS_RESP_STATS      -11
#define EXT_UPTIME                      0
#define EXT_PLAYERSTATS                 1
#define EXT_TEAMSCORE                   2



int totalmillis = 0;

enum { CS_ALIVE = 0, CS_DEAD, CS_SPAWNING, CS_LAGGED, CS_EDITING, CS_SPECTATOR };
static const char * const statenames[] = { "alive", "dead", "spawn", "LAG", "edit", "spectator" };
struct playerstate
{
    int ping;
    string name;
    string team;
    int frags, flags, deaths, teamkills, accuracy, health, armour;
    int gunselect, privilege, state;
    uint ip;

    playerstate() { reset(); }
    playerstate(const playerstate &s) { *this = s; }
    playerstate(ucharbuf &p) { read(p); }
    ~playerstate() {}
    
    void reset()
    {
        ping = 0;
        name[0] = '\0';
        team[0] = '\0';
        frags = flags = deaths = teamkills = accuracy = health = armour = 0;
        gunselect = privilege = state = 0;
        ip = 0;
    }
    
    playerstate &operator =(const playerstate &s)
    {
        if(&s != this)
        {
            ping = s.ping;
            copystring(name, s.name);
            copystring(team, s.team);
            frags = s.frags;
            flags = s.flags;
            deaths = s.deaths;
            teamkills = s.teamkills;
            accuracy = s.accuracy;
            health = s.health;
            armour = s.armour;
            gunselect = s.gunselect;
            privilege = s.privilege;
            state = s.state;
            ip = s.ip;
        }
        return *this;
    }
    
    int getfrags() const { return frags; }
    int getflags() const { return flags; }
    int getdeaths() const { return deaths; }
    float getkpd() const { return float(frags)/max(deaths, 1); }
    int getteamkills() const { return teamkills; }
    int getaccuracy() const { return accuracy; }
    int gethealth() const { return health; }
    int getarmour() const { return armour; }
    
    void read(ucharbuf &p)
    {
        ping = getint(p);
        getstring(name, p);
        getstring(team, p);
        frags = getint(p);
        flags = getint(p);
        deaths = getint(p);
        teamkills = getint(p);
        accuracy = getint(p);
        health = getint(p);
        armour = getint(p);
        gunselect = getint(p);
        privilege = getint(p);
        state = getint(p);
        ip = 0; get((uchar *)&ip, 3);
    }
};

struct playerinfo
{
    int clientnum;
    playerstate state;
    bool seen;
    
    playerinfo(): clientnum(-1) {}
    playerinfo(int cn): clientnum(cn) {}
};

struct teamstate
{
    string name;
    int score;
    int numbases;
    vector<int> bases;
    
    teamstate() { reset(); }
    teamstate(const teamstate &s) { *this = s; }
    teamstate(ucharbuf &p) { read(p); }
    ~teamstate() {}
    
    void reset()
    {
        name[0] = '\0';
        score = 0;
        numbases = -1;
        bases.shrink(0);
    }
    
    teamstate &operator =(const teamstate &s)
    {
        if(&s != this)
        {
            copystring(name, s.name);
            score = s.score;
            numbases = s.numbases;
            bases = s.bases;
        }
        return *this;
    }
    
    void read(ucharbuf &p)
    {
        getstring(name, p);
        score = getint(p);
        numbases = getint(p);
        bases.shrink(0);
        loopi(numbases)
        {
            int baseid = getint(p);
            if(p.overread()) break;
            bases.add(baseid);
        }
    }
};

enum { MM_AUTH = -1, MM_OPEN = 0, MM_VETO, MM_LOCKED, MM_PRIVATE, MM_PASSWORD, MM_START = MM_AUTH };
static const char * const mastermodenames[] =  { "auth",   "open",   "veto",       "locked",     "private",    "password" };
static const char * const mastermodecolors[] = { "",       "\f0",    "\f2",        "\f2",        "\f3",        "\f3" };
enum { ATTR_GAMEMODE = 0, ATTR_TIMEREMAINING, ATTR_MASTERMODE, ATTR_GAMEPAUSED, ATTR_GAMESPEED, NUMATTRS };
struct serverstate
{
    int protocol;
    int numclients, maxclients;
    int attrs[NUMATTRS];
    string mapname;
    string serverdesc;
    
    serverstate() { reset(); }
    serverstate(const serverstate &s) { *this = s; }
    serverstate(ucharbuf &p) { read(p); }
    ~serverstate() {}
    
    void reset()
    {
        protocol = 0;
        numclients = maxclients = 0;
        attrs[0] = attrs[1] = attrs[2] = attrs[3] = 0;
        attrs[4] = 100;
        mapname[0] = '\0';
        serverdesc[0] = '\0';
    }
    
    serverstate &operator =(const serverstate &s)
    {
        if(&s != this)
        {
            protocol = s.protocol;
            numclients = s.numclients;
            maxclients = s.maxclients;
            loopi(NUMATTRS) attrs[i] = s.attrs[i];
            copystring(mapname, s.mapname);
            copystring(serverdesc, s.serverdesc);
        }
        return *this;
    }
    
    void read(ucharbuf &p)
    {
        protocol = getint(p);
        numclients = getint(p);
        maxclients = getint(p);
        attrs[0] = attrs[1] = attrs[2] = attrs[3] = 0;
        attrs[4] = 100;
        int numattrs = getint(p);
        loopi(numattrs)
        {
            int a = getint(p);
            if(p.overread()) break;
            if(i < NUMATTRS) attrs[i] = a;
        }
        getstring(mapname, p);
        getstring(serverdesc, p);
    }
};

enum { GAME_SAUER = 0, GAME_TESS, NUMGAMES };
struct serverinfo
{
    ENetAddress address;
    int servertype;
    uint uptime;
    int ping;
    serverstate state;
    vector<playerinfo> players;
    vector<teamstate> teams;
};
vector<serverinfo> servers;
serverinfo *server = NULL;  // pointer to currently being processed message's server

// full - check whole packet, ping - accept any (only if ping), ext - check only part after time (extinfo request)
enum { CHK_FULL = 0, CHK_PING, CHK_EXT };
struct sentmsginfo
{
    int type;           // checking type
    int senttime;       // when packet was sent
    ENetAddress addr;   // ip and port message is sent to
    int length;         // exact length of request
    int hash;           // hash (depends on type)
};

struct outgoingmessage
{
    int type;           // checking type
    ENetAddress addr;   // ip and port message is supposed to be sent to
    packetbuf *p;       // packet itself
    int hash;           // hash (which parts are hashed depends on type)
};

vector<sentmsginfo *> sentmessages;
vector<outgoingmessage *> outgoingmessages;

// uses server pointer
playerinfo *addplayer(int cn)
{
    loopv(server->players) if(server->players[i]->clientnum == cn) return server->players[i];
    playerinfo *pi = new playerinfo(cn);
    server->players.add(pi);
    return pi;
}

uchar netbuf[MAXTRANS];
ENetAddress netaddr;
ENetSocket pingsock = ENET_SOCKET_NULL;
bool removesentmessage;

void extprocessmsg(ucharbuf &req, ucharbuf &p)
{
    int extcmd = getint(req);
    int extack = getint(p), extver = getint(p);
    if(p.overread()) return;
    if(extack != EXT_ACK || extver != EXT_VERSION) return;
    switch(extcmd)
    {
        case EXT_UPTIME:
        {
            int uptime = getint(p);
            if(p.overread()) return;
            server->uptime = uptime;
            break;
        }
        case EXT_PLAYERSTATS:
        {
            int reqcn = getint(req);
            int exterr = getint(p);
            if(p.overread()) return;
            if(exterr == EXT_ERROR)
            {
                if(reqcn >= 0) loopvrev(server->players) if(server->players[i]->clientnum == reqcn) delete server->players.remove(i);
                break;
            }
            if(exterr != EXT_NO_ERROR) return;  // something we can't understand
            int type = getint(p);
            if(p.overread()) return;
            switch(type)
            {
                case EXT_PLAYERSTATS_RESP_IDS:
                {
                    vector<int> ids;
                    while(p.remaining())
                    {
                        int id = getint(p);
                        if(p.overread()) break;
                        ids.add(id);
                    }
                    loopv(server->players) server->players[i]->seen = false;
                    loopv(ids)
                    {
                        if(ids[i] < 0 || (reqcn >= 0 && ids[i] != reqcn)) continue;
                        playerinfo *pi = addplayer(ids[i]);
                        pi->seen = true;
                    }
                    loopvrev(server->players)
                    {
                        if(server->players[i]->seen) continue;
                        if(reqcn >= 0 && server->players[i]->clientnum != reqcn) continue;
                        delete players.remove(i);
                    }
                    break;
                }
                case EXT_PLAYERSTATS_RESP_STATS:
                {
                    int cn = getint(p);
                    playerstate s(p);
                    if(p.overread()) return;
                    if(cn < 0) break;   // no negative cns plz
                    playerinfo *pi = addplayer(cn);
                    pi->state = s;
                    break;
                }
            }
        }
        case EXT_TEAMSCORE:
        {
            int notteammode = getint(p);
            int gamemode = getint(p);
            int timeleft = getint(p);
            if(p.overread()) return;
            server->teams.shrink(0);
            if(notteammode) break;  // we don't expect anything after this if its not teammode
            teamstate t;
            while(p.remaining())
            {
                t.read(p);
                if(p.overread()) break;
                server->teams.add(t);
            }
        }
    }
}

void processmsg(ucharbuf &req, ucharbuf &p)
{
    int time = 0;
    if(req.remaining() && !(time = getint(req)))
    {
        extprocessmsg(req, p);
        return;
    }
    
    serverstate s(p);
    if(p.overread()) return;
    server->state = s;
}

void hostserver(uint timeout)
{
    enet_uint32 waitcondition = ENET_SOCKET_WAIT_READ;
    if(outgoingmessages.length()) waitcondition |= ENET_SOCKET_WAIT_WRITE;
    if(enet_socket_wait(pingsock, &waitcondition, timeout) < 0) return;
    ENetBuffer buf;
    if(waitcondition & ENET_SOCKET_WAIT_READ)
    {
        buf.data = netbuf;
        buf.dataLength = sizeof(netbuf);
        int len = enet_socket_receive(pingsock, &netaddr, &buf, 1);
        if(len >= 0)
        {
            removesentmessage = false;
            loopv(sentmessages)
            {
                if(sentmessages[i]->length > len) continue;
                if(netaddr.host != sentmessages[i]->addr.host && netaddr.port != sentmessages[i]->addr.port) continue;
                switch(sentmessages[i]->type)
                {
                    case CHK_FULL:
                    {
                        uint hash = memhash(netbuf, sentmessages[i]->length);
                        if(hash == 
                    }
                }
            }
        }
    }
    if((waitcondition & ENET_SOCKET_WAIT_WRITE) && outgoingmessages.length())
    {
    }
}

int main(int argc, char **argv)
{
    // TODO

    
}
