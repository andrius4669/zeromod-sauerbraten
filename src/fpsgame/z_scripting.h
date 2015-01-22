#ifndef Z_SCRIPTING_H
#define Z_SCRIPTING_H

typedef void (* z_sleepfunc)(void *);
typedef void (* z_freevalfunc)(void *);

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
vector<z_sleepstruct> z_sleeps, z_announces;

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
    z_checksleep(z_sleeps);
    z_checksleep(z_announces);
}

static void z_sleepcmd_announce(void *str)
{
    if(str) sendservmsg((char *)str);
}

static void z_freestring(void *str) { delete[] (char *)str; }

void s_announce(int *offset, int *delay, char *message)
{
    z_addsleep(z_announces, *offset, *delay, true, z_sleepcmd_announce, newstring(message), z_freestring);
}
COMMAND(s_announce, "iis");

void s_clearannounces()
{
    z_clearsleep(z_announces);
}
COMMAND(s_clearannounces, "");

static void z_sleepcmd_cubescript(void *cmd)
{
    if(cmd) execute((char *)cmd);
}

void s_sleep(int *offset, int *delay, char *script)
{
    z_addsleep(z_sleeps, *offset, *delay, false, z_sleepcmd_cubescript, newstring(script), z_freestring);
}
COMMAND(s_sleep, "iis");

void s_sleep_r(int *offset, int *delay, char *script)
{
    z_addsleep(z_sleeps, *offset, *delay, true, z_sleepcmd_cubescript, newstring(script), z_freestring);
}
COMMAND(s_sleep_r, "iis");

void s_clearsleep()
{
    z_clearsleep(z_sleeps);
}
COMMAND(s_clearsleep, "");

#endif // Z_SCRIPTING_H
