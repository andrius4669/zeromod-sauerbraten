#ifdef Z_TRIGGERS_H
#error "already z_triggers.h"
#endif
#define Z_TRIGGERS_H

enum
{
    Z_TRIGGER_NONE = 0,
    Z_TRIGGER_STARTUP,
    Z_TRIGGER_NOCLIENTS,
    Z_TRIGGER_NOMASTER
};

typedef void (* z_triggerfunc)(int type);

struct z_triggerinfo
{
    z_triggerfunc fun;
    int type;
};

vector<z_triggerinfo> z_triggers;
static bool z_initedtriggers = false;
static vector<z_triggerinfo> *z_triggersinits = NULL;

static bool z_addtrigger(z_triggerfunc fun, int type)
{
    if(!z_initedtriggers)
    {
        if(!z_triggersinits) z_triggersinits = new vector<z_triggerinfo>;
        z_triggerinfo &t = z_triggersinits->add();
        t.fun = fun;
        t.type = type;
        return false;
    }
    z_triggerinfo &t = z_triggers.add();
    t.fun = fun;
    t.type = type;
    return false;
}

static void z_inittriggers()
{
    z_initedtriggers = true;
    if(z_triggersinits)
    {
        loopv(*z_triggersinits) z_addtrigger((*z_triggersinits)[i].fun, (*z_triggersinits)[i].type);
        DELETEP(z_triggersinits);
    }
}

static void z_exectrigger(int type)
{
    if(!z_initedtriggers) z_inittriggers();
    loopv(z_triggers) if(z_triggers[i].type == type) z_triggers[i].fun(type);
}

#define Z_TRIGGER(fun, type) template<int N> struct z_tname##fun; template<> struct z_tname##fun<__LINE__> { UNUSED static bool init; }; UNUSED bool z_tname##fun<__LINE__>::init = z_addtrigger(fun, type)
