# config file for irc bot
# written in perl and exec'd from main script
(
	# irc server host bot should connect to
	"server" => "irc.andrius4669.org",
	# port of irc server host
	"serverport" => 6667,
	# channels bot shall try joining on connect
	"channels" => ['#test'],
	# nicks which shall be ignored
	"ignored_nicks" => {},
	# irc handle for our bot
	"ircnick" => "tester9",
	# username our bot shall send to server
	"ircuser" => "tester",
	# real name
	"ircreal" => "-",
	# password if server requires it to connect to server
	"ircpass" => "",
	# message irc bot shall use when quitting
	"irc_quit_message" => "",
	# characters interepted as start of command
	"irccmdchars" => [],
	# the way irc bot interacts in sauerbraten; please specify an integer;
	# -1 - use server's console, 0..255 - use preconfigured talkbot (need to define in server-init.cfg first)
	# OVER 9000 - create own talkbot for each irc user in channel -- not recomended if using multiple channels
	"usetalkbot" => -1,
	# per-channel setting overrides for usetalkbot option
	"talkbotchans" => {},
	# show irc's join/part/quit/kick/nick messages to sauer clients? 1 - yes, 0 - no
	"showjoinpart" => 1,
	# per-channel setting overrides for showjoinpart option
	"joinpartchans" => {},
	# whether it's ok to disclose sauer clients IP addresses to irc channel(s)
	# note that if sauer server supplies geoip info in log, irc bot uses that anyway
	"showsauerips" => 0,
	# what irc privileges shall we treat as operators (and thus allow use bot's commands from irc)
	"ircopprivs" => ['@', '%', '&'],
	# how bot shall repond to CTCP VERSION? if undefined, bot won't answer
	"ctcp_version" => undef,
	# should bot skip first word of every stdin line?
	"skipstdin" => 0,
)
