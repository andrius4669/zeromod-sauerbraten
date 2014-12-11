#!/bin/sh

rm -f GeoIP.dat.gz
curl http://geolite.maxmind.com/download/geoip/database/GeoLiteCountry/GeoIP.dat.gz -o GeoIP.dat.gz
rm -f GeoIP.dat
gunzip GeoIP.dat.gz

rm -f GeoLiteCity.dat.gz
curl http://geolite.maxmind.com/download/geoip/database/GeoLiteCity.dat.gz -o GeoLiteCity.dat.gz
rm -f GeoLiteCity.dat
gunzip GeoLiteCity.dat.gz
