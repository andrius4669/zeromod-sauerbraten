#ifndef Z_ANNOUNCEKILLS_H
#define Z_ANNOUNCEKILLS_H


VAR(announcekills, 0, 0, 1);

VAR(announcekills_stopped, 0, 1, 1);
VAR(announcekills_stopped_min, 0, 15, 100);
VAR(announcekills_stopped_num, 0, 0, 1);

VAR(announcekills_multikill, 0, 2, 2);

VAR(announcekills_rampage, 0, 1, 2);
VAR(announcekills_rampage_min, 0, 0, 100);
VAR(announcekills_numeric, 0, 0, 1);
VAR(announcekills_numeric_num, 1, 5, 100);

static const char *z_teamcolorname(clientinfo *ci, const char *alt, clientinfo *as)
{
    static string buf[3];
    static int bufpos = 0;
    const char *name = alt && ci==as ? colorname(ci, (char *)alt) : colorname(ci);
    if(!m_teammode) return name;
    bufpos = (bufpos+1)%3;
    formatstring(buf[bufpos])("\fs\f%c%s\fr", !strcmp(ci->team, as->team) ? '1' : '3', name);
    return buf[bufpos];
}

static const char *z_multikillstr(int i)
{
    switch(i)
    {
        case 2: return "a \f6DOUBLE ";
        case 3: return "a \f6TRIPLE ";
        case 4: return "a \f6QUAD-";
        case 5: return "a \f6PENTA-";
        case 6: return "a \f3HEXA-";
        case 7: return "a \f3HEPTA-";    // sounds weird, but meh
        case 8: return "an \f3OCTA-";
        default: return NULL;
    }
}

void z_announcekill(clientinfo *actor, clientinfo *victim, int fragval)
{
    if(fragval > 0)
    {
        if(!actor->state.lastkill || gamemillis-actor->state.lastkill > 2000) actor->state.multikills = 0;
        actor->state.lastkill = gamemillis ? gamemillis : 1;
        actor->state.multikills++;
        actor->state.rampage++;
    }
    if(m_edit || !announcekills || (!announcekills_rampage && !announcekills_multikill)) return;
    bool showactor = fragval > 0 && actor->state.state==CS_ALIVE;
    loopv(clients)
    {
        clientinfo *ci = clients[i];
        const char *name = NULL, *nname = NULL;

        if(ci->state.aitype!=AI_NONE) continue;

        if(announcekills_stopped && victim!=actor && victim->state.rampage >= announcekills_stopped_min)
        {
            const char *vname = z_teamcolorname(victim, NULL, ci);
            nname = z_teamcolorname(actor, NULL, ci);
            if(!announcekills_stopped_num) sendf(ci->clientnum, 1, "ris", N_SERVMSG, tempformatstring("\f2%s was stopped by %s", vname, nname));
            else sendf(ci->clientnum, 1, "ris", N_SERVMSG, tempformatstring("\f2%s was stopped by %s (\f6%d kills\f2)", vname, nname, victim->state.rampage));
        }

        if(showactor && announcekills_multikill && actor->state.multikills > 1 &&
            (announcekills_multikill == 1 || ci->clientnum == actor->clientnum))
        {
            name = z_teamcolorname(actor, "you", ci);
            const char *mkstr = z_multikillstr(actor->state.multikills);
            if(mkstr) sendf(ci->clientnum, 1, "ris", N_SERVMSG, tempformatstring("\f2%s scored %sKILL!!", name, mkstr));
            else sendf(ci->clientnum, 1, "ris", N_SERVMSG, tempformatstring("\f2%s scored \f3%d MULTIPLE KILLS!!", name, actor->state.multikills));
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
                    sendf(ci->clientnum, 1, "ris", N_SERVMSG, tempformatstring("\f2%s made \f7%d\f2 kills", name, actor->state.rampage));
                }
            }
            else
            {
                const char *msg;
                switch(actor->state.rampage)
                {
                    case 5:  msg = "on a KILLING SPREE!!"; break;
                    case 10: msg = "on a \f6RAMPAGE!!"; break;
                    case 15: msg = "\f6DOMINATING!!"; break;
                    case 20: msg = "\f6UNSTOPPABLE!!"; break;
                    case 30: msg = "\f6GODLIKE!!"; break;
                    default: msg = NULL; break;
                }
                if(msg)
                {
                    if(!nname) nname = z_teamcolorname(actor, NULL, ci);
                    sendf(ci->clientnum, 1, "ris", N_SERVMSG, tempformatstring("\f2%s is %s", nname, msg));
                }
            }
        }
    }
}

#endif // Z_ANNOUNCEKILLS_H
