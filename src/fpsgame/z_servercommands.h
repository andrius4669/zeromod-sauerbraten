#ifdef Z_SERVERCOMMANDS_H
#error "already z_servercommands.h"
#endif
#define Z_SERVERCOMMANDS_H

#ifndef Z_FORMAT_H
#error "want z_format.h"
#endif

typedef void (* z_scmdfun)(int argc, char **argv, int sender);

enum
{
    ZC_PRIV_MASK = 0x0F,
    ZC_HIDDEN = 1 << 8,
    ZC_DISABLED = 1 << 9
};

struct z_servcmdinfo
{
    string name;
    z_scmdfun fun;
    int privilege;
    int numargs;
    bool hidden;
    bool enabled;

    void setopts(uint opts)
    {
        privilege = opts & ZC_PRIV_MASK;
        hidden = !!(opts & ZC_HIDDEN);
        enabled = !(opts & ZC_DISABLED);
    }

    z_servcmdinfo(): fun(NULL), numargs(0)
    {
        name[0] = '\0';
        setopts(0);
    }
    z_servcmdinfo(const z_servcmdinfo &n) { *this = n; }
    z_servcmdinfo(const char *_name, z_scmdfun _fun, uint opts, int _numargs): fun(_fun), numargs(_numargs)
    {
        copystring(name, _name);
        setopts(opts);
    }
    z_servcmdinfo &operator =(const z_servcmdinfo &n)
    {
        if(&n != this)
        {
            copystring(name, n.name);
            fun = n.fun;
            privilege = n.privilege;
            numargs = n.numargs;
            hidden = n.hidden;
            enabled = n.enabled;
        }
        return *this;
    }
    ~z_servcmdinfo() {}

    bool valid() const { return name[0] && fun; }
    bool cansee(int priv, bool local) const { return valid() && enabled && !hidden && (priv >= privilege || local); }
    bool canexec(int priv, bool local) const { return valid() && ((enabled && priv >= privilege) || local); }
};

vector<z_servcmdinfo> z_servcommands;
static bool z_initedservcommands = false;
static vector<z_servcmdinfo> *z_servcommandinits = NULL;

static bool addservcmd(const z_servcmdinfo &cmd)
{
    if(!z_initedservcommands)
    {
        if(!z_servcommandinits) z_servcommandinits = new vector<z_servcmdinfo>;
        z_servcommandinits->add(cmd);
        return false;
    }
    z_servcommands.add(cmd);
    return false;
}

static void z_initservcommands()
{
    z_initedservcommands = true;
    if(z_servcommandinits)
    {
        loopv(*z_servcommandinits) if((*z_servcommandinits)[i].valid()) addservcmd((*z_servcommandinits)[i]);
        DELETEP(z_servcommandinits);
    }
}

static z_servcmdinfo *z_servcmd_find(const char *cmd)
{
    loopv(z_servcommands) if(!strcasecmp(z_servcommands[i].name, cmd)) return &z_servcommands[i];
    return NULL;
}

void z_servcmd_set_privilege(const char *cmd, int privilege)
{
    if(!z_initedservcommands) z_initservcommands();
    loopv(z_servcommands) if(!strcasecmp(z_servcommands[i].name, cmd)) z_servcommands[i].privilege = privilege;
}

void commands_privilege(tagval *args, int numargs)
{
    vector<char *> commands;
    for(int i = 0; i + 1 < numargs; i += 2)
    {
        explodelist(args[i].getstr(), commands);
        loopvj(commands) z_servcmd_set_privilege(commands[j], clamp(args[i + 1].getint(), 0, int(PRIV_ADMIN + 1)));
        commands.deletearrays();
    }
}
COMMAND(commands_privilege, "si2V");

void z_enable_command(const char *cmd, bool val)
{
    if(!z_initedservcommands) z_initservcommands();
    loopv(z_servcommands) if(!strcasecmp(z_servcommands[i].name, cmd)) z_servcommands[i].enabled = val;
}

