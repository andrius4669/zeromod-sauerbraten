#!/bin/sh

curl 'http://geolite.maxmind.com/download/geoip/database/GeoLite2-City.mmdb.gz' > GeoLite2-City.mmdb.gz.new
mv -f GeoLite2-City.mmdb.gz.new GeoLite2-City.mmdb.gz
gunzip -c GeoLite2-City.mmdb.gz > GeoLite2-City.mmdb.new
mv -f GeoLite2-City.mmdb.new GeoLite2-City.mmdb
rm -f GeoLite2-City.mmdb.gz
