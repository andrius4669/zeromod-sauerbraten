#ifdef Z_MS_ENGINESERVER_OVERRIDE_H
#error "already z_ms_engineserver_override.h"
#endif
#define Z_MS_ENGINESERVER_OVERRIDE_H

ENetAddress serveraddress = { ENET_HOST_ANY, ENET_PORT_ANY };

struct msinfo
{
    string mastername;
    int masterport;
    ENetSocket mastersock;
    int lastupdatemaster, lastconnectmaster, masterconnecting, masterconnected;
    int reconnectdelay;
    // NOTE: masterconnected is also reused for tracking last registration input from masterserver, after it's been connected
    vector<char> masterout, masterin;
    int masteroutpos, masterinpos;
    bool allowupdatemaster;
    int masternum;
    string masterauth;
    int masterauthpriv;
    bool masterauthpriv_allow;
    char *networkident;
    char *whitelistauth;
    char *banmessage;
    int minauthpriv, maxauthpriv;
    int banmode;

    msinfo(): masterport(server::masterport()), mastersock(ENET_SOCKET_NULL),
        lastupdatemaster(0), lastconnectmaster(0), masterconnecting(0), masterconnected(0), reconnectdelay(0),
        masteroutpos(0), masterinpos(0), allowupdatemaster(true), masternum(-1),
        masterauthpriv(-1), masterauthpriv_allow(false), networkident(NULL),
        whitelistauth(NULL), banmessage(NULL), minauthpriv(0), maxauthpriv(3), banmode(1)
    {
        copystring(mastername, server::defaultmaster());
        masterauth[0] = '\0';
    }
    ~msinfo() { disconnectmaster(); delete[] networkident; delete[] whitelistauth; delete[] banmessage; }

    const char *fullname()
    {
        static char *mn = NULL; // this won't bite us?
        if(masterport == server::masterport()) return mastername;
        if(!mn) mn = new char[MAXSTRLEN];
        nformatstring(mn, MAXSTRLEN, "%s:%d", mastername, masterport);
        return mn;
    }

    void disconnectmaster()
    {
        if(mastersock != ENET_SOCKET_NULL)
        {
            server::masterdisconnected(masternum);
            enet_socket_destroy(mastersock);
            mastersock = ENET_SOCKET_NULL;
        }

        masterout.setsize(0);
        masterin.setsize(0);
        masteroutpos = masterinpos = 0;

        lastupdatemaster = masterconnecting = masterconnected = 0;
        masterauthpriv = -1;
    }

    ENetSocket connectmaster(bool wait)
    {
        // if disabled bail out
        if(!mastername[0]) return ENET_SOCKET_NULL;
        // resolve everytime. we dont want to use old IP incase it changes after masterserver being unreachable for a while
        ENetAddress masteraddress = { ENET_HOST_ANY, (enet_uint16)masterport };
        if(isdedicatedserver()) logoutf("looking up %s...", mastername);
        if(!resolverwait(mastername, &masteraddress)) return ENET_SOCKET_NULL;
        // connect to it
        ENetSocket sock = enet_socket_create(ENET_SOCKET_TYPE_STREAM);
        if(sock == ENET_SOCKET_NULL)
        {
            if(isdedicatedserver()) logoutf("could not open master server socket");
            return ENET_SOCKET_NULL;
        }
        if(wait || serveraddress.host == ENET_HOST_ANY || !enet_socket_bind(sock, &serveraddress))
        {
            enet_socket_set_option(sock, ENET_SOCKOPT_NONBLOCK, 1);
            enet_socket_set_option(sock, ENET_SOCKOPT_KEEPALIVE, 1);
            if(wait)
            {
                if(!connectwithtimeout(sock, fullname(), masteraddress)) return sock;
            }
            else if(!enet_socket_connect(sock, &masteraddress)) return sock;
        }
        enet_socket_destroy(sock);
        if(isdedicatedserver()) logoutf("could not connect to master server (%s)", fullname());
        return ENET_SOCKET_NULL;
    }

    bool requestmaster(const char *req)
    {
        if(mastersock == ENET_SOCKET_NULL)
        {
            mastersock = connectmaster(false);
            if(mastersock == ENET_SOCKET_NULL) return false;
            lastconnectmaster = masterconnecting = totalmillis ? totalmillis : 1;
            if(!reconnectdelay) reconnectdelay = 2000;
            else
            {
                reconnectdelay += reconnectdelay/2;
                if(reconnectdelay > 5*60*1000) reconnectdelay = 5*60*1000;
            }
        }

        if(masterout.length() >= 4096) return false;

        masterout.put(req, strlen(req));
        return true;
    }

