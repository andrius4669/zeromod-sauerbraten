#ifdef Z_RECORDS_H
#error "already z_records.h"
#endif
#define Z_RECORDS_H

#ifndef Z_FORMAT_H
#error "want z_format.h"
#endif


VAR(record_unnamed, 0, 0, 1); // whether to record unnamed players or not
VAR(record_expire, 0, 0, INT_MAX); // expire time, in milliseconds
VAR(record_min, 0, 0, INT_MAX); // shortest recordable race win time, in milliseconds
VAR(record_max, 0, 0, INT_MAX); // longest recordable race win time, in milliseconds. 0 = disable
// NOTE record times are not saved in HDD in any way currently
SVAR(record_style_new, "\f6NEW RECORD: \f7%C (%t)");
SVAR(record_style_newold, "\f6NEW RECORD: \f7%C (%t) \f2(old: \f7%O (%o)\f2)");

struct z_record
{
    char *map;
    char *name;
    char *ip;
    int mode;
    int timestamp;
    int time;
};
vector<z_record> z_records;
static int z_currentrecord = -1; // -1 = not yet found, -2 = comfirmed not found

static void z_checkexpiredrecords();

// finds current record + expire old records
static void z_findcurrentrecord()
{
    z_checkexpiredrecords(); // expire old records
    if(z_currentrecord != -1) return; // if comfirmed found or not found, do not attempt to search again
    loopv(z_records) if(z_records[i].mode == gamemode && !strcmp(z_records[i].map, smapname))
    {
        z_currentrecord = i;
        return;
    }
    z_currentrecord = -2;
}

// resets z_currentrecord pointer (to use when switching map, etc)
static void z_resetcurrentrecord()
{
    z_currentrecord = -1;
}

#if 0 // unused
// clears all records
static void z_clearallrecords()
{
    loopv(z_records)
    {
        DELETEA(z_records[i].map);
        DELETEA(z_records[i].name);
        DELETEA(z_records[i].ip);
    }
    z_records.setsize(0);
    z_currentrecord = -2;
}
#endif

// clears records of specific mode
static void z_clearrecords(int mode)
{
    loopv(z_records) if(z_records[i].mode == mode)
    {
        DELETEA(z_records[i].map);
        DELETEA(z_records[i].name);
        DELETEA(z_records[i].ip);
        z_records.remove(i);
        if(z_currentrecord > i) z_currentrecord--;
        else if(z_currentrecord == i) z_currentrecord = -2;
        i--;
    }
}

// clears old records
static void z_checkexpiredrecords()
{
    if(!record_expire) return;
    int i;
    for(i = 0; i < z_records.length() && (totalmillis - z_records[i].timestamp) >= record_expire; i++)
    {
        DELETEA(z_records[i].map);
        DELETEA(z_records[i].name);
        DELETEA(z_records[i].ip);
    }
    z_records.remove(0, i);
    if(z_currentrecord >= i) z_currentrecord -= i;
    else if(z_currentrecord >= 0) z_currentrecord = -2;
}

