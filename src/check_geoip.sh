#!/bin/sh

CXX=$*

$CXX -o /dev/null -x c++ - -lGeoIP 2>/dev/null << EOF
#include "GeoIP.h"
#include "GeoIPCity.h"
int main() { GeoIP *gi = NULL; GeoIP_delete(gi); return 0; }
EOF
if [ $? -eq 0 ]; then printf ' -DUSE_GEOIP'; fi

$CXX -o /dev/null -x c++ - -lmaxminddb 2>/dev/null << EOF
#include "maxminddb.h"
int main() { MMDB_s *m = NULL; MMDB_close(m); return 0; }
EOF
if [ $? -eq 0 ]; then printf ' -DUSE_MMDB'; fi

echo ''
unset CXX
