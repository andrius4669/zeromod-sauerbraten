maxminddb geolite2 db can be downloaded from <https://dev.maxmind.com/geoip/geoip2/geolite2/>.
that used to be easily downloadable via script but now requires account as per <https://blog.maxmind.com/2019/12/18/significant-changes-to-accessing-and-using-geolite2-databases/>.
for easy updating, install geoipupdate tool, adjust /etc/GeoIP.conf re account details, and add geoipupdate to root's crontab as per `man geoipupdate`.
then specify proper database path in geoip_mmdb setting.
`geoip_mmdb "/var/lib/GeoIP/GeoLite2-Country.mmdb"` or `geoip_mmdb "/var/lib/GeoIP/GeoLite2-City.mmdb"` for example.

alternatively, try <https://db-ip.com/db/lite.php>.
