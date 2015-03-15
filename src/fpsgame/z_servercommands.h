#ifndef Z_SERVERCOMMANDS_H
#define Z_SERVERCOMMANDS_H

typedef void (* z_scmdfun)(int argc, char **argv, int sender);

struct z_servcmdinfo
{
    string name;
    z_scmdfun fun;
    int privilege;
    int numargs;
    bool hidden;
    int vispriv;
    bool enabled;

    z_servcmdinfo(): fun(NULL), privilege(PRIV_NONE), numargs(0), hidden(false), vispriv(PRIV_NONE), enabled(true) { name[0] = '\0'; }
    z_servcmdinfo(const z_servcmdinfo &n) { *this = n; }
    z_servcmdinfo(const char *_name, z_scmdfun _fun, int _priv, int _numargs, bool _hidden = false, int _vispriv = PRIV_NONE):
        fun(_fun), privilege(_priv), numargs(_numargs), hidden(_hidden), vispriv(_vispriv), enabled(true) { copystring(name, _name); }
    z_servcmdinfo &operator =(const z_servcmdinfo &n)
    {
        if(&n != this)
        {
            copystring(name, n.name);
            fun = n.fun;
            privilege = n.privilege;
            numargs = n.numargs;
            hidden = n.hidden;
            vispriv = n.vispriv;
            enabled = n.enabled;
        }
        return *this;
    }
    ~z_servcmdinfo() {}

    bool valid() const { return name[0] && fun; }
    bool cansee(int priv, bool local) const { return valid() && enabled && !hidden && ((priv >= privilege && priv >= vispriv) || local); }
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

void z_servcmd_set_privilege(const char *cmd, int privilege)
{
    if(!z_initedservcommands) z_initservcommands();
    loopv(z_servcommands) if(!strcasecmp(z_servcommands[i].name, cmd)) z_servcommands[i].privilege = privilege;
}

void z_enable_command(const char *cmd, bool val)
{
    if(!z_initedservcommands) z_initservcommands();
    loopv(z_servcommands) if(!strcasecmp(z_servcommands[i].name, cmd)) z_servcommands[i].enabled = val;
}

void z_enable_commands(tagval *args, int numargs, bool val)
{
    loopi(numargs) z_enable_command(args[i].getstr(), val);
}
ICOMMAND(enable_commands, "sV", (tagval *args, int numargs), z_enable_commands(args, numargs, true));
ICOMMAND(disable_commands, "sV", (tagval *args, int numargs), z_enable_commands(args, numargs, false));

#define SCOMMANDZ(_name, _priv, _funcname, _args, _hidden) UNUSED static bool __s_dummy__##_name = addservcmd(z_servcmdinfo(#_name, _funcname, _priv, _args, _hidden))
#define SCOMMAND(_name, _priv, _func) SCOMMANDZ(_name, _priv, _func, 0, false)
#define SCOMMANDN SCOMMAND
#define SCOMMANDH(_name, _priv, _func) SCOMMANDZ(_name, _priv, _func, 0, true)
#define SCOMMANDNH SCOMMANDH
#define SCOMMANDA(_name, _priv, _func, _args) SCOMMANDZ(_name, _priv, _func, _args, false)
#define SCOMMANDNA SCOMMANDA
#define SCOMMANDAH(_name, _priv, _func, _args) SCOMMANDZ(_name, _priv, _func, _args, true)
#define SCOMMANDNAH SCOMMANDAH


VAR(allowservcmd, 0, 1, 1);

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
    z_format(buf, sizeof buf, servcmd_message_unknownclient, ft);
    if(*buf) sendf(cntosend, 1, "ris", N_SERVMSG, buf);
}

SVAR(servcmd_message_pleasespecifymessage, "please specify message");
static inline void z_servcmd_pleasespecifymessage(int cntosend)
{
    if(*servcmd_message_pleasespecifymessage) sendf(cntosend, 1, "ris", N_SERVMSG, servcmd_message_pleasespecifymessage);
}

#endif // Z_SERVERCOMMANDS_H