    bool requestmasterf(const char *fmt, ...)
    {
        defvformatstring(req, fmt, fmt);
        return requestmaster(req);
    }

    void processmasterinput()
    {
        if(masterinpos >= masterin.length()) return;

        char *input = &masterin[masterinpos], *end = (char *)memchr(input, '\n', masterin.length() - masterinpos);
        while(end)
        {
            *end = '\0';

            const char *args = input;
            while(args < end && !iscubespace(*args)) args++;
            int cmdlen = args - input;
            while(args < end && iscubespace(*args)) args++;

            if(!strncmp(input, "failreg", cmdlen))
            {
                //masterconnected = totalmillis ? totalmillis : 1;
                conoutf(CON_ERROR, "master server (%s) registration failed: %s", fullname(), args);
                disconnectmaster();
                return;
            }
            else if(!strncmp(input, "succreg", cmdlen))
            {
                masterconnected = totalmillis ? totalmillis : 1;
                reconnectdelay = 0;
                conoutf("master server (%s) registration succeeded", fullname());
            }
            else server::processmasterinput(masternum, input, cmdlen, args);

            end++;
            masterinpos = end - masterin.getbuf();
            input = end;
            end = (char *)memchr(input, '\n', masterin.length() - masterinpos);
        }

        if(masterinpos >= masterin.length())
        {
            masterin.setsize(0);
            masterinpos = 0;
        }
        else if(masterinpos >= 1024)
        {
            masterin.remove(0, masterinpos);
            masterinpos = 0;
        }
    }

    void flushmasteroutput()
    {
        if(masterconnecting && totalmillis - masterconnecting >= 60000)
        {
            logoutf("could not connect to master server (%s)", fullname());
            disconnectmaster();
        }
        if(masterout.empty() || !masterconnected) return;

        ENetBuffer buf;
        buf.data = &masterout[masteroutpos];
        buf.dataLength = masterout.length() - masteroutpos;
        int sent = enet_socket_send(mastersock, NULL, &buf, 1);
        if(sent >= 0)
        {
            masteroutpos += sent;
            if(masteroutpos >= masterout.length())
            {
                masterout.setsize(0);
                masteroutpos = 0;
            }
            else if(masteroutpos >= 1024) // keep things tidy
            {
                masterout.remove(0, masteroutpos);
                masteroutpos = 0;
            }
        }
        else disconnectmaster();
    }

    void flushmasterinput()
    {
        if(masterin.length() >= masterin.capacity())
            masterin.reserve(4096);

        ENetBuffer buf;
        buf.data = masterin.getbuf() + masterin.length();
        buf.dataLength = masterin.capacity() - masterin.length();
        int recv = enet_socket_receive(mastersock, NULL, &buf, 1);
        if(recv > 0)
        {
            masterin.advance(recv);
            processmasterinput();
        }
        else if(recv >= -1) disconnectmaster();
    }

    void updatemasterserver()
    {
        extern int serverport;
        if(!masterconnected && lastconnectmaster && totalmillis-lastconnectmaster <= reconnectdelay) return;
        if(masterconnected && lastupdatemaster && lastupdatemaster-masterconnected > 0 && totalmillis-lastupdatemaster >= 5*60*1000) disconnectmaster();
        if(mastername[0] && allowupdatemaster)
        {
            requestmasterf("regserv %d\n", serverport);
            lastupdatemaster = totalmillis ? totalmillis : 1;
        }
    }
};

vector<msinfo> mss;     // masterservers
int currmss = -1;

void addms()
{
    mss.add();
    currmss = mss.length()-1;
    mss[currmss].masternum = currmss;
}
COMMAND(addms, "");
ICOMMAND(addmaster, "", (), addms());

void clearmss()
{
    server::masterdisconnected(-1);
    mss.shrink(0);
    currmss = -1;
}
COMMAND(clearmss, "");
ICOMMAND(clearmasters, "", (), clearmss());

VARFN(updatemaster, allowupdatemaster, 0, 1, 1,
{
    if(!mss.inrange(currmss)) addms();
    mss[currmss].allowupdatemaster = allowupdatemaster;
});

