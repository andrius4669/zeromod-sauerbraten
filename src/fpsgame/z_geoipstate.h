#ifdef Z_GEOIPSTATE_H
#error "already z_geoipstate.h"
#endif
#define Z_GEOIPSTATE_H

union geoshortconv
{
    char b[2];
    short s;

    geoshortconv() {}
    explicit geoshortconv(short s_): s(s_) {}

    void set_upper(const char *code)
    {
        if(code && code[0] && code[1]) { loopi(2) b[i] = toupper(code[i]); }
        else s = 0;
    }

    void set_lower(const char *code)
    {
        if(code && code[0] && code[1]) { loopi(2) b[i] = tolower(code[i]); }
        else s = 0;
    }
};

struct geoipstate
{
    char *network, *city, *region, *country, *continent;
    // 1 - anonymous proxy, 2 - satellite provider
    int anonymous;
    geoshortconv extcountry, extcontinent;

    geoipstate(): network(NULL), city(NULL), region(NULL), country(NULL), continent(NULL), anonymous(0), extcountry(0), extcontinent(0) {}
    geoipstate(const geoipstate &s): network(NULL), city(NULL), region(NULL), country(NULL), continent(NULL) { *this = s; }
    ~geoipstate() { cleanup(); }

    void cleanup()
    {
        DELETEA(network);
        DELETEA(city);
        DELETEA(region);
        DELETEA(country);
        DELETEA(continent);
        anonymous = 0;
        extcountry.s = 0;
        extcontinent.s = 0;
    }

    geoipstate &operator =(const geoipstate &s)
    {
        if(&s != this)
        {
            cleanup();
            if(s.network) network = newstring(s.network);
            if(s.city) city = newstring(s.city);
            if(s.region) region = newstring(s.region);
            if(s.country) country = newstring(s.country);
            if(s.continent) continent = newstring(s.continent);
            anonymous = s.anonymous;
            extcountry = s.extcountry;
            extcontinent = s.extcontinent;
        }
        return *this;
    }

    void setextinfo(const char *countrycode, const char *continentcode)
    {
        extcountry.set_upper(countrycode);
        extcontinent.set_lower(continentcode);
    }
};
