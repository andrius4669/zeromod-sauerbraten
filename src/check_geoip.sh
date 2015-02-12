#!/bin/sh

CXX=$*

$CXX -o /dev/null -x c++ '-lGeoIP' '-' 2>/dev/null << EOF
#include "GeoIP.h"
#include "GeoIPCity.h"
int main() { GeoIP *gi = NULL; GeoIP_delete(gi); return 0; }
EOF
if [ $? -eq 0 ]; then printf '1'; fi
echo ''
unset CXX
