#!/bin/sh
# server starting and irc bot script
# currently limitted to one server

trap 'kill -TERM -0' EXIT

iicmd='ii'
ircdir='./irc'
server='irc.andrius4669.org'
srport='6667'
iinick='zmdbt'
iiname="zeromod"
chans="#test"
servfifo="./servfifo"
servbin="./sauer_server"
logfile="log"

talkbotnum=0

# waits till pipe is avaiable
wait_for_pipe ()
{
	while ! test -p "$1"; do sleep "$2"; done
}

# (re)spawns ii instance
ii_caller ()
{
	while true
	do
		# cleanup
		rm -f "$ircdir/$server/in"
		# spawn ii
		$iicmd -i "$ircdir" -s "$server" -p "$srport" -n "$iinick" -f "$iiname" &
		iipid="$!"
		# wait till connected
		wait_for_pipe "$ircdir/$server/in" .3
		# join channels
		printf "/j %s\n" "$chans" > "$ircdir/$server/in"
		# wait till it exits
		wait "$iipid"
	done
}

# waits till path becomes readable
wait_for_read ()
{
	while ! test -r "$1"; do sleep "$2"; done
}

# sends stuff directly to irc
send2irc ()
{
	wait_for_pipe "$ircdir/$server/in" .1
	printf "%s\n" "$1" > "$ircdir/$server/in"
}

# parses chat string and sends to irc
chat_2_irc ()
{
	local line="$1"
	# "a (12): aaa zzz" -> "a" "(12): aaa zzz"
	#nick="${line%% *}" line="${line#* }"
	# "(12): aaa zzz" -> "(12):" "aaa zzz"
	#cnum="${line%% *}" line="${line#* }"
	send2irc "/PRIVMSG $chans :$line"
}

# parse server log
server_parser ()
{
	# wait till we can read log file
	wait_for_read "$logfile" .1
	# read from logfile
	local type
	tail -f -n 0 "$logfile" | while IFS= read -r line
	do
		# "xxx: yyyyy aaa"  -> "xxx:" "yyyyy aaa"
		type="${line%% *}" line="${line#* }"
		# xxx: -> xxx
		type="${type%:}"

		case "$type" in
			"chat") chat_2_irc "$line";;
		esac
	done
}

write_2_serv ()
{
	wait_for_pipe "$servfifo" .1
	printf "%s\n" "$1" > "$servfifo"
}

escape_cube ()
{
	printf "%s\n" "$1" | sed -e 's/\^/\^\^/g' -e 's/"/\^"/g'
}

chat_2_serv ()
{
	local nick=`escape_cube "$1"` line=`escape_cube "$2"`
	write_2_serv "s_talkbot_fakesay $talkbotnum "'"'"$nick"'"'" "'"'"$line"'"'
}

# parse irc channel output
irc_chan_parser ()
{
	local chanfile="$ircdir/$server/$1/out"
	wait_for_read "$chanfile" .1
	local date time nick
	tail -f -n 0 "$chanfile" | while IFS= read -r line
	do
		date="${line%% *}" line="${line#* }"
		time="${line%% *}" line="${line#* }"
		nick="${line%% *}" line="${line#* }"
		# "<zzz>" -> "zzz>" -> "zzz"
		nick="${nick#<}" nick="${nick%>}"
		chat_2_serv "$nick" "$line"
	done
}

server_caller ()
{
	while true
	do
		rm -f "$servfifo"
		mkfifo "$servfifo"
		tail -f "$servfifo" | $servbin >> "$logfile"
	done
}


ii_caller &
server_caller &
server_parser &
irc_chan_parser "$chans" &

wait