SVARF(mastername, server::defaultmaster(),
{
    if(!mss.inrange(currmss)) addms();
    mss[currmss].disconnectmaster();
    mss[currmss].lastconnectmaster = 0;
    mss[currmss].reconnectdelay = 0;
    copystring(mss[currmss].mastername, mastername);
});

VARF(masterport, 1, server::masterport(), 0xFFFF,
{
    if(!mss.inrange(currmss)) addms();
    mss[currmss].disconnectmaster();
    mss[currmss].lastconnectmaster = 0;
    mss[currmss].reconnectdelay = 0;
    mss[currmss].masterport = masterport;
});

SVARF(masterauth, "",
{
    if(!mss.inrange(currmss)) addms();
    copystring(mss[currmss].masterauth, masterauth);
});

bool requestmasterf(int m, const char *fmt, ...)
{
    if(mss.inrange(m))
    {
        defvformatstring(req, fmt, fmt);
        return mss[m].requestmaster(req);
    }
    else return false;
}

ENetSocket connectmaster(int m, bool wait)
{
    return mss.inrange(m) ? mss[m].connectmaster(wait) : ENET_SOCKET_NULL;
}

void updatemasterserver()
{
    loopv(mss) mss[i].updatemasterserver();
}

void flushmasteroutput()
{
    loopv(mss) mss[i].flushmasteroutput();
}

int findauthmaster(const char *desc, int old)
{
    for(int m = old >= 0 ? old + 1 : 0; m < mss.length(); m++) if(!strcmp(desc, mss[m].masterauth)) return m;
    return -1;
}

const char *getmastername(int m)
{
    if(!mss.inrange(m)) return "";
    return mss[m].fullname();
}

const char *getmasterauth(int m) { return mss.inrange(m) ? mss[m].masterauth : ""; }

int nummss() { return mss.length(); }

ICOMMAND(masterauthpriv, "i", (int *i),
{
    if(!mss.inrange(currmss)) addms();
    mss[currmss].masterauthpriv_allow = *i!=0;
});

ICOMMAND(masterminauthpriv, "i", (int *i),
{
    if(!mss.inrange(currmss)) addms();
    mss[currmss].minauthpriv = clamp(*i, 0, 3);
});

ICOMMAND(mastermaxauthpriv, "i", (int *i),
{
    if(!mss.inrange(currmss)) addms();
    mss[currmss].maxauthpriv = clamp(*i, 0, 3);
});

void masterauthpriv_set(int m, int priv)
{
    if(!mss.inrange(m)) return;
    if(mss[m].masterauthpriv_allow) mss[m].masterauthpriv = clamp(priv, 0, mss[m].maxauthpriv);
}

int masterauthpriv_get(int m)
{
    if(!mss.inrange(m)) return 2;
    int authpriv = mss[m].masterauthpriv;
    mss[m].masterauthpriv = -1;
    return authpriv >= 0 ? authpriv : min(mss[m].maxauthpriv, 2);
}

bool allowmasterauth(int m, int priv)
{
    if(!mss.inrange(m)) return false;
    return priv >= mss[m].minauthpriv && priv <= mss[m].maxauthpriv;
}

ICOMMAND(masterbanwarn, "s", (char *s),
{
    if(!mss.inrange(currmss)) addms();
    delete[] mss[currmss].networkident;
    mss[currmss].networkident = *s ? newstring(s) : NULL;
});

ICOMMAND(masterwhitelistauth, "s", (char *s),
{
    if(!mss.inrange(currmss)) addms();
    delete[] mss[currmss].whitelistauth;
    mss[currmss].whitelistauth = *s ? newstring(s) : NULL;
});

ICOMMAND(masterbanmessage, "s", (char *s),
{
    if(!mss.inrange(currmss)) addms();
    delete[] mss[currmss].banmessage;
    mss[currmss].banmessage = *s ? newstring(s) : NULL;
});

ICOMMAND(masterbanmode, "i", (int *i),    // -1 - whitelist, 0 - ignore, 1 (default) - blacklist, 2 - specban
{
    if(!mss.inrange(currmss)) addms();
    mss[currmss].banmode = clamp(*i, -1, 2);
});

bool getmasterbaninfo(int m, const char *&ident, int &disc, const char *&wlauth, const char *&banmsg)
{
    if(!mss.inrange(m)) return false;
    ident = mss[m].networkident;
    disc = mss[m].banmode;
    wlauth = mss[m].whitelistauth;
    banmsg = mss[m].banmessage;
    return true;
}
