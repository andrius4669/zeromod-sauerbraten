#ifdef Z_SERVCMD_H
#error "already z_servcmd.h"
#endif
#define Z_SERVCMD_H

#ifndef Z_LOG_H
#error "want z_log.h"
#endif
#ifndef Z_SERVERCOMMANDS_H
#error "want z_servercommands.h"
#endif


#define Z_MAXSERVCMDARGS 32


bool z_parseclient(const char *str, int &cn)
{
    char *end = NULL;
    int n = strtol(str, &end, 10);
    if(end && !*end) { cn = n; return true; }
    loopv(clients) if(!strcmp(str, clients[i]->name)) { cn = clients[i]->clientnum; return true; }
    loopv(clients) if(!strcasecmp(str, clients[i]->name)) { cn = clients[i]->clientnum; return true; }
    return false;
}

bool z_parseclient_verify(const char *str, int &cn, bool allowall, bool allowbot, bool allowspy)
{
    if(!z_parseclient(str, cn)) return false;
    if(cn < 0) return allowall;
    clientinfo *ci = allowbot ? getinfo(cn) : (clientinfo *)getclientinfo(cn);
    return ci && ci->connected && (allowspy || !ci->spy);
}

clientinfo *z_parseclient_return(const char *str, bool allowbot, bool allowspy)
{
    int cn;
    if(!z_parseclient(str, cn)) return NULL;
    if(cn < 0) return NULL;
    clientinfo *ci = allowbot ? getinfo(cn) : (clientinfo *)getclientinfo(cn);
    return (ci && ci->connected && (allowspy || !ci->spy)) ? ci : NULL;
}

SVAR(servcmd_chars, "");

bool z_servcmd_check(char *&text)
{
    const char *c = &servcmd_chars[0];
    while(*c != '\0' && *c != text[0]) c++;
    if(*c == '\0' || text[1] == '\0') return false;
    text++;
    if(*text == *c) return false;   // double cmd char -> interept as normal message
    return true;
}


void z_servcmd_parse(int sender, char *text)
{
    if(!z_initedservcommands) z_initservcommands();

    clientinfo *ci = (clientinfo *)getclientinfo(sender);
    if(!ci) return;

    for(char *p = text; *p; p++) if(iscubespace(*p) && *p!='\f') *p = ' ';
    z_log_servcmd(ci, text);

    int argc = 1;
    char *argv[Z_MAXSERVCMDARGS];
    argv[0] = text;
    for(size_t i = 1; i < sizeof(argv)/sizeof(argv[0]); i++) argv[i] = NULL;
    char *s = strchr(text, ' ');
    if(s)
    {
        *s++ = '\0';
        while(*s == ' ') s++;
        if(*s) { argv[1] = s; argc++; }
    }
    z_servcmdinfo *cc = NULL;
    loopv(z_servcommands)
    {
        z_servcmdinfo &c = z_servcommands[i];
        if(!c.valid() || strcasecmp(c.name, text)) continue;
        cc = &c;
        break;
    }
    if(!cc || (!cc->enabled && !ci->local) || (cc->hidden && !ci->local && ci->privilege < cc->privilege))
    {
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("unknown server command: %s", text));
        return;
    }
    if(!cc->canexec(ci->privilege, ci->local))
    {
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("you need to claim %s to execute this server command", privname(cc->privilege)));
        return;
    }
    int n = cc->numargs ? min(cc->numargs+1, Z_MAXSERVCMDARGS) : Z_MAXSERVCMDARGS;
    if(argv[1]) for(int i = 2; i < n; i++)
    {
        s = strchr(argv[i - 1], ' ');
        if(!s) break;
        *s++ = '\0';
        while(*s == ' ') s++;
        if(*s == '\0') break;
        argv[i] = s;
        argc++;
    }
    cc->fun(argc, argv, sender);
}
