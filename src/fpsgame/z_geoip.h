#ifndef Z_GEOIP_H
#define Z_GEOIP_H

#include "z_geoipstate.h"


#ifdef USE_MMDB
#include "maxminddb.h"
static MMDB_s *z_mmdb = NULL;
#endif


#ifdef USE_GEOIP
#include "GeoIP.h"
#include "GeoIPCity.h"
static GeoIP *z_gi = NULL, *z_gic = NULL;
#endif


#ifdef USE_MMDB
static void z_reset_mmdb() { if(z_mmdb) { MMDB_close(z_mmdb); free(z_mmdb); z_mmdb = NULL; } }
#else
static inline void z_reset_mmdb() {}
#endif

#ifdef USE_GEOIP
static void z_reset_geoip_country() { if(z_gi) { GeoIP_delete(z_gi); z_gi = NULL; } }
#else
static inline void z_reset_geoip_country() {}
#endif

#ifdef USE_GEOIP
static void z_reset_geoip_city() { if(z_gic) { GeoIP_delete(z_gic); z_gic = NULL; } }
#else
static inline void z_reset_geoip_city() {}
#endif

static void z_reset_geoip()
{
    z_reset_mmdb();
    z_reset_geoip_country();
    z_reset_geoip_city();
}

VAR(geoip_reload, 0, 1, 1);
VARF(geoip_enable, 0, 0, 1, { if(geoip_reload) z_reset_geoip(); });
SVARF(geoip_mmdb,    "", { if(geoip_reload) z_reset_mmdb(); });
SVAR(geoip_mmdb_lang, "en");
SVARF(geoip_country, "", { if(geoip_reload) z_reset_geoip_country(); });
SVARF(geoip_city,    "", { if(geoip_reload) z_reset_geoip_city(); });

VAR(geoip_show_ip, 0, 2, 2);
VAR(geoip_show_network, 0, 1, 2);
VAR(geoip_show_city, 0, 0, 2);
VAR(geoip_show_region, 0, 0, 2);
VAR(geoip_show_country, 0, 1, 2);
VAR(geoip_show_continent, 0, 0, 2);
VAR(geoip_skip_duplicates, 0, 1, 2);
VAR(geoip_country_use_db, 0, 2, 2);
VAR(geoip_fix_country, 0, 1, 1);
VAR(geoip_ban_anonymous, 0, 0, 1);

static const struct
{
    uint ip, mask;      // in host byte order
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

#if defined(USE_GEOIP) || defined(USE_MMDB)
    static bool z_geoip_reset_atexit = false;
    if(!z_geoip_reset_atexit)
    {
        z_geoip_reset_atexit = true;
        atexit(z_reset_geoip);
    }
#endif

#ifdef USE_MMDB

    if(!z_mmdb && *geoip_mmdb)
    {
        const char *found = findfile(geoip_mmdb, "rb");
        if(found)
        {
            z_mmdb = (MMDB_s *)calloc(1, sizeof(MMDB_s));
            if(z_mmdb)
            {
                int s = MMDB_open(found, MMDB_MODE_MMAP, z_mmdb);
                if(s != MMDB_SUCCESS)
                {
                    free(z_mmdb);
                    z_mmdb = NULL;
                    logoutf("WARNING: could not open mmdb database \"%s\": %s", geoip_mmdb, MMDB_strerror(s));
                }
            }
        }
        else logoutf("WARNING: could not find mmdb database \"%s\"", geoip_mmdb);
    }

#else

    if(*geoip_mmdb)
    {
        static bool warned = false;
        if(!warned)
        {
            logoutf("WARNING: libmaxminddb support was not compiled in");
            warned = true;
        }
    }

#endif

#ifdef USE_GEOIP

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
    if(!z_gi && *geoip_country)
    {
        const char *found = findfile(geoip_country, "rb");
        if(found) z_gi = GeoIP_open(found, GEOIP_COUNTRY_OPENMODE);
        if(z_gi) GeoIP_set_charset(z_gi, GEOIP_CHARSET_UTF8);
        else logoutf("WARNING: could not open geoip country database \"%s\"", geoip_country);
    }

    #ifndef GEOIP_CITY_OPENMODE
        #define GEOIP_CITY_OPENMODE GEOIP_OPENMODE
    #endif
    if(!z_gic && *geoip_city)
    {
        const char *found = findfile(geoip_city, "rb");
        if(found) z_gic = GeoIP_open(found, GEOIP_CITY_OPENMODE);
        if(z_gic) GeoIP_set_charset(z_gic, GEOIP_CHARSET_UTF8);
        else logoutf("WARNING: could not open geoip city database \"%s\"", geoip_city);
    }

    #undef GEOIP_OPENMODE
    #undef GEOIP_COUNTRY_OPENMODE
    #undef GEOIP_CITY_OPENMODE

#else

    if(*geoip_country || *geoip_city)
    {
        static bool warned = false;
        if(!warned)
        {
            logoutf("WARNING: libGeoIP support was not compiled in");
            warned = true;
        }
    }

#endif
}

