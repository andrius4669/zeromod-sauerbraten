#ifdef Z_MS_GAMESERVER_OVERRIDE_H
#error "already z_ms_gameserver_override.h"
#endif
#define Z_MS_GAMESERVER_OVERRIDE_H

#ifndef Z_GBANS_OVERRIDE_H
#error "want z_gbans_override.h"
#endif

VAR(authconnect, 0, 1, 2);
VAR(anyauthconnect, 0, 0, 1);

bool z_allowauthconnect(int priv = PRIV_ADMIN)
{
    return authconnect < 2 ? authconnect!=0 : priv>=PRIV_ADMIN;
}

clientinfo *findauth(int m, uint id)
{
    loopv(clients) if(clients[i]->authmaster == m && clients[i]->authreq == id) return clients[i];
    loopv(connects) if(connects[i]->authmaster == m && connects[i]->authreq == id) return connects[i];
    return NULL;
}

uint nextauthreq = 0;

void authfailed(clientinfo *ci)
{
    if(!ci) return;
    int om = ci->authmaster;
    ci->cleanauth(false);
    for(int m = findauthmaster(ci->authdesc, om); m >= 0; m = findauthmaster(ci->authdesc, m))
    {
        if(!ci->authreq) { if(!nextauthreq) nextauthreq = 1; ci->authreq = nextauthreq++; }
        if(requestmasterf(m, "reqauth %u %s\n", ci->authreq, ci->authname))
        {
            ci->authmaster = m;
            break;
        }
    }
    if(ci->authmaster < 0)
    {
        ci->cleanauth();
        if(ci->connectauth) disconnect_client(ci->clientnum, ci->connectauth);
    }
}

void authfailed(int m, uint id)
{
    authfailed(findauth(m, id));
}

void authsucceeded(int m, uint id, int priv)
{
    clientinfo *ci = findauth(m, id);
    if(!ci) return;
    if(!allowmasterauth(m, priv))
    {
        authfailed(ci);
        return;
    }
    ci->cleanauth(ci->connectauth!=0);
    bool connecting = false;
    if(ci->connectauth)
    {
        if(z_allowauthconnect(priv) || (ci->xi.wlauth && !strcmp(ci->xi.wlauth, ci->authdesc)))
        {
            connected(ci);
            connecting = true;
        }
        else
        {
            disconnect_client(ci->clientnum, ci->connectauth);
            return;
        }
    }
    if(ci->authkickvictim >= 0)
    {
        if(setmaster(ci, true, "", ci->authname, ci->authdesc, priv, false, true, !connecting))
            trykick(ci, ci->authkickvictim, ci->authkickreason, ci->authname, ci->authdesc, priv);
        ci->cleanauthkick();
    }
    else setmaster(ci, true, "", ci->authname, ci->authdesc, priv, false, false, !connecting);
}

void authchallenged(int m, uint id, const char *val, const char *desc = "")
{
    clientinfo *ci = findauth(m, id);
    if(!ci) return;
    sendf(ci->clientnum, 1, "risis", N_AUTHCHAL, desc, id, val);
}