// maybe register and possibly broadcast new record
static void z_newrecord(clientinfo *ci, int time)
{
    // check if we should register this
    if(ci->state.aitype != AI_NONE) return; // bot
    if(!ci->name[0] || (!record_unnamed && !strcmp(ci->name, "unnamed"))) return;
    if(time <= record_min) return;
    if(record_max && time >= record_max) return;
    // find existing record
    z_findcurrentrecord();
    if(z_currentrecord >= 0)
    {
        z_record &r = z_records[z_currentrecord];
        if(r.time < time) return; // do nothing if record is not improved
        // print message now because later we won't have this data
        z_formattemplate ft[] =
        {
            { 'C', "%s", (const void *)colorname(ci) },
            { 'c', "%s", (const void *)ci->name },
            { 'n', "%d", (const void *)(intptr_t)ci->clientnum },
            { 't', "%s", (const void *)formatmillisecs(time) },
            { 'O', "%s", (const void *)r.name },
            { 'o', "%s", (const void *)formatmillisecs(r.time) },
            { 0, NULL, NULL }
        };
        string buf;
        z_format(buf, sizeof(buf), record_style_newold, ft);
        if(*buf) sendservmsg(buf);
        // remove it
        DELETEA(r.map);
        DELETEA(r.name);
        DELETEA(r.ip);
        z_records.remove(z_currentrecord);
    }
    else
    {
        z_formattemplate ft[] =
        {
            { 'C', "%s", (const void *)colorname(ci) },
            { 'c', "%s", (const void *)ci->name },
            { 'n', "%d", (const void *)(intptr_t)ci->clientnum },
            { 't', "%s", (const void *)formatmillisecs(time) },
            { 'O', "%s", (const void *)"" },
            { 'o', "%s", (const void *)"" },
            { 0, NULL, NULL }
        };
        string buf;
        z_format(buf, sizeof(buf), record_style_new, ft);
        if(*buf) sendservmsg(buf);
    }

    // save new record
    z_currentrecord = z_records.length();
    z_record &r = z_records.add();
    r.map = newstring(smapname);
    r.name = newstring(ci->name);
    r.ip = newstring(getclienthostname(ci->clientnum));
    r.mode = gamemode;
    r.timestamp = totalmillis;
    r.time = time;
}

static void z_servcmd_showrecord(int argc, char **argv, int sender)
{
    int r = -1;
    const char *mn = argc > 1 ? argv[1] : smapname;
    if(mn == smapname || !strcmp(mn, smapname))
    {
        // fast case
        z_findcurrentrecord();
        r = z_currentrecord;
    }
    else
    {
        loopv(z_records)
        {
            if(z_records[i].mode == gamemode && !strcmp(z_records[i].map, mn)) { r = i; break; }
        }
    }
    if(r < 0)
    {
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("record for \"%s\" not found", mn));
        return;
    }
    z_record &ro = z_records[r];
    clientinfo *ci = (clientinfo *)getclientinfo(sender);
    // TODO maybe expose IP to master/auth (should be configured with some variable)
    if(ci && (ci->privilege >= PRIV_ADMIN || ci->local))
    {
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("record for \"%s\": %s [%s] (%s)", mn, ro.name, formatmillisecs(ro.time), ro.ip ? ro.ip : "unknown"));
    }
    else sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("record for \"%s\": %s [%s]", mn, ro.name, formatmillisecs(ro.time)));
}
SCOMMANDA(showrecord, PRIV_NONE, z_servcmd_showrecord, 1);

static void z_servcmd_delrecord(int argc, char **argv, int sender)
{
    int r = -1;
    const char *mn = argc > 1 ? argv[1] : smapname;
    if(mn == smapname || !strcmp(mn, smapname))
    {
        // fast case
        z_findcurrentrecord();
        r = z_currentrecord;
    }
    else
    {
        loopv(z_records)
        {
            if(z_records[i].mode == gamemode && !strcmp(z_records[i].map, mn)) { r = i; break; }
        }
    }
    if(r < 0)
    {
        sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("record for \"%s\" does not exist", mn));
        return;
    }
    DELETEA(z_records[r].map);
    DELETEA(z_records[r].name);
    DELETEA(z_records[r].ip);
    z_records.remove(r);
    if(z_currentrecord > r) z_currentrecord--;
    else if(z_currentrecord == r) z_currentrecord = -2;
    sendf(sender, 1, "ris", N_SERVMSG, tempformatstring("record for \"%s\" removed", mn));
}
SCOMMANDA(delrecord, PRIV_ADMIN, z_servcmd_delrecord, 1);

static void z_servcmd_clearrecords(int argc, char **argv, int sender)
{
    z_clearrecords(gamemode);
    sendf(sender, 1, "ris", N_SERVMSG, "cleared all records for current gamemode");
}
SCOMMANDA(clearrecords, PRIV_ADMIN, z_servcmd_clearrecords, 1);
