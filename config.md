## available config options

### engine/server
* **serverip** <**string**> - IP to bind to. defaults to `"0.0.0.0"`.
* **serverport** <**int**> - port to bind to. defaults to `28785`.
* **maxclients** <**int**> - maximum amount of clients to be allowed to join the server.\
  defaults to `8`. setting to `0` resets to the default.
* **maxdupclients** <**int**> - max clients from the same IP allowed to connect.
* **maxslots** <**int**> - maximum enet slots override.
* **nolansock** <**bool**> - disables listening on separate port for LAN autodetection.
* **serveruprate** <**int**> - upload speed limit.
* **logtimestamp** <**select**> - append timestamp to log lines.
  * `0` (the default) - don't
  * `1` - in `%Y-%m-%d_%H:%M:%S` format
  * `2` - in `%Y-%m-%dT%H:%M:%S` (ISO) format
* **updatemaster** <**bool**> - whether to register to masterserver (if disabled, will still allow to use auth).
* **mastername** <**string**> - masterserver hostname / IP address.
* **masterport** <**int**> - masterserver port.
* **serveracceptstdin** <**bool**> - whether server process should accept cubescript commands from standard input.

### client as server
* **startlistenserver** <**bool**> - client command to start local server. optional argument which defaults to `0` specifies whether to register to masterserver.
* **stoplistenserver** - client command to stop local server.
* **localconnect** - connect to local server.

## fpsgame/server
* **lockmaprotation** <**select**> - map rotation locking mode.
  * `0` (the default) - allow selecting map not in map rotation
  * `1` - allow selecting map not in map rotation only for master
  * `1` - allow selecting map not in map rotation only for admin
* **restrictmapvote** <**select**> - map voting restriction mode.
  * `0` (the default) - allow anyone to vote for the map
  * `1` - only allow masters to vote for the map
  * `2` - only allow admins to vote for the map
* **maprotationreset** - clear map rotation.
* **maprotation** <**string list (mode list)**> <**string list (map list)**> \[<**string list**> <**string list**>\]... - add map rotations entry.
* **maxdemos** <**int**> - maximum count of demos to keep.
* **maxdemosize** <**int**> - maximum demo size, in mebibytes.
* **restrictdemos** <**bool**> - whether to restrict demo record setting to admin. defaults to `1`.
* **autorecorddemo** <**bool**> - default of demo record setting. defaults to `0`.
* **restrictpausegame** <**bool**> - whether to restrict pause game setting to admin. defaults to `1`.
* **restrictgamespeed** <**bool**> - whether to restrict game speed setting to admin. defaults to `1`.
* **serverdesc** <**string**> - server description.
* **serverpass** <**string**> - allow connecting only with this password. empty string (`""`) disables.
* **adminpass** <**string**> - allow getting admin with this password. empty string (`""`) disables.
* **publicserver** <**select**> - controls whether or not the server is intended for "public" use.
  * `0` (the default) - allows `setmaster 1` and locked/private mastermodes (for coop-editing and such)
  * `1` - can only gain master by "auth" or admin, and doesn't allow locked/private mastermodes
  * `2` - allows `setmaster 1` but disallows private mastermode (for public coop-editing)
* **servermotd** <**string**> - message to send to clients after they connect.
* **teamkillkickreset** - clear teamkillkick rules.
* **teamkillkick** <**string list (mode list)**> <**int (tk limit)**> \[<**int (ban time in minutes)**>\] - add teamkillkick rule.\
  ban time defaults to 30 minutes if not specified or `0`; set to negative value to disable ban.
* **serverauth** <**string**> - set server auth domain (for `sauth`/`sauthkick` client commands).
* **clearusers** - clear local auth users.
* **adduser** <**string (name)**> <**string (desc)**> <**string (pubkey)**> <**string (priv)**> - add local auth user.
* **gamelimit** <**int**> - game limit, in seconds. defaults to `600` (10 minutes).
* **gamelimit_overtime** <**bool**> - make certain gamemodes 1/2 longer (eg. 15 minutes instead of 10). defaults to `0` as this logic was removed from vanilla server in collect edition.
* **persistbots** <**bool**> - persist bots when switching maps/modes. defaults to `0`.
* **serverintermission** <**int**> - intermission time, in seconds. defaults to `10`.
* **overtime** <**bool**> - enable overtime when scores are tied. defaults to `0`.
* **modifiedmapspectator** <**switch**> - whether to force clients with modified map to spectator mode.
  * `0` - no
  * `1` (the default) - only if server has map file to compare to
  * `2` - always, even if server doesn't have map file to compare to (in that case it uses majority voting)
* **maxsendmap** <**int**> - maximum size of sendmap we accept, in mebibytes. defaults to `4`.
* **serverautomaster** <**switch**> - give master to first client who joins.
  * `0` (the default) - don't
  * `1` - give master
  * `2` - give auth
* **serverbotlimit** <**int**> - bot limit. the default is `8`.
* **serverbotbalance** <**bool**> - whether to balance bots. the default is `1`.

