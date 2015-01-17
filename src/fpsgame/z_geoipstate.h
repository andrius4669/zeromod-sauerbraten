#ifndef Z_GEOIPSTATE_H
#define Z_GEOIPSTATE_H

struct geoipstate
{
    char *network, *city, *region, *country, *continent;
    // 1 - anonymous proxy, 2 - satellite provider
    int anonymous;

    geoipstate(): network(NULL), city(NULL), region(NULL),
        country(NULL), continent(NULL), anonymous(0) {}
    geoipstate(const geoipstate &s): network(NULL), city(NULL), region(NULL),
        country(NULL), continent(NULL), anonymous(0) { *this = s; }
    ~geoipstate() { cleanup(); }

    void cleanup()
    {
        DELETEP(network);
        DELETEP(city);
        DELETEP(region);
        DELETEP(country);
        DELETEP(continent);
        anonymous = 0;
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
        }
        return *this;
    }
};

#endif // Z_GEOIPSTATE_H
