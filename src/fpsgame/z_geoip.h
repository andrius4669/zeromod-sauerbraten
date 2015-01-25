#ifndef Z_GEOIP_H
#define Z_GEOIP_H

#include "z_geoipstate.h"

#ifdef USE_GEOIP

#include "GeoIP.h"
#include "GeoIPCity.h"

static GeoIP *z_gi = NULL, *z_gic = NULL;

#endif // USE_GEOIP

VAR(geoip_reload, 0, 1, 1);

static void z_reset_geoip_country()
{
#ifdef USE_GEOIP
    if(z_gi) { GeoIP_delete(z_gi); z_gi = NULL; }
#endif
}

static void z_reset_geoip_city()
{
#ifdef USE_GEOIP
    if(z_gic) { GeoIP_delete(z_gic); z_gic = NULL; }
#endif
}

static void z_reset_geoip()
{
    z_reset_geoip_country();
    z_reset_geoip_city();
}

VARF(geoip_enable, 0, 0, 1, { if(geoip_reload) z_reset_geoip(); });
VARF(geoip_country_enable, 0, 1, 1, { if(geoip_reload) z_reset_geoip_country(); });
SVARF(geoip_country_database, "GeoIP.dat", { if(geoip_reload) z_reset_geoip_country(); });
VARF(geoip_city_enable, 0, 0, 1, { if(geoip_reload) z_reset_geoip_city(); });
SVARF(geoip_city_database, "GeoLiteCity.dat", { if(geoip_reload) z_reset_geoip_city(); });

VAR(geoip_show_ip, 0, 2, 2);
VAR(geoip_show_network, 0, 1, 2);
VAR(geoip_show_city, 0, 0, 2);
VAR(geoip_show_region, 0, 0, 2);
VAR(geoip_show_country, 0, 1, 2);
VAR(geoip_show_continent, 0, 0, 2);
VAR(geoip_skip_duplicates, 0, 1, 2);
VAR(geoip_country_use_db, 0, 0, 2);
VAR(geoip_fix_country, 0, 1, 1);
SVAR(geoip_color_scheme, "777");
VAR(geoip_ban_anonymous, 0, 0, 1);

static const struct
{
    uint ip, mask;
    const char *name;
} reservedips[] =
{
    // localhost
    { 0x7F000000, 0xFF000000, "localhost" },            // 127.0.0.0/8
    // lan
    { 0xC0A80000, 0xFFFF0000, "Local Area Network" },   // 192.168.0.0/16
    { 0x0A000000, 0xFF000000, "Local Area Network" },   // 10.0.0.0/8
    { 0x64400000, 0xFFC00000, "Local Area Network" },   // 100.64.0.0/10
    { 0xAC100000, 0xFFF00000, "Local Area Network" },   // 172.16.0.0/12
    { 0xC0000000, 0xFFFFFFF8, "Local Area Network" },   // 192.0.0.0/29
    { 0xC6120000, 0xFFFE0000, "Local Area Network" },   // 198.18.0.0/15
    // autoconfigured lan
    { 0xA9FE0000, 0xFFFF0000, "Local Area Network" }    // 169.254.0.0/16
};

static void z_init_geoip()
{
    if(!geoip_enable) return;
#ifdef USE_GEOIP
    static bool z_geoip_reset_atexit = false;
    if(!z_geoip_reset_atexit)
    {
        z_geoip_reset_atexit = true;
        atexit(z_reset_geoip);
    }
    #ifndef GEOIP_OPENMODE
        #ifndef _WIN32
            #define GEOIP_OPENMODE GEOIP_MMAP_CACHE
        #else
            #define GEOIP_OPENMODE GEOIP_INDEX_CACHE
        #endif
    #endif
    #ifndef GEOIP_COUNTRY_OPENMODE
        #define GEOIP_COUNTRY_OPENMODE GEOIP_OPENMODE
    #endif
    #ifndef GEOIP_CITY_OPENMODE
        #define GEOIP_CITY_OPENMODE GEOIP_OPENMODE
    #endif
    if(geoip_country_enable && geoip_country_database[0] && !z_gi)
    {
        const char *found = findfile(geoip_country_database, "rb");
        if(found) z_gi = GeoIP_open(found, GEOIP_COUNTRY_OPENMODE);
        if(z_gi) GeoIP_set_charset(z_gi, GEOIP_CHARSET_UTF8);
        else logoutf("WARNING: could not open geoip country database file \"%s\"", geoip_country_database);
    }
    if(geoip_city_enable && geoip_city_database[0] && !z_gic)
    {
        const char *found = findfile(geoip_city_database, "rb");
        if(found) z_gic = GeoIP_open(found, GEOIP_CITY_OPENMODE);
        if(z_gic) GeoIP_set_charset(z_gic, GEOIP_CHARSET_UTF8);
        else logoutf("WARNING: could not open geoip city database file \"%s\"", geoip_city_database);
    }
    #undef GEOIP_OPENMODE
    #undef GEOIP_COUNTRY_OPENMODE
    #undef GEOIP_CITY_OPENMODE
#else // USE_GEOIP
    if(geoip_country_enable || geoip_city_enable) logoutf("WARNING: GeoIP library support was not compiled in");
#endif // USE_GEOIP
}

#ifdef USE_GEOIP
static const char *z_geoip_decode_continent(const char *cont)
{
    if(cont[0] == 'A' && cont[1] == 'F') return "Africa";
    else if(cont[0] == 'A' && cont[1] == 'S') return "Asia";
    else if(cont[0] == 'E' && cont[1] == 'U') return "Europe";
    else if(cont[0] == 'N' && cont[1] == 'A') return "North America";
    else if(cont[0] == 'O' && cont[1] == 'C') return "Oceania";
    else if(cont[0] == 'S' && cont[1] == 'A') return "South America";
    else return NULL;
}
#endif