bool tryauth(clientinfo *ci, const char *user, const char *desc)
{
    ci->cleanauth();
    if(!nextauthreq) nextauthreq = 1;
    ci->authreq = nextauthreq++;
    filtertext(ci->authname, user, false, false, 100);
    copystring(ci->authdesc, desc);
    userinfo *u = users.access(userkey(ci->authname, ci->authdesc));
    if(u)
    {
        uint seed[3] = { ::hthash(serverauth) + detrnd(size_t(ci) + size_t(user) + size_t(desc), 0x10000), uint(totalmillis), randomMT() };
        vector<char> buf;
        ci->authchallenge = genchallenge(u->pubkey, seed, sizeof(seed), buf);
        sendf(ci->clientnum, 1, "risis", N_AUTHCHAL, desc, ci->authreq, buf.getbuf());
    }
    else
    {
        bool tried = false;
        for(int m = findauthmaster(desc); m >= 0; m = findauthmaster(desc, m))
        {
            tried = true;
            if(requestmasterf(m, "reqauth %u %s\n", ci->authreq, ci->authname))
            {
                ci->authmaster = m;
                break;
            }
        }
        if(ci->authmaster < 0)
        {
            ci->cleanauth();
            if(tried) sendf(ci->clientnum, 1, "ris", N_SERVMSG, "not connected to authentication server");
        }
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
    int om = ci->authmaster;
    if(om < 0)
    {
        if(ci->authchallenge && checkchallenge(val, ci->authchallenge))
        {
            userinfo *u = users.access(userkey(ci->authname, ci->authdesc));
            if(u)
            {
                bool connecting = false;
                if(ci->connectauth)
                {
                    if(z_allowauthconnect(u->privilege) || (ci->xi.wlauth && !strcmp(ci->xi.wlauth, desc)))
                    {
                        connected(ci);
                        connecting = true;
                    }
                    else
                    {
                        ci->cleanauth();
                        return false;
                    }
                }
                if(ci->authkickvictim >= 0)
                {
                    if(setmaster(ci, true, "", ci->authname, ci->authdesc, u->privilege, false, true, !connecting))
                        trykick(ci, ci->authkickvictim, ci->authkickreason, ci->authname, ci->authdesc, u->privilege);
                }
                else setmaster(ci, true, "", ci->authname, ci->authdesc, u->privilege, false, false, !connecting);
            }
        }
        ci->cleanauth();
    }
    else if(!requestmasterf(om, "confauth %u %s\n", id, val))
    {
        bool tried = false;
        ci->cleanauth(false);
        for(int m = findauthmaster(desc, om); m >= 0; m = findauthmaster(desc, m))
        {
            tried = true;
            if(!ci->authreq) { if(!nextauthreq) nextauthreq = 1; ci->authreq = nextauthreq++; }
            if(requestmasterf(m, "reqauth %u %s\n", ci->authreq, ci->authname))
            {
                ci->authmaster = m;
                break;
            }
        }
        if(ci->authmaster < 0)
        {
            ci->cleanauth();
            if(tried) sendf(ci->clientnum, 1, "ris", N_SERVMSG, "not connected to authentication server");
        }
    }
    // return false = disconnect
    return ci->authreq || !ci->connectauth;
}

void masterconnected(int m)
{
    if(m >= 0)
    {
        if(isdedicatedserver()) logoutf("master server (%s) connected", getmastername(m));
        // clear gbans on connect (not on disconnect) to prevent ms outages from clearing all bans
        cleargbans(m);
    }
    // XXX can m < 0 even happen? I don't think it can..
}

void masterdisconnected(int m)
{
    if(m >= 0 && isdedicatedserver()) logoutf("master server (%s) disconnected", getmastername(m));
    if(m < 0) cleargbans(-1);
    loopvrev(clients)
    {
        clientinfo *ci = clients[i];
        if(ci->authreq && (m >= 0 ? ci->authmaster == m : ci->authmaster >= 0)) authfailed(ci);
    }
}

void processmasterinput(int m, const char *cmd, int cmdlen, const char *args)
{
    uint id;
    string val;
    if(sscanf(cmd, "failauth %u", &id) == 1)
        authfailed(m, id);
    else if(sscanf(cmd, "succauth %u", &id) == 1)
        authsucceeded(m, id, masterauthpriv_get(m));
    else if(sscanf(cmd, "chalauth %u %255s", &id, val) == 2)
        authchallenged(m, id, val, getmasterauth(m));
    else if(!strncmp(cmd, "cleargbans", cmdlen))
        cleargbans(m);
    else if(sscanf(cmd, "addgban %100s", val) == 1)
        addban(m, val);
    else if(sscanf(cmd, "z_priv %100s", val) == 1)
    {
        switch(val[0])
        {
            case 'a': case 'A': masterauthpriv_set(m, PRIV_ADMIN); break;
            case 'm': case 'M': default: masterauthpriv_set(m, PRIV_AUTH); break;
            case 'n': case 'N': masterauthpriv_set(m, PRIV_NONE); break;
        }
    }
    else
    {
        logoutf("unknown response from master (%s): %s", getmastername(m), cmd);
    }
}