## mode-specific tuning
* **regenbluearmour** <**bool**> - whether to give blue armour by default in regen capture mode. defaults to `1`.
* **ctftkpenalty** <**bool**> - whether to penalize teamkill of flag holder in ctf modes. defaults to `1`.

```
fpsgame/z_announcekills.h:VAR(announcekills, 0, 0, 1);
fpsgame/z_announcekills.h:VAR(announcekills_teamnames, 0, 1, 1);
fpsgame/z_announcekills.h:VAR(announcekills_stopped, 0, 1, 1);
fpsgame/z_announcekills.h:VAR(announcekills_stopped_min, 0, 15, 100);
fpsgame/z_announcekills.h:VAR(announcekills_multikill, 0, 2, 2);
fpsgame/z_announcekills.h:VAR(announcekills_rampage, 0, 1, 2);
fpsgame/z_announcekills.h:VAR(announcekills_rampage_min, 0, 0, 100);
fpsgame/z_announcekills.h:VAR(announcekills_numeric, 0, 0, 1);
fpsgame/z_announcekills.h:VAR(announcekills_numeric_num, 1, 5, 100);
fpsgame/z_announcekills.h:VAR(announcekills_noctf, 0, 1, 1);
fpsgame/z_announcekills.h:ICOMMAND(announcekills_multikillstrings, "is2V", (tagval *args, int numargs),
fpsgame/z_announcekills.h:SVAR(announcekills_style_stopped, "\f2%V was stopped by %A (\f6%n kills\f2)");
fpsgame/z_announcekills.h:SVAR(announcekills_style_multikills, "\f2%Y scored %sKILL!!");
fpsgame/z_announcekills.h:SVAR(announcekills_style_multikills_num, "\f2%Y scored \f3%n MULTIPLE KILLS!!");
fpsgame/z_announcekills.h:SVAR(announcekills_style_numeric, "\f2%Y made \f7%n\f2 kills");
fpsgame/z_announcekills.h:SVAR(announcekills_style_rampage, "\f2%A is %s");
fpsgame/z_announcekills.h:ICOMMAND(announcekills_rampagestrings, "is2V", (tagval *args, int numargs),
fpsgame/z_antiflood.h:VAR(antiflood_mode, 0, 2, 2);   // 0 - disconnect, 1 - rate limit, 2 - block
fpsgame/z_antiflood.h:VAR(antiflood_text, 0, 10, 8192);
fpsgame/z_antiflood.h:VAR(antiflood_text_time, 1, 5000, 2*60000);
fpsgame/z_antiflood.h:VAR(antiflood_rename, 0, 4, 8192);
fpsgame/z_antiflood.h:VAR(antiflood_rename_time, 0, 4000, 2*60000);
fpsgame/z_antiflood.h:VAR(antiflood_team, 0, 4, 8192);
fpsgame/z_antiflood.h:VAR(antiflood_team_time, 1, 4000, 2*60000);
fpsgame/z_antiflood.h:SVAR(antiflood_style, "\f6antiflood: %T was blocked, wait %w ms before resending");
fpsgame/z_autosendmap.h:VARFN(autosendmap, defaultautosendmap, 0, 0, 2, { if(clients.empty()) z_autosendmap = defaultautosendmap; });
fpsgame/z_awards.h:VAR(awards, 0, 0, 1);
fpsgame/z_awards.h:VAR(awards_splitmessage, 0, 1, 1);
fpsgame/z_awards.h:VAR(awards_max_equal, 1, 3, 5);
fpsgame/z_awards.h:SVAR(awards_style_clientsseparator, ", ");
fpsgame/z_awards.h:SVAR(awards_style_clientprefix, "\fs\f7");
fpsgame/z_awards.h:SVAR(awards_style_clientsuffix, "\fr");
fpsgame/z_awards.h:SVAR(awards_style_nobody, "\fs\f4(no one)\fr");
fpsgame/z_awards.h:COMMAND(stats_style, "ss2V");
fpsgame/z_awards.h:COMMAND(stats_stylereset, "");
fpsgame/z_awards.h:COMMAND(awards_style, "ss2V");
fpsgame/z_awards.h:COMMAND(awards_stylereset, "");
fpsgame/z_awards.h:VAR(awards_fallback, 0, 1, 1);
fpsgame/z_awards.h:SVAR(stats_style_normal, "\f6stats: \f7%C: \f2frags: \f7%f\f2, deaths: \f7%l\f2, suicides: \f7%s\f2, accuracy(%%): \f7%a\f2, kpd: \f7%p");
fpsgame/z_awards.h:SVAR(stats_style_teamplay, "\f6stats: \f7%C: \f2frags: \f7%f\f2, deaths: \f7%l\f2, suicides: \f7%s\f2, teamkills: \f7%t\f2, accuracy(%%): \f7%a\f2, kpd: \f7%p");
fpsgame/z_awards.h:SVAR(stats_style_ctf, "\f6stats: \f7%C: \f2frags: \f7%f\f2, flags: \f7%g\f2, deaths: \f7%l\f2, suicides: \f7%s\f2, teamkills: \f7%t\f2, accuracy(%%): \f7%a\f2, kpd: \f7%p");
fpsgame/z_awards.h:VAR(stats_fallback, 0, 1, 1);
fpsgame/z_bans.h:VAR(serverautounban, -1, 1, 1); // -1 = depends whether authed mode, 0 - off, 1 - on
fpsgame/z_bans.h:VARF(teamkillspectate, 0, 0, 1,
fpsgame/z_bans.h:SVAR(ban_message_teamkillspectate, "you got spectated because teamkills limit was reached");
fpsgame/z_bans.h:SVAR(ban_message_teamkillban, "ip you are connecting from is banned because of teamkills");
fpsgame/z_bans.h:SVAR(ban_message_kickban, "");
fpsgame/z_bans.h:SVAR(ban_message_kickbanreason, "ip you are connecting from is banned because: %r");
fpsgame/z_bans.h:SVAR(ban_message_specreason, "you are spectated because of %t ban");
fpsgame/z_checkpos.h:VAR(servertrackpos, 0, 0, 1);
fpsgame/z_checkpos.h:VAR(servertrackpos_autotune, 0, 0, 1);
fpsgame/z_checkpos.h:VAR(servertrackpos_maxlag, 0, 0, INT_MAX);
fpsgame/z_checkpos.h:COMMAND(z_test_totalx, "i");
fpsgame/z_checkpos.h:COMMAND(z_test_totaly, "i");
fpsgame/z_checkpos.h:COMMAND(z_test_totalmillis, "i");
fpsgame/z_checkpos.h:COMMAND(z_test_gamemillis, "i");
fpsgame/z_checkpos.h:COMMAND(z_test_poslen, "i");
fpsgame/z_format.h:SVAR(date_format, "%Y-%m-%d %H:%M:%S");
fpsgame/z_format.h:VAR(date_local, 0, 1, 1);
fpsgame/z_gbans_override.h:VAR(showbanreason, 0, 0, 1);
fpsgame/z_gbans_override.h:SVAR(ban_message_banned, "ip/range you are connecting from is banned because: %r");
fpsgame/z_gbans_override.h:SVAR(pbans_file, "pbans.cfg");
fpsgame/z_gbans_override.h:VAR(pbans_expire, 0, 0, INT_MAX);    // pbans expiration time, in seconds
fpsgame/z_gbans_override.h:COMMAND(pban, "ssss");
fpsgame/z_gbans_override.h:COMMAND(clearpbans, "i");
fpsgame/z_gbans_override.h:ICOMMAND(ipban, "s", (const char *ipname), addban(-1, ipname));
fpsgame/z_gbans_override.h:ICOMMAND(clearipbans, "", (), ipbans.clear());
fpsgame/z_gbans_override.h:VAR(pbans_minrange, 0, 0, 32);
fpsgame/z_genericservercommands.h:VAR(servcmd_pm_comfirmation, 0, 1, 1);
fpsgame/z_geoip.h:VAR(geoip_reload, 0, 2, 2);
fpsgame/z_geoip.h:VARF(geoip_enable, 0, 0, 1, { if(geoip_reload || !geoip_enable) z_reset_geoip(); });
fpsgame/z_geoip.h:SVARF(geoip_mmdb, "", { if(geoip_reload) z_reset_mmdb(geoip_reload == 1); });
fpsgame/z_geoip.h:VAR(geoip_mmdb_poll, 0, 0, 1);
fpsgame/z_geoip.h:SVAR(geoip_mmdb_lang, "en");
fpsgame/z_geoip.h:SVARF(geoip_country, "", { if(geoip_reload) z_reset_geoip_country(); });
fpsgame/z_geoip.h:SVARF(geoip_city, "", { if(geoip_reload) z_reset_geoip_city(); });
fpsgame/z_geoip.h:VAR(geoip_show_ip, 0, 2, 2);          // IP address
fpsgame/z_geoip.h:VAR(geoip_show_network, 0, 1, 2);     // network description
fpsgame/z_geoip.h:VAR(geoip_show_city, 0, 0, 2);        // city
fpsgame/z_geoip.h:VAR(geoip_show_region, 0, 0, 2);      // region
fpsgame/z_geoip.h:VAR(geoip_show_country, 0, 1, 2);     // country
fpsgame/z_geoip.h:VAR(geoip_show_continent, 0, 0, 2);   // continent
fpsgame/z_geoip.h:VAR(geoip_skip_duplicates, 0, 1, 2);  // skip duplicate fields. 1 - only if without gaps, 2 - with gaps too
fpsgame/z_geoip.h:VAR(geoip_country_use_db, 0, 2, 2);   // in case of old libgeoip, select the way we handle cases with 2 databases (country & city).
fpsgame/z_geoip.h:VAR(geoip_fix_country, 0, 1, 1);      // convert "Y, X of" to "X of Y"
fpsgame/z_geoip.h:VAR(geoip_ban_mode, 0, 0, 1);         // 0 - blacklist, 1 - whitelist
fpsgame/z_geoip.h:VAR(geoip_ban_anonymous, 0, 0, 1);    // 1 - ban networks marked anonymous by geoip
fpsgame/z_geoip.h:VAR(geoip_default_networks, 0, 1, 1); // whether to include default list of hardcoded network names
fpsgame/z_geoip.h:ICOMMAND(geoip_override_network, "ss", (const char *ip, const char *network),
fpsgame/z_geoip.h:ICOMMAND(geoip_clearoverrides, "", (), z_geoip_overrides.shrink(0));
fpsgame/z_geoip.h:ICOMMAND(geoip_clearbans, "", (), z_geoipbans.shrink(0));
fpsgame/z_geoip.h:ICOMMAND(geoip_ban_country, "ss", (char *country, char *message), z_geoip_addban(GIB_COUNTRY, country, message));
fpsgame/z_geoip.h:ICOMMAND(geoip_ban_continent, "ss", (char *continent, char *message), z_geoip_addban(GIB_CONTINENT, continent, message));
fpsgame/z_geoip.h:SVAR(geoip_ban_message, "");
fpsgame/z_geoipserver.h:VAR(geoip_admin_restriction, 0, 1, 1);  // restrictions shall apply for lower than admin privs, or lower than master?
fpsgame/z_geoipserver.h:SVAR(geoip_style_normal, "%C connected from %L");
fpsgame/z_geoipserver.h:SVAR(geoip_style_normal_query, "%C is connected from %L");
fpsgame/z_geoipserver.h:SVAR(geoip_style_local, "%C connected as local client");
fpsgame/z_geoipserver.h:SVAR(geoip_style_local_query, "%C is connected as local client");
fpsgame/z_geoipserver.h:SVAR(geoip_style_failed_query, "failed to get any geoip information about %C");
fpsgame/z_geoipserver.h:SVAR(geoip_style_location_prefix, "");
fpsgame/z_geoipserver.h:SVAR(geoip_style_location_sep, ", ");
fpsgame/z_geoipserver.h:SVAR(geoip_style_location_postfix, "");
fpsgame/z_geoipserver.h:VAR(geoip_style_location_order, 0, 1, 1); // 0 - continent first, 1 - continent last
fpsgame/z_geoipserver.h:VAR(geoip_log, 0, 0, 2);    // 0 - no info, 1 - restricted (censored) info, 2 - full info
fpsgame/z_geoipserver.h:ICOMMAND(s_geoip_resolveclients, "", (),
fpsgame/z_geoipserver.h:ICOMMAND(s_geoip_client, "ii", (int *cn, int *a),
fpsgame/z_loadmap.h:SVAR(mappath, "packages/base");
fpsgame/z_log.h:SVAR(kick_style_normal,        "%k kicked %v");
fpsgame/z_log.h:SVAR(kick_style_normal_reason, "%k kicked %v because: %r");
fpsgame/z_log.h:SVAR(kick_style_spy,           "%v was kicked");
fpsgame/z_log.h:SVAR(kick_style_spy_reason,    "%v was kicked because: %r");
fpsgame/z_log.h:SVAR(bans_style_normal,        "%C %bned %v for %t");
fpsgame/z_log.h:SVAR(bans_style_normal_reason, "%C %bned %v for %t because: %r");
fpsgame/z_log.h:SVAR(bans_style_spy,           "%v was %bned for %t");
fpsgame/z_log.h:SVAR(bans_style_spy_reason,    "%v was %bned for %t because: %r");
fpsgame/z_log.h:VAR(discmsg_privacy, 0, 1, 1);          // avoid broadcasting clients' ips to unauthorized clients
fpsgame/z_log.h:VAR(discmsg_verbose, 0, 0, 2);          // (only applies for not connected clients) whether show disc msg (2 - force)
fpsgame/z_log.h:VAR(discmsg_showip_admin, -1, -1, 1);   // whether we should treat master or admin as authorized client (-1 - depending on auth mode)
fpsgame/z_log.h:VAR(discmsg_showip_kicker, 0, 0, 1);    // whether we should always treat kicker as authorized to see regardless of priv
fpsgame/z_maploaded.h:VAR(mapload_debug, 0, 0, 1);
fpsgame/z_maprotation.h:SVARF(maprotationmode, "", z_setmrt(maprotationmode));
fpsgame/z_maprotation.h:VAR(maprotation_norepeat, 0, 10, 9000); // how much old maps we should exclude when using random selection
fpsgame/z_maprotation.h:VAR(nextmapclear, 0, 1, 2); // 0 - don't, 1 - on same-map change, 2 - on any map change
fpsgame/z_maprotation.h:VAR(mapbattlemaps, 2, 3, 10);
fpsgame/z_maprotation.h:VAR(mapbattle_intermission, 1, 20, 3600);
fpsgame/z_maprotation.h:VAR(mapbattle_chat, 0, 0, 1);
fpsgame/z_maprotation.h:SVAR(mapbattle_style_announce, "\f3mapbattle:\f7 %M"); // %M - map list separated by separator
fpsgame/z_maprotation.h:SVAR(mapbattle_style_announce_map, "%m \f0#%n\f7"); // how map should look in map list
fpsgame/z_maprotation.h:SVAR(mapbattle_style_announce_separator, ", ");
fpsgame/z_maprotation.h:SVAR(mapbattle_style_vote, "\f3mapbattle:\f2 \f7%c \f2voted for \f7%m \f0#%n\f2; current votes: %V");
fpsgame/z_maprotation.h:SVAR(mapbattle_style_vote_current, "\fs\f7%m \f0#%n\f2: \f3%v\fr");
fpsgame/z_maprotation.h:SVAR(mapbattle_style_vote_separator, ", ");
fpsgame/z_maprotation.h:SVAR(mapbattle_style_notmapbattle, "currently there is no map battle");
fpsgame/z_maprotation.h:SVAR(mapbattle_style_outofrange, "map battle vote out of range");
fpsgame/z_mapsucks.h:VARF(mapsucks, 0, 0, 2, z_enable_command("mapsucks", mapsucks!=0));
fpsgame/z_mapsucks.h:VAR(mapsucks_time, 0, 30, 3600);
fpsgame/z_mapsucks.h:SVAR(mapsucks_style_vote, "%C thinks this map sucks. current mapsucks votes: [%z/%l]. you can rate this map with #mapsucks");
fpsgame/z_mapsucks.h:SVAR(mapsucks_style_waitsuccess, "mapsucks voting succeeded, staring intermission in %t seconds");
fpsgame/z_mapsucks.h:SVAR(mapsucks_style_success, "mapsucks voting succeeded, starting intermission");
fpsgame/z_mapsucks.h:SVAR(mapsucks_style_speccantvote, "Spectators may not rate map");
fpsgame/z_mapsucks.h:SVAR(mapsucks_style_editmode, "mapsucks is disabled in coop edit mode");
fpsgame/z_ms_engineserver_override.h:COMMAND(addms, "");
fpsgame/z_ms_engineserver_override.h:ICOMMAND(addmaster, "", (), addms());
fpsgame/z_ms_engineserver_override.h:COMMAND(clearmss, "");
fpsgame/z_ms_engineserver_override.h:ICOMMAND(clearmasters, "", (), clearmss());
fpsgame/z_ms_engineserver_override.h:VARFN(updatemaster, allowupdatemaster, 0, 1, 1,
fpsgame/z_ms_engineserver_override.h:SVARF(mastername, server::defaultmaster(),
fpsgame/z_ms_engineserver_override.h:VARF(masterport, 1, server::masterport(), 0xFFFF,
fpsgame/z_ms_engineserver_override.h:SVARF(masterauth, "",
fpsgame/z_ms_engineserver_override.h:ICOMMAND(masterauthpriv, "i", (int *i),
fpsgame/z_ms_engineserver_override.h:ICOMMAND(masterminauthpriv, "i", (int *i),
fpsgame/z_ms_engineserver_override.h:ICOMMAND(mastermaxauthpriv, "i", (int *i),
fpsgame/z_ms_engineserver_override.h:ICOMMAND(masterbanwarn, "s", (char *s),
fpsgame/z_ms_engineserver_override.h:ICOMMAND(masterwhitelistauth, "s", (char *s),
fpsgame/z_ms_engineserver_override.h:ICOMMAND(masterbanmessage, "s", (char *s),
fpsgame/z_ms_engineserver_override.h:ICOMMAND(masterbanmode, "i", (int *i),    // -1 - whitelist, 0 - ignore, 1 (default) - blacklist, 2 - specban
fpsgame/z_ms_gameserver_override.h:VAR(authconnect, 0, 1, 2);
fpsgame/z_ms_gameserver_override.h:VAR(anyauthconnect, 0, 0, 1);
fpsgame/z_mutes.h:VARFN(autoeditmute, z_defaultautoeditmute, 0, 0, 1, { if(clients.empty()) z_autoeditmute = z_defaultautoeditmute!=0; });
fpsgame/z_nodamage.h:VAR(antispawnkill, 0, 0, 5000); // in milliseconds
fpsgame/z_nodamage.h:VARF(servernodamage, 0, 0, 2, { if(!servernodamage_global || clients.empty()) z_nodamage = servernodamage; });
fpsgame/z_nodamage.h:VARF(servernodamage_global, 0, 1, 1,
fpsgame/z_patchmap.h:VAR(s_patchreliable, -1, 1, 1); // use reliable packets or unreliable ones?
fpsgame/z_patchmap.h:COMMAND(s_sendallpatches, ""); // sends out all var and ent patches
fpsgame/z_patchmap.h:COMMAND(s_sendentpatches, "i"); // sends out last x ent patches. if x is unspecified, send out all ent patches
fpsgame/z_patchmap.h:COMMAND(s_sendvarpatches, "i"); // sends out last x var patches. if x is unspecified, send out all var patches
fpsgame/z_patchmap.h:VAR(s_undoentpatches, 0, 1, 1); // should we clear/undo ents when doing s_clearentpatches/s_clearpatches?
fpsgame/z_patchmap.h:COMMAND(s_clearpatches, ""); // undos/clears var and ent patches
fpsgame/z_patchmap.h:COMMAND(s_clearentpatches, "i"); // undos/clears last x ent patches
fpsgame/z_patchmap.h:COMMAND(s_clearvarpatches, "i"); // clears last x var patches
fpsgame/z_patchmap.h:VAR(s_autosendpatch, 0, 1, 1); // whether to automatically send patches on s_patchent/s_patchvar* or not
fpsgame/z_patchmap.h:VAR(s_patchadd, 0, 1, 1); // whether to add patch to list or not
fpsgame/z_patchmap.h:COMMAND(s_patchent, "sifffiiiiii"); // map id x y z type a1 a2 a3 a4 a5
fpsgame/z_patchmap.h:COMMAND(s_patchvari, "ssi");
fpsgame/z_patchmap.h:COMMAND(s_patchvarf, "ssf");
fpsgame/z_patchmap.h:COMMAND(s_patchvars, "sss");
fpsgame/z_persistteams.h:VARFN(persistteams, defaultpersistteams, 0, 0, 2, { if(clients.empty()) z_persistteams = defaultpersistteams; });
fpsgame/z_protectteamscores.h:VAR(protectteamscores, 0, 0, 2);
fpsgame/z_racemode.h:VARF(racemode_enabled, 0, 0, 1,
fpsgame/z_racemode.h:VARF(racemode_default, 0, 0, 1,
fpsgame/z_racemode.h:VAR(racemode_allowcheat, 0, 0, 1);
fpsgame/z_racemode.h:VAR(racemode_allowedit, 0, 0, 1);
fpsgame/z_racemode.h:VAR(racemode_alloweditmode, 0, 1, 1);
fpsgame/z_racemode.h:VAR(racemode_allowmultiplaces, 0, 1, 1);
fpsgame/z_racemode.h:VAR(racemode_hideeditors, 0, 0, 1);
fpsgame/z_racemode.h:VAR(racemode_strict, 0, 0, 1);              // prefer strict logic to most convenient behavour
fpsgame/z_racemode.h:VAR(racemode_waitmap, 0, 10000, INT_MAX);
fpsgame/z_racemode.h:VAR(racemode_sendspectators, 0, 1, 1);
fpsgame/z_racemode.h:VAR(racemode_startmillis, 0, 5000, INT_MAX);
fpsgame/z_racemode.h:VAR(racemode_gamelimit, 0, 0, INT_MAX);
fpsgame/z_racemode.h:VAR(racemode_winnerwait, 0, 30000, INT_MAX);
fpsgame/z_racemode.h:VAR(racemode_finishmillis, 0, 5000, INT_MAX);
fpsgame/z_racemode.h:VAR(racemode_maxents, 0, 1000, MAXENTS);
fpsgame/z_racemode.h:VAR(racemode_minplaces, 1, 1, RACE_MAXPLACES);
fpsgame/z_racemode.h:ICOMMAND(clearrends, "", (), z_rends.shrink(0));
fpsgame/z_racemode.h:ICOMMAND(rend, "sfffii", (char *map, float *x, float *y, float *z, int *r, int *p),
fpsgame/z_racemode.h:VARF(racemode_record, 0, 0, 1,
fpsgame/z_racemode.h:VAR(racemode_record_atstart, 0, 1, 1); // whether to announce current record holder at start of race
fpsgame/z_racemode.h:VAR(racemode_record_atend, 0, 1, 1); // whether to announce current record holder at end of race
fpsgame/z_racemode.h:SVAR(racemode_record_style_atstart, "\f6RECORD HOLDER: \f7%O (%o)");
fpsgame/z_racemode.h:SVAR(racemode_record_style_atend, "\f6RECORD HOLDER: \f7%O (%o)");
fpsgame/z_racemode.h:SVAR(racemode_style_winners, "\f6RACE WINNERS: %W");
fpsgame/z_racemode.h:SVAR(racemode_style_winplace, "\f2%P PLACE: \f7%C (%t)");
fpsgame/z_racemode.h:COMMAND(racemode_style_places, "sV");
fpsgame/z_racemode.h:SVAR(racemode_style_enterplace, "\f6race: \f7%C \f2won \f6%P PLACE!! \f7(%t)");
fpsgame/z_racemode.h:SVAR(racemode_style_leaveplace, "\f6race: \f7%C \f2left \f6%P PLACE!!");
fpsgame/z_racemode.h:SVAR(racemode_style_starting, "\f6race: \f2starting race in %T...");
fpsgame/z_racemode.h:SVAR(racemode_style_start, "\f6race: \f7START!!");
fpsgame/z_racemode.h:SVAR(racemode_style_timeleft, "\f2time left: %T");
fpsgame/z_racemode.h:SVAR(racemode_style_ending, "\f6race: \f2ending race in %T...");
fpsgame/z_racemode.h:SVAR(racemode_message_gotmap, "since you got the map, you may continue racing now");
fpsgame/z_racemode.h:SVAR(racemode_message_respawn, "since you respawned, you may continue racing");
fpsgame/z_racemode.h:SVAR(racemode_message_editmode, "edit mode is not allowed in racemode. please suicide to continue racing");
fpsgame/z_racemode.h:SVAR(racemode_message_editing, "editing map is not allowed in racemode. please getmap or reconnect to continue racing");
fpsgame/z_records.h:VAR(record_unnamed, 0, 0, 1); // whether to record unnamed players or not
fpsgame/z_records.h:VAR(record_expire, 0, 0, INT_MAX); // expire time, in milliseconds
fpsgame/z_records.h:VAR(record_min, 0, 0, INT_MAX); // shortest recordable race win time, in milliseconds
fpsgame/z_records.h:VAR(record_max, 0, 0, INT_MAX); // longest recordable race win time, in milliseconds. 0 = disable
fpsgame/z_records.h:SVAR(record_style_new, "\f6NEW RECORD: \f7%C (%t)");
fpsgame/z_records.h:SVAR(record_style_newold, "\f6NEW RECORD: \f7%C (%t) \f2(old: \f7%O (%o)\f2)");
fpsgame/z_rename.h:ICOMMAND(s_rename, "is", (int *cn, char *newname),
fpsgame/z_savemap.h:ICOMMAND(serversavemap, "i", (int *i), z_enable_command("savemap", *i!=0));
fpsgame/z_scripting.h:COMMAND(s_announce, "iis");
fpsgame/z_scripting.h:COMMAND(s_clearannounces, "");
fpsgame/z_scripting.h:COMMAND(s_sleep, "iis");
fpsgame/z_scripting.h:COMMAND(s_sleep_r, "iis");
fpsgame/z_scripting.h:COMMAND(s_clearsleep, "");
fpsgame/z_scripting.h:VARF(autospecsecs, 0, 0, 604800, z_autospec_process(NULL));
fpsgame/z_scripting.h:COMMAND(s_write, "is");
fpsgame/z_scripting.h:ICOMMAND(s_wall, "C", (char *s), sendservmsg(s));
fpsgame/z_scripting.h:COMMAND(s_kick, "is");
fpsgame/z_scripting.h:COMMAND(s_kickban, "iss");
fpsgame/z_scripting.h:COMMAND(s_listclients, "b");
fpsgame/z_scripting.h:ICOMMAND(s_getclientname, "i", (int *cn), { clientinfo *ci = getinfo(*cn); result(ci && ci->connected ? ci->name : ""); });
fpsgame/z_scripting.h:ICOMMAND(s_getclientprivilege, "i", (int *cn), { clientinfo *ci = getinfo(*cn); intret(ci && ci->connected ? ci->privilege : 0); });
fpsgame/z_scripting.h:ICOMMAND(s_getclientprivname, "i", (int *cn), { clientinfo *ci = getinfo(*cn); result(ci && ci->connected ? privname(ci->privilege) : ""); });
fpsgame/z_scripting.h:ICOMMAND(s_getclienthostname, "i", (int *cn), { const char *hn = getclienthostname(*cn); result(hn ? hn : ""); });
fpsgame/z_scripting.h:ICOMMAND(s_numclients, "bbbb", (int *exclude, int *nospec, int *noai, int *priv),
fpsgame/z_scripting.h:ICOMMAND(s_addai, "ib", (int *skill, int *limit), intret(aiman::addai(*skill, *limit) ? 1 : 0));
fpsgame/z_scripting.h:ICOMMAND(s_delai, "", (), intret(aiman::deleteai() ? 1 : 0));
fpsgame/z_scripting.h:ICOMMAND(s_m_noitems,      "", (), intret(m_noitems      ? 1 : 0));
fpsgame/z_scripting.h:ICOMMAND(s_m_noammo,       "", (), intret(m_noammo       ? 1 : 0));
fpsgame/z_scripting.h:ICOMMAND(s_m_insta,        "", (), intret(m_insta        ? 1 : 0));
fpsgame/z_scripting.h:ICOMMAND(s_m_tactics,      "", (), intret(m_tactics      ? 1 : 0));
fpsgame/z_scripting.h:ICOMMAND(s_m_efficiency,   "", (), intret(m_efficiency   ? 1 : 0));
fpsgame/z_scripting.h:ICOMMAND(s_m_capture,      "", (), intret(m_capture      ? 1 : 0));
fpsgame/z_scripting.h:ICOMMAND(s_m_capture_only, "", (), intret(m_capture_only ? 1 : 0));
fpsgame/z_scripting.h:ICOMMAND(s_m_regencapture, "", (), intret(m_regencapture ? 1 : 0));
fpsgame/z_scripting.h:ICOMMAND(s_m_ctf,          "", (), intret(m_ctf          ? 1 : 0));
fpsgame/z_scripting.h:ICOMMAND(s_m_ctf_only,     "", (), intret(m_ctf_only     ? 1 : 0));
fpsgame/z_scripting.h:ICOMMAND(s_m_protect,      "", (), intret(m_protect      ? 1 : 0));
fpsgame/z_scripting.h:ICOMMAND(s_m_hold,         "", (), intret(m_hold         ? 1 : 0));
fpsgame/z_scripting.h:ICOMMAND(s_m_collect,      "", (), intret(m_collect      ? 1 : 0));
fpsgame/z_scripting.h:ICOMMAND(s_m_teammode,     "", (), intret(m_teammode     ? 1 : 0));
fpsgame/z_scripting.h:ICOMMAND(s_m_overtime,     "", (), intret(m_overtime     ? 1 : 0));
fpsgame/z_scripting.h:ICOMMAND(s_m_edit,         "", (), intret(m_edit         ? 1 : 0));
fpsgame/z_scripting.h:ICOMMAND(s_setteam, "isi", (int *cn, char *newteam, int *mode),
fpsgame/z_scripting.h:ICOMMAND(s_getteam, "i", (int *cn),
fpsgame/z_scripting.h:ICOMMAND(s_listteam, "sbb", (char *team, int *spec, int *bots),
fpsgame/z_scripting.h:ICOMMAND(s_listteams, "bb", (int *spec, int *bots),
fpsgame/z_scripting.h:ICOMMAND(s_countteam, "sbb", (char *team, int *spec, int *bots),
fpsgame/z_servcmd.h:SVAR(servcmd_chars, "");
fpsgame/z_servercommands.h:COMMAND(commands_privilege, "si2V");
fpsgame/z_servercommands.h:ICOMMAND(commands_enable, "sV", (tagval *args, int numargs), z_enable_commands(args, numargs, true));
fpsgame/z_servercommands.h:ICOMMAND(commands_disable, "sV", (tagval *args, int numargs), z_enable_commands(args, numargs, false));
fpsgame/z_servercommands.h:ICOMMAND(commands_hide, "sV", (tagval *args, int numargs), z_hide_commands(args, numargs, true));
fpsgame/z_servercommands.h:ICOMMAND(commands_show, "sV", (tagval *args, int numargs), z_hide_commands(args, numargs, false));
fpsgame/z_servercommands.h:ICOMMAND(commands_delete, "sV", (tagval *args, int numargs), z_delete_commands(args, numargs));
fpsgame/z_servercommands.h:ICOMMAND(commands_clone, "sV", (tagval *args, int numargs), z_clone_command(args, numargs));
fpsgame/z_servercommands.h:VAR(allowservcmd, 0, 0, 1);
fpsgame/z_servercommands.h:SVAR(servcmd_message_pleasespecifyclient, "please specify client number");
fpsgame/z_servercommands.h:SVAR(servcmd_message_unknownclient, "unknown client: %s");
fpsgame/z_servercommands.h:SVAR(servcmd_message_pleasespecifymessage, "please specify message");
fpsgame/z_serverdesc.h:SVARF(serverdesctmpl, "%s", z_serverdescchanged()); // template to use with user settable stuff
fpsgame/z_serverdesc.h:VAR(serverdescfilter, 0, 1, 1); // whether stuff sent to serverlist is filtered
fpsgame/z_serverdesc.h:VAR(serverdescuserfilter, 0, 1, 1); // whether user settable stuff is filtered
fpsgame/z_setmaster_override.h:VAR(defaultmastermode, -1, 0, 3);
fpsgame/z_talkbot.h:COMMAND(cleartalkbots, "iN");
fpsgame/z_talkbot.h:ICOMMAND(talkbot, "is", (int *cn, char *name), intret(addtalkbot(*cn, name)));
fpsgame/z_talkbot.h:COMMAND(talkbotpriv, "iib");
fpsgame/z_talkbot.h:ICOMMAND(s_talkbot_say, "is", (int *bn, char *msg),
fpsgame/z_talkbot.h:ICOMMAND(s_talkbot_fakesay, "iss", (int *bn, char *fakename, char *msg),
fpsgame/z_talkbot.h:ICOMMAND(s_talkbot_sayteam, "iis", (int *bn, int *tn, char *msg),
fpsgame/z_talkbot.h:ICOMMAND(s_talkbot_fakesayteam, "isis", (int *bn, char *fakename, int *tn, char *msg),
fpsgame/z_talkbot.h:ICOMMAND(listtalkbots, "s", (char *name), {
fpsgame/z_tree.h:COMMAND(z_testtree_init, "");
fpsgame/z_tree.h:COMMAND(z_testtree_add, "i");
fpsgame/z_tree.h:COMMAND(z_testtree_rnd, "");
fpsgame/z_tree.h:COMMAND(z_testtree_speed, "ii");
```
