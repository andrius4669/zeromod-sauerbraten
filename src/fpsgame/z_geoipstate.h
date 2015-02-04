#ifndef Z_GEOIPSTATE_H
#define Z_GEOIPSTATE_H

struct geoipstate
{
    char *network, *city, *region, *country, *continent;
    // 1 - anonymous proxy, 2 - satellite provider
    int anonymous;
    union shortconv
    {
        char b[2];
        short s;
        shortconv() {}
        shortconv(short s_): s(s_) {}
    } extcountry, extcontinent;

    geoipstate(): network(NULL), city(NULL), region(NULL), country(NULL), continent(NULL), anonymous(0), extcountry(0), extcontinent(0) {}
    geoipstate(const geoipstate &s): network(NULL), city(NULL), region(NULL), country(NULL), continent(NULL) { *this = s; }
    ~geoipstate() { cleanup(); }

    void cleanup()
    {
        DELETEP(network);
        DELETEP(city);
        DELETEP(region);
        DELETEP(country);
        DELETEP(continent);
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

    void setextinfo(const char *countrycode, const char *continent)
    {
        if(countrycode && countrycode[0] && countrycode[1] && !countrycode[2])
        {
            loopi(2) extcountry.b[i] = countrycode[i] < 'a' || countrycode[i] > 'z' ? countrycode[i] : countrycode[i] - 'a'-'A';
        }
        else
        {
            extcountry.s = 0;
        }
        if(continent && continent[0] && continent[1] && !continent[2])
        {
            loopi(2) extcontinent.b[i] = continent[i] >= 'A' && continent[i] <= 'Z' ? continent[i] + 'a'-'A' : continent[i];
        }
        else
        {
            extcontinent.s = 0;
        }
    }
};

#endif // Z_GEOIPSTATE_H