#if defined(USE_GEOIP) || defined(USE_MMDB)
static const char *z_geoip_decode_continent(const char *code)
{
    /**/ if(code[0] == 'A' && code[1] == 'F') return "Africa";
    else if(code[0] == 'A' && code[1] == 'S') return "Asia";
    else if(code[0] == 'E' && code[1] == 'U') return "Europe";
    else if(code[0] == 'N' && code[1] == 'A') return "North America";
    else if(code[0] == 'O' && code[1] == 'C') return "Oceania";
    else if(code[0] == 'S' && code[1] == 'A') return "South America";
    else return NULL;
}
#endif

short z_geoip_getextinfo(geoipstate &gs)
{
    return geoip_show_country == 1 && gs.extcountry.s ? gs.extcountry.s : (geoip_show_country == 1 ||  geoip_show_continent == 1) ? gs.extcontinent.s : 0;
}

void z_geoip_resolveclient(geoipstate &gs, enet_uint32 ip)
{
    gs.cleanup();
    if(!geoip_enable || !ip) return;
    z_init_geoip();
    enet_uint32 hip = ENET_NET_TO_HOST_32(ip);
    // look in list of reserved ips
    loopi(sizeof(reservedips)/sizeof(reservedips[0])) if((hip & reservedips[i].mask) == reservedips[i].ip)
    {
        gs.network = newstring(reservedips[i].name);
        // if ip is reserved, geoip won't find it anyway
        return;
    }

#if defined(USE_GEOIP) || defined(USE_MMDB)
    uchar buf[MAXSTRLEN];
    size_t len;

    const char *country_code = NULL, *continent_code = NULL;
#endif

#ifdef USE_MMDB
    if(z_mmdb)
    {
        struct sockaddr_in sin;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        memcpy(&sin.sin_addr, &ip, sizeof(enet_uint32));

        int mmdb_error;
        MMDB_lookup_result_s result = MMDB_lookup_sockaddr(z_mmdb, (struct sockaddr *)&sin, &mmdb_error);
        if(mmdb_error == MMDB_SUCCESS && result.found_entry)
        {
            MMDB_entry_data_s data;

            // continent code
            mmdb_error = MMDB_get_value(&result.entry, &data, "continent", "code", NULL);
            if(mmdb_error == MMDB_SUCCESS && data.has_data && data.type == MMDB_DATA_TYPE_UTF8_STRING && data.data_size >= 2)
                continent_code = data.utf8_string;

            // country code
            mmdb_error = MMDB_get_value(&result.entry, &data, "country", "iso_code", NULL);
            if(mmdb_error == MMDB_SUCCESS && data.has_data && data.type == MMDB_DATA_TYPE_UTF8_STRING && data.data_size >= 2)
                country_code = data.utf8_string;

            // save em
            gs.setextinfo(country_code, continent_code);


            // continent name
            mmdb_error = MMDB_get_value(&result.entry, &data, "continent", "names", geoip_mmdb_lang, NULL);
            if((mmdb_error != MMDB_SUCCESS || !data.has_data) && strcmp(geoip_mmdb_lang, "en"))
                mmdb_error = MMDB_get_value(&result.entry, &data, "continent", "names", "en", NULL);
            if(mmdb_error == MMDB_SUCCESS && data.has_data && data.type == MMDB_DATA_TYPE_UTF8_STRING)
            {
                len = decodeutf8(buf, sizeof(buf)-1, (const uchar *)data.utf8_string, data.data_size);
                if(len > 0) { buf[len] = 0; gs.continent = newstring((const char *)buf); }
            }
            else if(continent_code)
            {
                const char *contname = z_geoip_decode_continent(continent_code);
                if(contname) gs.continent = newstring(contname);
            }


            // country name
            mmdb_error = MMDB_get_value(&result.entry, &data, "country", "names", geoip_mmdb_lang, NULL);
            if((mmdb_error != MMDB_SUCCESS || !data.has_data) && strcmp(geoip_mmdb_lang, "en"))
                mmdb_error = MMDB_get_value(&result.entry, &data, "country", "names", "en", NULL);
            if(mmdb_error == MMDB_SUCCESS && data.has_data && data.type == MMDB_DATA_TYPE_UTF8_STRING)
            {
                len = decodeutf8(buf, sizeof(buf)-1, (const uchar *)data.utf8_string, data.data_size);
                if(len > 0)
                {
                    buf[len] = 0;
                    gs.country = newstring((const char *)buf);
                    const char *comma;
                    if(geoip_fix_country && (comma = strstr(gs.country, ", ")))
                    {
                        vector<char> countrybuf;
                        const char * const aftercomma = &comma[2];
                        countrybuf.put(aftercomma, strlen(aftercomma));
                        if(countrybuf.length()) countrybuf.add(' ');
                        countrybuf.put(gs.country, comma - gs.country);
                        countrybuf.add('\0');
                        delete[] gs.country;
                        gs.country = newstring(countrybuf.getbuf());
                    }
                }
            }


            // region
            if(geoip_show_region)
            {
                mmdb_error = MMDB_get_value(&result.entry, &data, "subdivisions", "0", "names", geoip_mmdb_lang, NULL);
                if((mmdb_error != MMDB_SUCCESS || !data.has_data) && strcmp(geoip_mmdb_lang, "en"))
                    mmdb_error = MMDB_get_value(&result.entry, &data, "subdivisions", "0", "names", "en", NULL);
                if(mmdb_error == MMDB_SUCCESS && data.has_data && data.type == MMDB_DATA_TYPE_UTF8_STRING)
                {
                    len = decodeutf8(buf, sizeof(buf)-1, (const uchar *)data.utf8_string, data.data_size);
                    if(len > 0) { buf[len] = 0; gs.region = newstring((const char *)buf); }
                }
            }


            // city
            if(geoip_show_city)
            {
                mmdb_error = MMDB_get_value(&result.entry, &data, "city", "names", geoip_mmdb_lang, NULL);
                if((mmdb_error != MMDB_SUCCESS || !data.has_data) && strcmp(geoip_mmdb_lang, "en"))
                    mmdb_error = MMDB_get_value(&result.entry, &data, "city", "names", "en", NULL);
                if(mmdb_error == MMDB_SUCCESS && data.has_data && data.type == MMDB_DATA_TYPE_UTF8_STRING)
                {
                    len = decodeutf8(buf, sizeof(buf)-1, (const uchar *)data.utf8_string, data.data_size);
                    if(len > 0) { buf[len] = 0; gs.city = newstring((const char *)buf); }
                }
            }


            // anonymous proxy?
            mmdb_error = MMDB_get_value(&result.entry, &data, "traits", "is_anonymous_proxy", NULL);
            if(mmdb_error == MMDB_SUCCESS && data.has_data)
            {
                // expects true
                gs.anonymous = 1;
                gs.network = newstring("Anonymous Proxy");
            }

            return;
        }
    }
#endif // USE_MMDB

#ifdef USE_GEOIP
    const char *country_name = NULL;
    int country_id = -1;
    GeoIPRecord *gir = NULL;

    if(z_gi)
    {
        int id = GeoIP_id_by_ipnum(z_gi, hip);
        if(id >= 0) country_id = id;
    }
    if(z_gic && (geoip_show_city || geoip_show_region || country_id <= 0 || geoip_country_use_db))
    {
        gir = GeoIP_record_by_ipnum(z_gic, hip);
    }

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

    gs.setextinfo(country_code, continent_code);

    if(geoip_show_continent && continent_code && continent_code[0])
    {
        const char *continent_name = z_geoip_decode_continent(continent_code);
        if(continent_name) gs.continent = newstring(continent_name);
    }

    const char *network_name = NULL;
    // process some special country values
    if(country_code)
    {
        if(country_code[0] == 'A' && country_code[1] > '0' && country_code[1] <= '9')   // anonymous network
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
