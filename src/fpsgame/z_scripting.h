#ifndef Z_SCRIPTING_H
#define Z_SCRIPTING_H

#include "z_servcmd.h"

typedef void (* z_sleepfunc)(void *);
typedef void (* z_freevalfunc)(void *);

enum
{
    ZS_SLEEPS = 0,
    ZS_ANNOUNCES,
    ZS_NUM
};

struct z_sleepstruct
{
    int delay, millis;
    void *val;
    bool reload;
    z_sleepfunc func;
    z_freevalfunc freeval;

    z_sleepstruct() {}
    ~z_sleepstruct() { if(freeval) freeval(val); }
};
vector<z_sleepstruct> z_sleeps[ZS_NUM];

static void z_addsleep(vector<z_sleepstruct> &sleeps, int offset, int delay, bool reload, z_sleepfunc func, void *val, z_freevalfunc freeval)
{
    z_sleepstruct &ss = sleeps.add();
    ss.millis = totalmillis + offset;
    ss.delay = delay;
    ss.func = func;
    ss.freeval = freeval;
    ss.val = val;
    ss.reload = reload;
}

static void z_clearsleep(vector<z_sleepstruct> &sleeps) { sleeps.shrink(0); }

static void z_checksleep(vector<z_sleepstruct> &sleeps)
{
    loopv(sleeps)
    {
        z_sleepstruct &ss = sleeps[i];
        if(totalmillis-ss.millis >= ss.delay)
        {
            // prepare
            z_sleepfunc sf = ss.func;
            ss.func = NULL;     // its sign that this entry is currently being executed
            z_freevalfunc fvf = ss.freeval;
            ss.freeval = NULL;  // disallow freeing value while executing
            void *val = ss.val;
            // execute
            sf(val);
            // check if not cleared
            if(sleeps.inrange(i) && ss.func == NULL)  // its still the same entry
            {
                if(ss.reload)
                {
                    ss.func = sf;
                    ss.freeval = fvf;
                    ss.millis += ss.delay;
                }
                else
                {
                    if(fvf) fvf(val);
                    sleeps.remove(i--);
                }
            }
            else    // sleeps array was modified in function
            {
                if(fvf) fvf(val);
                break;              // abandom, since array was modified
            }
        }
    }
}

void z_checksleep()
{
    loopi(sizeof(z_sleeps)/sizeof(z_sleeps[0])) z_checksleep(z_sleeps[i]);
}

static void z_sleepcmd_announce(void *str)
{
    if(str) sendservmsg((char *)str);
}

static void z_freestring(void *str) { delete[] (char *)str; }

void s_announce(int *offset, int *delay, char *message)
{
    z_addsleep(z_sleeps[ZS_ANNOUNCES], *offset, *delay, true, z_sleepcmd_announce, newstring(message), z_freestring);
}
COMMAND(s_announce, "iis");

void s_clearannounces()
{
    z_clearsleep(z_sleeps[ZS_ANNOUNCES]);
}
COMMAND(s_clearannounces, "");

static void z_sleepcmd_cubescript(void *cmd)
{
    if(cmd) execute((char *)cmd);
}

void s_sleep(int *offset, int *delay, char *script)
{
    z_addsleep(z_sleeps[ZS_SLEEPS], *offset, *delay, false, z_sleepcmd_cubescript, newstring(script), z_freestring);
}
COMMAND(s_sleep, "iis");

void s_sleep_r(int *offset, int *delay, char *script)
{
    z_addsleep(z_sleeps[ZS_SLEEPS], *offset, *delay, true, z_sleepcmd_cubescript, newstring(script), z_freestring);
}
COMMAND(s_sleep_r, "iis");

void s_clearsleep()
{
    z_clearsleep(z_sleeps[ZS_SLEEPS]);
}
COMMAND(s_clearsleep, "");

void s_write(char *c, char *msg)
{
    int cn;
    if(!z_parseclient_verify(c, cn, true, false, true)) return;
    sendf(cn, 1, "ris", N_SERVMSG, msg);
}
COMMAND(s_write, "ss");

ICOMMAND(s_wall, "C", (char *s), sendservmsg(s));

void s_kick(char *c, char *reason)
{
    int cn;
    if(!z_parseclient_verify(c, cn, true, false, true) || getinfo(cn)->local) return;
    /* todo: print msg with reason */
    disconnect_client(cn, DISC_KICK);
}
COMMAND(s_kick, "ss");

void s_kickban(char *c, char *t, char *reason)
{
    int cn;
    if(!z_parseclient_verify(c, cn, true, false, true) || getinfo(cn)->local) return;
    uint ip = getclientip(cn);
    if(!ip) return;

    int time = t[0] ? z_readbantime(t) : 4*60*60000;
    if(time <= 0) return;

    addban(ip, time, BAN_KICK, reason);
    kickclients(ip);
}
COMMAND(s_kickban, "sss");

void s_listclients(int *bots)
{
    string cn;
    vector<char> buf;
    loopv(clients) if(clients[i]->state.aitype==AI_NONE || (*bots > 0 && clients[i]->state.state!=CS_SPECTATOR))
    {
        if(buf.length()) buf.add(' ');
        formatstring(cn)("%d", clients[i]->clientnum);
        buf.put(cn, strlen(cn));
    }
    buf.add('\0');
    result(buf.getbuf());
}
COMMAND(s_listclients, "i");

#endif // Z_SCRIPTING_H
