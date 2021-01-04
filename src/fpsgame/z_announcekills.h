#ifdef Z_ANNOUNCEKILLS_H
#error "already z_announcekills.h"
#endif
#define Z_ANNOUNCEKILLS_H


VAR(announcekills, 0, 0, 1);

VAR(announcekills_teamnames, 0, 1, 1);

VAR(announcekills_stopped, 0, 1, 1);
VAR(announcekills_stopped_min, 0, 15, 100);

VAR(announcekills_multikill, 0, 2, 2);

VAR(announcekills_rampage, 0, 1, 2);
VAR(announcekills_rampage_min, 0, 0, 100);
VAR(announcekills_numeric, 0, 0, 1);
VAR(announcekills_numeric_num, 1, 5, 100);

VAR(announcekills_noctf, 0, 1, 1);


struct z_multikillstrstruct { int val; char *str; };
vector<z_multikillstrstruct> z_multikillstrings;
ICOMMAND(announcekills_multikillstrings, "is2V", (tagval *args, int numargs),
{
    while(!z_multikillstrings.empty()) delete[] z_multikillstrings.pop().str;
    for(int i = 0; i + 1 < numargs; i += 2)
    {
        z_multikillstrstruct &v = z_multikillstrings.add();
        v.val = args[i].getint();
        v.str = newstring(args[i+1].getstr());
    }
});

SVAR(announcekills_style_stopped, "\f2%V was stopped by %A (\f6%n kills\f2)");
SVAR(announcekills_style_multikills, "\f2%Y scored %sKILL!!");
SVAR(announcekills_style_multikills_num, "\f2%Y scored \f3%n MULTIPLE KILLS!!");
SVAR(announcekills_style_numeric, "\f2%Y made \f7%n\f2 kills");
SVAR(announcekills_style_rampage, "\f2%A is %s");

vector<z_multikillstrstruct> z_rampagestrings;
ICOMMAND(announcekills_rampagestrings, "is2V", (tagval *args, int numargs),
{
    while(!z_rampagestrings.empty()) delete[] z_rampagestrings.pop().str;
    for(int i = 0; i + 1 < numargs; i += 2)
    {
        z_multikillstrstruct &v = z_rampagestrings.add();
        v.val = args[i].getint();
        v.str = newstring(args[i+1].getstr());
    }
});

static const char *z_teamcolorname(clientinfo *ci, const char *alt, clientinfo *as)
{
    static string buf[3];
    static int bufpos = 0;
    const char *name = alt && ci==as ? colorname(ci, alt) : colorname(ci);
    if(!m_teammode || !announcekills_teamnames) return name;
    bufpos = (bufpos+1)%3;
    formatstring(buf[bufpos], "\fs\f%c%s\fr", !strcmp(ci->team, as->team) ? '1' : '3', name);
    return buf[bufpos];
}

static const char *z_multikillstr(int i)
{
    if(z_multikillstrings.length())
    {
        loopv(z_multikillstrings) if(z_multikillstrings[i].val == i) return z_multikillstrings[i].str;
        return NULL;
    }
    // defaults
    switch(i)
    {
        case 2: return "a \f6DOUBLE ";
        case 3: return "a \f6TRIPLE ";
        case 4: return "a \f6QUAD-";
        case 5: return "a \f6PENTA-";
        case 6: return "a \f6HEXA-";
        case 7: return "a \f6HEPTA-";    // sounds weird, but meh
        case 8: return "an \f6OCTA-";
        default: return NULL;
    }
}

static inline bool z_shouldannounce()
{
    return announcekills &&
        (announcekills_rampage ||
            announcekills_multikill ||
            announcekills_stopped) &&
        !m_edit &&
        (!announcekills_noctf || !m_ctf);
}

