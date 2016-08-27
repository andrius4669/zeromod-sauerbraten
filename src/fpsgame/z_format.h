#ifdef Z_FORMAT_H
#error "already z_format.h"
#endif
#define Z_FORMAT_H

struct z_formattemplate
{
    char format;
    const char *type;
    const void *ptr;
};

int z_format(char *dest, size_t maxlen, const char *fmt, const z_formattemplate *ft)
{
    char *start = dest, *limit = dest+maxlen-1;
    while(dest < limit)
    {
        char c = *fmt++;
        if(!c) break;
        if(c == '%')
        {
            c = *fmt++;
            if(!c) break;
            if(c != '%')
            {
                const z_formattemplate *ct;
                for(ct = ft; ct->format && ct->format != c; ct++);
                if(ct->format)
                {
                    int mlen, len;
                    mlen = limit-dest;
                    len = snprintf(dest, mlen+1, ct->type, ct->ptr);
                    if(len > mlen) len = mlen;
                    if(len > 0) dest += len;
                }
                continue;
            }
        }
        *dest++ = c;
    }
    *dest = 0;
    return dest-start;
}

static const struct { const char *name; int timediv; } z_timedivinfos[] =
{
    // month is inaccurate
    { "week", 60*60*24*7 },
    { "day", 60*60*24 },
    { "hour", 60*60 },
    { "minute", 60 },
    { "second", 1 }
};

void z_formatsecs(vector<char> &timebuf, uint secs)
{
    bool moded = false;
    const size_t tl = sizeof(z_timedivinfos)/sizeof(z_timedivinfos[0]);
    for(size_t i = 0; i < tl; i++)
    {
        uint t = secs / z_timedivinfos[i].timediv;
        if(!t && (i+1<tl || moded)) continue;
        secs %= z_timedivinfos[i].timediv;
        if(moded) timebuf.add(' ');
        moded = true;
        charbuf b = timebuf.reserve(10 + 1);
        int blen = b.remaining();
        int plen = snprintf(b.buf, blen, "%u", t);
        timebuf.advance(clamp(plen, 0, blen-1));
        timebuf.add(' ');
        timebuf.put(z_timedivinfos[i].name, strlen(z_timedivinfos[i].name));
        if(t != 1) timebuf.add('s');
        if(!secs) break;
    }
}

SVAR(date_format, "%Y-%m-%d %H:%M:%S");
VAR(date_local, 0, 1, 1);

size_t z_formatdate(char *dst, size_t len, const time_t &t)
{
    struct tm *tt = date_local ? localtime(&t) : gmtime(&t);
    return strftime(dst, len, date_format, tt);
}

static const char *formatmillisecs(int ms)
{
    int mins = ms / 60000;
    ms %= 60000;
    if(!mins) return tempformatstring("%d.%03d sec", ms/1000, ms%1000);
    else if(mins && ms) return tempformatstring("%d min %d.%03d sec", mins, ms/1000, ms%1000);
    else return tempformatstring("%d min", mins);
}
