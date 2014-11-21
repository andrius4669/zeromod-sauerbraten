#!/bin/sh

CXX=$*

cat > check_geoip.cpp << EOF
#include "GeoIP.h"
#include "GeoIPCity.h"

int main() { GeoIP *gi = NULL; GeoIP_delete(gi); return 0; }
EOF

$CXX check_geoip.cpp -o check_geoip "-lGeoIP" 2>/dev/null
if [ $? -eq 0 ]; then printf "1"; rm check_geoip; fi
echo ''
rm check_geoip.cpp