void z_geoip_resolveclient(geoipstate &gs, enet_uint32 ip)
{
    gs.cleanup();
    if(!geoip_enable) return;
    z_init_geoip();
    ip = ENET_NET_TO_HOST_32(ip);   // geoip uses host byte order
    if(!ip) return;
    // look in list of reserved ips
    loopi(sizeof(reservedips)/sizeof(reservedips[0])) if((ip & reservedips[i].mask) == reservedips[i].ip)
    {
        gs.network = newstring(reservedips[i].name);
        // if ip is reserved, geoip won't find it anyway
        return;
    }

#ifdef USE_GEOIP
    int country_id = -1;
    GeoIPRecord *gir = NULL;

    if(z_gi)
    {
        int id = GeoIP_id_by_ipnum(z_gi, ip);
        if(id >= 0) country_id = id;
    }
    if(z_gic && (geoip_show_city || geoip_show_region || (country_id <= 0 || geoip_country_use_db)))
    {
        gir = GeoIP_record_by_ipnum(z_gic, ip);
    }

    const char *country_name = NULL, *country_code = NULL, *continent_code = NULL;
    switch(geoip_country_use_db)
    {
        case 0:
            /* use country db when avaiable */
            if(z_gi && country_id > 0)
            {
                continent_code = GeoIP_continent_by_id(country_id);
                country_code = GeoIP_code_by_id(country_id);
                country_name = GeoIP_country_name_by_id(z_gi, country_id);
            }
            /* use city db if needed */
            if(gir && !country_name)
            {
                continent_code = gir->continent_code;
                country_code = gir->country_code;
                country_name = gir->country_name;
            }
            break;
        case 1:
            /* first use city db if possible */
            if(gir)
            {
                continent_code = gir->continent_code;
                country_code = gir->country_code;
                country_name = gir->country_name;
            }
            /* use country db if needed */
            if(z_gi && country_id > 0 && !country_name)
            {
                continent_code = GeoIP_continent_by_id(country_id);
                country_code = GeoIP_code_by_id(country_id);
                country_name = GeoIP_country_name_by_id(z_gi, country_id);
            }
            break;
        case 2:
            /* if both country and city datas exists, and city one doesn't matches country one, drop city data */
            if(z_gi && country_id > 0)
            {
                continent_code = GeoIP_continent_by_id(country_id);
                country_code = GeoIP_code_by_id(country_id);
                country_name = GeoIP_country_name_by_id(z_gi, country_id);
            }
            if(gir)
            {
                if(country_name)
                {
                    if(strcmp(country_code, gir->country_code))
                    {
                        GeoIPRecord_delete(gir);
                        gir = NULL;
                    }
                }
                else
                {
                    continent_code = gir->continent_code;
                    country_code = gir->country_code;
                    country_name = gir->country_name;
                }
            }
            break;
    }

    uchar buf[MAXSTRLEN];
    size_t len;

    if(geoip_show_continent && continent_code && continent_code[0])
    {
        const char *continent_name = z_geoip_decode_continent(continent_code);
        if(continent_name) gs.continent = newstring(continent_name);
    }

    const char *network_name = NULL;
    // process some special country values
    if(country_code)
    {
        if(country_code[0] == 'A' && country_code[1] >= '0' && country_code[1] <= '9')  // anonymous network
        {
            gs.anonymous = country_code[1]-'0';
            network_name = country_name;
            country_name = NULL;
        }
        if(country_code[0] == 'O' && country_code[1] >= '0' && country_code[1] <= '9')  // marked as "Other"
        {
            country_name = NULL;    // "connected from Other".... naaah
        }
    }

    if(geoip_show_country && country_name)
    {
        vector<char> countrybuf;
        const char *comma;
        if(geoip_fix_country && (comma = strstr(country_name, ", ")))
        {
            const char * const aftercomma = &comma[2];
            countrybuf.put(aftercomma, strlen(aftercomma));
            if(countrybuf.length()) countrybuf.add(' ');
            countrybuf.put(country_name, comma - country_name);
            countrybuf.add('\0');
            country_name = countrybuf.getbuf();
        }
        if(country_name[0])
        {
            len = decodeutf8(buf, sizeof(buf)-1, (const uchar *)country_name, strlen(country_name));
            if(len > 0) { buf[len] = '\0'; gs.country = newstring((const char *)buf); }
        }
    }

    if(gir)
    {
        if(geoip_show_region && gir->region)
        {
            const char *region = GeoIP_region_name_by_code(gir->country_code, gir->region);
            if(region)
            {
                len = decodeutf8(buf, sizeof(buf)-1, (const uchar *)region, strlen(region));
                if(len > 0) { buf[len] = '\0'; gs.region = newstring((const char *)buf); }
            }
        }
        if(geoip_show_city && gir->city)
        {
            len = decodeutf8(buf, sizeof(buf)-1, (const uchar *)gir->city, strlen(gir->city));
            if(len > 0) { buf[len] = '\0'; gs.city = newstring((const char *)buf); }
        }
    }

    if(network_name && network_name[0])
    {
        // not expecting to get utf8 encoded string
        gs.network = newstring(network_name);
    }

    if(gir) GeoIPRecord_delete(gir);
#endif // USE_GEOIP
}

#endif // Z_GEOIP_H
