#!/bin/sh

getgeoip ()
{
	curl "$2"/"$1".gz > "$1".gz.new
	mv -f "$1".gz.new "$1".gz
	gunzip -c "$1".gz > "$1".new
	mv -f "$1".new "$1"
	rm -f "$1".gz
}

getgeoip 'GeoIP.dat' 'http://geolite.maxmind.com/download/geoip/database/GeoLiteCountry'
getgeoip 'GeoLiteCity.dat' 'http://geolite.maxmind.com/download/geoip/database'
