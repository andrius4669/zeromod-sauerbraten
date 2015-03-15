#!/bin/sh

rm -f GeoLite2-City.mmdb.gz
curl http://geolite.maxmind.com/download/geoip/database/GeoLite2-City.mmdb.gz -o GeoLite2-City.mmdb.gz
rm -f GeoLite2-City.mmdb
gunzip GeoLite2-City.mmdb.gz