void z_announcekill(clientinfo *actor, clientinfo *victim, int fragval)
{
    if(fragval > 0)
    {
        if(!actor->state.lastkill || gamemillis-actor->state.lastkill > 2000) actor->state.multikills = 0;
        actor->state.lastkill = gamemillis ? gamemillis : 1;
        actor->state.multikills++;
        actor->state.rampage++;
        if(actor->state.rampage > actor->state.maxstreak) actor->state.maxstreak = actor->state.rampage;
    }
    if(!z_shouldannounce()) return;
    bool showactor = fragval > 0 && actor->state.state==CS_ALIVE;

    loopv(clients)  // message may be different for each client
    {
        clientinfo *ci = clients[i];
        const char *name = NULL, *nname = NULL;

        if(ci->state.aitype!=AI_NONE) continue;

        if(announcekills_stopped && victim!=actor && victim->state.rampage >= announcekills_stopped_min)
        {
            const char *vname = z_teamcolorname(victim, NULL, ci);
            nname = z_teamcolorname(actor, NULL, ci);

            z_formattemplate ft[] =
            {
                { 'V', "%s", (const void *)vname },
                { 'A', "%s", (const void *)nname },
                { 'n', "%d", (const void *)(intptr_t)victim->state.rampage },
                { 0, NULL, NULL }
            };

            string buf;
            z_format(buf, sizeof(buf), announcekills_style_stopped, ft);
            if(*buf) sendf(ci->clientnum, 1, "ris", N_SERVMSG, buf);
        }

        if(showactor && announcekills_multikill && actor->state.multikills > 1 &&
            (announcekills_multikill == 1 || ci->clientnum == actor->clientnum))
        {
            name = z_teamcolorname(actor, "you", ci);
            if(!nname) nname = z_teamcolorname(actor, NULL, ci);
            const char *mkstr = z_multikillstr(actor->state.multikills);

            z_formattemplate ft[] =
            {
                { 'Y', "%s", (const void *)name },
                { 'A', "%s", (const void *)nname },
                { 's', "%s", (const void *)mkstr },
                { 'n', "%d", (const void *)(intptr_t)actor->state.multikills },
                { 0, NULL, NULL }
            };

            string buf;
            z_format(buf, sizeof(buf), mkstr ? announcekills_style_multikills : announcekills_style_multikills_num, ft);
            if(*buf) sendf(ci->clientnum, 1, "ris", N_SERVMSG, buf);
        }

        bool shouldbroadcast = announcekills_rampage == 1 && actor->state.rampage >= announcekills_rampage_min;
        if(showactor && announcekills_rampage && actor->state.rampage > 0 &&
            (shouldbroadcast || ci->clientnum == actor->clientnum))
        {
            if(announcekills_numeric)
            {
                if(actor->state.rampage % announcekills_numeric_num == 0)
                {
                    if(!name) name = z_teamcolorname(actor, "you", ci);
                    if(!nname) nname = z_teamcolorname(actor, NULL, ci);

                    z_formattemplate ft[] =
                    {
                        { 'Y', "%s", (const void *)name },
                        { 'A', "%s", (const void *)nname },
                        { 'n', "%d", (const void *)(intptr_t)actor->state.rampage },
                        { 0, NULL, NULL }
                    };

                    string buf;
                    z_format(buf, sizeof(buf), announcekills_style_numeric, ft);
                    if(*buf) sendf(ci->clientnum, 1, "ris", N_SERVMSG, buf);
                }
            }
            else
            {
                const char *msg;
                if(z_rampagestrings.length())
                {
                    msg = NULL;
                    loopv(z_rampagestrings) if(z_rampagestrings[i].val == actor->state.rampage) { msg = z_rampagestrings[i].str; break; }
                }
                else
                {
                    switch(actor->state.rampage)
                    {
                        case 5:  msg = "on a \f6KILLING SPREE!!"; break;
                        case 10: msg = "on a \f6RAMPAGE!!"; break;
                        case 15: msg = "\f6DOMINATING!!"; break;
                        case 20: msg = "\f6UNSTOPPABLE!!"; break;
                        case 30: msg = "\f6GODLIKE!!"; break;
                        default: msg = NULL; break;
                    }
                }
                if(msg)
                {
                    if(!name) name = z_teamcolorname(actor, "you", ci);
                    if(!nname) nname = z_teamcolorname(actor, NULL, ci);

                    z_formattemplate ft[] =
                    {
                        { 'Y', "%s", (const void *)name },
                        { 'A', "%s", (const void *)nname },
                        { 's', "%s", (const void *)msg },
                        { 0, NULL, NULL }
                    };

                    string buf;
                    z_format(buf, sizeof(buf), announcekills_style_rampage, ft);
                    if(*buf) sendf(ci->clientnum, 1, "ris", N_SERVMSG, buf);
                }
            }
        }
    }
}
