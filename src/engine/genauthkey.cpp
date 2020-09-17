#include <stdio.h>
#include <stdint.h>
#include <sodium.h>

// including cpp file, cause we will use its internal structures
// cube.h is included by it automatically
#include "crypto.cpp"

enum { AM_SECRETPASS, AM_PRIVKEY, AM_RANDOMPASS, AM_RANDOMKEY };

// shared/stream.cpp depends on this
void conoutfv(int type, const char *fmt, va_list args)
{
    vfprintf(stdout, fmt, args);
    fputc('\n', stdout);
}

void printhelp(FILE *f, const char *progname)
{
    fprintf(f, "Usage:\n");
    fprintf(f, "\t%s [-s] secretpass\n", progname);
    fprintf(f, "\t%s  -p  privatekey\n", progname);
    fprintf(f, "\t%s  -r\n", progname);
    fprintf(f, "\t%s  -R\n", progname);
    fprintf(f,
            "generates authkey pair for sauerbraten auth system\n"
            "\t-h           \tthis help screen\n"
            "\t-s secretpass\tuse secret password to generate key pair\n"
            "\t-p privatekey\tuse private key to generate public key\n"
            "\t-r           \tgenerate with random password\n"
            "\t-R           \tgenerate with random private key\n"
    );
}

int main(int argc, const char **argv)
{
    int mode = AM_SECRETPASS;
    int verbose = 1;
    int ignoreargs = false;
    string seed = {0};

    if(argc <= 1)
    {
        printhelp(stderr, argv[0]);
        return 1;
    }

    for(int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];
        if(*arg == '-' && !ignoreargs)
        {
            ++arg;
            if(*arg == '-') ++arg;

            if (arg[0] == '\0') ignoreargs = true;
            else if (arg[1] == '\0')
            {
                switch(*arg)
                {
                case 's':
                    mode = AM_SECRETPASS;
                    break;

                case 'p':
                    mode = AM_PRIVKEY;
                    break;

                case 'r':
                    mode = AM_RANDOMPASS;
                    break;

                case 'R':
                    mode = AM_RANDOMKEY;
                    break;

                case 'q':
                    verbose--;
                    break;

                case 'v':
                    verbose++;
                    break;

                case 'h':
                    printhelp(stdout, argv[0]);
                    return 0;

                default:
                    fprintf(stderr, "unrecognized option '%s'\n", argv[i]);
                    printhelp(stderr, argv[0]);
                    return 1;
                }
            }
            else {
                if(!strcmp(arg, "help"))
                {
                    printhelp(stdout, argv[0]);
                    return 0;
                }
                else
                {
                    fprintf(stderr, "unrecognized option \"%s\"\n", arg);
                    printhelp(stderr, argv[0]);
                    return 1;
                }
            }
        }
        else
        {
            if(!seed[0])
            {
                copystring(seed, argv[i]);
                if(!seed[0])
                {
                    fprintf(stderr, "empty seed not allowed\n");
                    return 1;
                }
            }
            else
            {
                fprintf(stderr, "seed already specified\n");
                return 1;
            }
        }
    }

    if(!seed[0] && (mode == AM_SECRETPASS || mode == AM_PRIVKEY))
    {
        fprintf(stderr, "no seed specified!\n");
        return 1;
    }

    if(!seed[0])
    {
        if(sodium_init() < 0)
        {
            fprintf(stderr, "sodium_init() failed\n");
            return 1;
        }

        // generate random password
        if(mode == AM_RANDOMPASS)
        {
            // randomly select password length
            uint32_t s = 16 + randombytes_uniform(8);
            // characters used in password
            static const char characters[] =
                "0123456789"
                "ABCDEFGHIJKLMNOPQRSTUVXYZ"
                "abcdefghijklmnopqrstuvxyz"
                "!#$%&*+,-./<=>?@_`";
            const size_t sz = sizeof(characters)-1;
            char *p = seed;
            for(uint32_t i = 0; i < s; i++)
            {
                *p++ = characters[randombytes_uniform(sz)];
                *p++ = characters[randombytes_uniform(sz)];
            }
        }
        else
        {
            randombytes_buf(seed, sizeof(seed));
        }
    }

    tiger::hashval hash;
    // private key is just a hash of password
    bigint<8*sizeof(hash.bytes)/BI_DIGIT_BITS> privkey;

    if(mode != AM_PRIVKEY)
    {
        if(mode!=AM_RANDOMKEY && (mode==AM_RANDOMPASS || verbose>1))
        {
            printf("%s%s\n", verbose>0 ? "secure pass: " : "", seed);
        }

        if(mode != AM_RANDOMKEY) tiger::hash((const uchar *)seed, (int)strlen(seed), hash);
        else tiger::hash((const uchar *)seed, (int)sizeof(seed), hash);
        sodium_memzero(seed, sizeof(seed));

        memcpy(privkey.digits, hash.bytes, sizeof(hash.bytes));
        sodium_memzero(hash.bytes, sizeof(hash.bytes));

        privkey.len = 8*sizeof(hash.bytes)/BI_DIGIT_BITS;
        privkey.shrink();
    }
    else
    {
        privkey.parse(seed);
        sodium_memzero(seed, sizeof(seed));
    }

    if(mode != AM_PRIVKEY || verbose>1)
    {
        vector<char> privstr;
        privkey.printdigits(privstr);
        privstr.add('\0');

        printf("%s%s\n", verbose>0 ? "private key: " : "", privstr.getbuf());

        sodium_memzero(privstr.getbuf(), privstr.length());
    }


    ecjacobian c(ecjacobian::base);
    c.mul(privkey);
    sodium_memzero(privkey.digits, sizeof(privkey.digits));

    c.normalize();

    vector<char> pubstr;
    c.print(pubstr);
    pubstr.add('\0');

    printf("%s%s\n", verbose>0 ? "public key: " : "", pubstr.getbuf());

    return 0;
}