void z_enable_commands(tagval *args, int numargs, bool val)
{
    loopi(numargs) z_enable_command(args[i].getstr(), val);
}
ICOMMAND(commands_enable, "sV", (tagval *args, int numargs), z_enable_commands(args, numargs, true));
ICOMMAND(commands_disable, "sV", (tagval *args, int numargs), z_enable_commands(args, numargs, false));

static void z_hide_command(const char *cmd, bool val)
{
    if(!z_initedservcommands) z_initservcommands();
    loopv(z_servcommands) if(!strcasecmp(z_servcommands[i].name, cmd)) z_servcommands[i].hidden = val;
}

void z_hide_commands(tagval *args, int numargs, bool val)
{
    loopi(numargs) z_hide_command(args[i].getstr(), val);
}
ICOMMAND(commands_hide, "sV", (tagval *args, int numargs), z_hide_commands(args, numargs, true));
ICOMMAND(commands_show, "sV", (tagval *args, int numargs), z_hide_commands(args, numargs, false));

static void z_delete_command(const char *cmd)
{
    if(!z_initedservcommands) z_initservcommands();
    loopv(z_servcommands) if(!strcasecmp(z_servcommands[i].name, cmd))
    {
        z_servcommands.remove(i--);
    }
}

void z_delete_commands(tagval *args, int numargs)
{
    loopi(numargs) z_delete_command(args[i].getstr());
}
ICOMMAND(commands_delete, "sV", (tagval *args, int numargs), z_delete_commands(args, numargs));

void z_clone_command(tagval *args, int numargs)
{
    if(numargs < 2) return;
    if(!z_initedservcommands) z_initservcommands();
    int c = -1;
    const char *cmd = args[0].getstr();
    loopv(z_servcommands) if(!strcasecmp(z_servcommands[i].name, cmd)) { c = i; break; }
    if(c < 0) return;
    for(int i = 1; i < numargs; ++i)
    {
        z_servcmdinfo &ncmd = z_servcommands.add(z_servcommands[c]);
        copystring(ncmd.name, args[i].getstr());
    }
}
ICOMMAND(commands_clone, "sV", (tagval *args, int numargs), z_clone_command(args, numargs));

#define SCOMMANDZ(name, opts, func, args) UNUSED static bool __s_dummy__##name = addservcmd(z_servcmdinfo(#name, func, opts, args))
#define SCOMMAND(name, opts, func) SCOMMANDZ(name, opts, func, 0)
#define SCOMMANDH(name, opts, func) SCOMMAND(name, (opts) | ZC_HIDDEN, func)
#define SCOMMANDA SCOMMANDZ
#define SCOMMANDAH(name, opts, func, args) SCOMMANDZ(name, (opts) | ZC_HIDDEN, func, args)
#define SHELP(name, help)

VAR(allowservcmd, 0, 0, 1);

SVAR(servcmd_message_pleasespecifyclient, "please specify client number");
static inline void z_servcmd_pleasespecifyclient(int cntosend)
{
    if(*servcmd_message_pleasespecifyclient) sendf(cntosend, 1, "ris", N_SERVMSG, servcmd_message_pleasespecifyclient);
}

SVAR(servcmd_message_unknownclient, "unknown client: %s");
static void z_servcmd_unknownclient(const char *clientname, int cntosend)
{
    string buf;
    z_formattemplate ft[] =
    {
        { 's', "%s", clientname },
        { 0,   NULL, NULL }
    };
    z_format(buf, sizeof(buf), servcmd_message_unknownclient, ft);
    if(*buf) sendf(cntosend, 1, "ris", N_SERVMSG, buf);
}

SVAR(servcmd_message_pleasespecifymessage, "please specify message");
static inline void z_servcmd_pleasespecifymessage(int cntosend)
{
    if(*servcmd_message_pleasespecifymessage) sendf(cntosend, 1, "ris", N_SERVMSG, servcmd_message_pleasespecifymessage);
}
