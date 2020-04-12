maxminddb geolite2 db can be downloaded from <https://dev.maxmind.com/geoip/geoip2/geolite2/>.
that used to be easily downloadable via script but now requires account as per <https://blog.maxmind.com/2019/12/18/significant-changes-to-accessing-and-using-geolite2-databases/>.
for easy updating, install geoipupdate tool, adjust /etc/GeoIP.conf re account details, and add geoipupdate to root's crontab as per `man geoipupdate`.
then specify proper database path in geoip_mmdb setting.
`geoip_mmdb "/usr/share/GeoIP/GeoLite2-Country.mmdb"` or `geoip_mmdb "/usr/share/GeoIP/GeoLite2-City.mmdb"` for example (some systems use different path though).
for automatic usage of updated database, consider setting `geoip_reload 2` and `geoip_mmdb_poll 1`.
that should result in complete hands-off automatic updating.

alternatively, try <https://db-ip.com/db/lite.php>.
