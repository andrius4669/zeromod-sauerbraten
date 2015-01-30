#ifndef Z_FORMAT_H
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
                    continue;
                }
            }
        }
        *dest++ = c;
    }
    *dest = 0;
    return dest-start;
}

#endif // Z_FORMAT_H
