# Discord bot for zeromod-sauerbraten by sata.
# version: 1.00 (initial release)
# stable working core functionality

# Some notable features:
# 1) Breaks lengthy discord messages to multiple lines in-game
# 2) Blocks discord messages containing attachment(s) and Emoji(s) from sending to sauerbraten server.
# 3) Notifies discord users about Message failures.


import re
import discord
import asyncio
from discord.ext import tasks
import time
import sys
import os
import string
import signal
import atexit


########################################################################################################################
def unblock_sauer_binary(fifo):
    fifo.writelines('\n')
    fifo.flush()


def filter_message(message, showservercommands):
    regex1 = r"^geoip: "
    regex2 = None
    if showservercommands is True:
        regex2 = r"^(connect|disconnect|master|chat|kick|servcmd): [^client]"
    else:
        regex2 = r"^(connect|disconnect|master|chat|kick): [^client]"
    if re.search(regex1, message) is not None:
        return True
    elif re.search(regex2, message) is not None:
        return True
    else:
        return False


@tasks.loop(seconds=0.1)
async def log_to_discord(sauer_log, channel):
    sauer_log.seek(0, 2)    # Only start reading the log from eof. Prevents spamming entire older log to discord.
    lastline = None
    while True:
        newline = sauer_log.readline()
        if len(newline) > 0 and filter_message(newline, showservercommands):
            if newline == lastline:
                continue
            if re.search(r"^geoip: ", newline) is not None:
                lastline = newline
                await channel.send(newline)
            else:
                await channel.send(newline)
        else:
            await asyncio.sleep(0.1)


def break_message(message, fifo, split_length):
    text = message.content
    total = len(text)
    start = 0
    while total > 0:
        if total > split_length:
            string = text[start:start+split_length] + "...."
            if start != 0:
                string = "...." + string
            start += split_length
            total -= split_length
            fifo.writelines('s_talkbot_say "" "[ {} ]:" "{}"\n'.format(message.author.name, string))
            fifo.flush()
        else:
            string = "...." + text[start:start+total]
            total = 0
            fifo.writelines('s_talkbot_say "" "[ {} ]:" "{}"\n'.format(message.author.name, string))
            fifo.flush()
        time.sleep(1.0)


def parse_config(conf_file):
    try:
        conf_file = open(conf_file, "r")
    except FileNotFoundError:
        print("[FILE-ERROR]: Config file not found. Make sure it is in the same directory as Discord Bot and named correctly (bot.config).")
        sys.exit(1)
    except PermissionError:
        print("[FILE-ERROR]: Unable to open config file for reading. Permission denied.")
        sys.exit(1)
    else:
        global TOKEN
        global channel_id
        global fifo_name
        global log_filename
        global showservercommands

        error_occured = False
        lines = conf_file.readlines()

        for line in lines:
            if re.search(r"(^discordbot_token) ([^ ]*)", line) is not None:
                TOKEN = line.split()[1].strip('"').strip("'")
            elif re.search(r"(^discordchannel_id) ([^ ]*)", line) is not None:
                channel_id = int(line.split()[1].strip('"').strip("'"))
            elif re.search(r"(^discordfifo_name) ([^ ]*)", line) is not None:
                fifo_name = line.split()[1].strip('"').strip("'")
            elif re.search(r"(^sauer_server_log_filename) ([^ ]*)", line) is not None:
                log_filename = line.split()[1].strip('"').strip("'")
            elif re.search(r"(^showservercommands) ([^ ]*)", line) is not None:
                if line.split()[1].strip('"').strip("'") == "1":
                    showservercommands = True

        if TOKEN is None:
            print("[CONFIG-ERROR]: 'discordbottoken' option was not found/set properly in config file.")
            error_occured = True
        if channel_id is None:
            print("[CONFIG-ERROR]: 'discordchannel_id' option was not found/set properly in config file.")
            error_occured = True
        if fifo_name is None:
            print("[CONFIG-ERROR]: 'discordfifo_name' option was not found/set properly in config file.")
            error_occured = True
        if log_filename is None:
            print("[CONFIG-ERROR]: 'sauer_server_log_filename' option was not found/set properly in config file.")
            error_occured = True
        if error_occured is True:
            sys.exit(0)


def exit_cleanup():
    print("[CLEANUP]: Closing FIFO.")
    fifo.close()
    print("[CLEANUP]: Closing sauerbaten log file.")
    sauer_log.close()
    print("[EXIT]: Discord Bot terminated successfully.")
    sys.exit(0)


def handle(sig, frame):
    print("[CLEANUP]: Closing FIFO.")
    fifo.close()
    print("[CLEANUP]: Closing sauerbaten log file.")
    sauer_log.close()
    print("[EXIT]: Discord Bot terminated successfully.")
    sys.exit(0)
########################################################################################################################


########################################################################################################################
TOKEN = None
channel_id = None
fifo_name = None
log_filename = None
guild_id = None
showservercommands = False


# SETUP INITIAL VALUE OF VARIABLES
parse_config("bot.config")


# HANDLE EXCEPTIONS RELATED TO FIFO
if os.path.exists(fifo_name) is False:
    print("[FILE-ERROR]: FIFO file not found. Make sure it is in the same directory as Discord Bot and its name matches the one specified in bot.config")
    sys.exit(1)
try:
    fifo = open(fifo_name, "w")
except PermissionError:
    print("[FILE-ERROR]: Unable to open FIFO file for writing. Permission denied.")
    sys.exit(1)
##################################


unblock_sauer_binary(fifo)  # Required to NOT LET sauerbraten server wait for input from fifo.
client = discord.Client()   # Initialize discord bot client


# HANDLE EXCEPTIONS RELATED TO SAUERBRATEN LOG FILE
try:
    sauer_log = open(log_filename, "r")
except FileNotFoundError:
    print("[FILE-ERROR]: Sauerbraten log file not found. Make sure it is in the same directory as Discord Bot and its name matches the one specified in bot.config")
    sys.exit(1)
except PermissionError:
    print("[FILE-ERROR]: Unable to open Sauerbraten log file for reading. Permission denied.")
    sys.exit(1)
###################################################


@client.event
async def on_ready():
    channel = client.get_channel(channel_id)
    log_to_discord.start(sauer_log, channel)
    print("[CONNECTION]: Successfully connected to discord.")
    print('[LOGIN]: We have logged in as {0.user} in discord'.format(client))
    await channel.send("<< [ BOT Initiated Successfully ] >>")


@client.event
async def on_message(message):
    if message.author == client.user:
        return
    # if message.content.startswith('$hello'):
    #     await message.channel.send('Hello!')
    elif message.channel.id == channel_id:
        if len(message.attachments) > 0:
            await message.author.send(f"[MESSAGE-FAILED]: Your last message ( http://discordapp.com/channels/{message.guild.id}/{message.channel.id}/{message.id} ) was not delivered to Sauerbraten Server because it contains unsupported Content/Attachments. Only simple text messages are allowed!")
        elif re.search(f"[^{re.escape(string.printable)}]", message.content) is not None:
            await message.author.send(f"[MESSAGE-FAILED]: Your last message ( http://discordapp.com/channels/{message.guild.id}/{message.channel.id}/{message.id} ) was not delivered to Sauerbraten Server because it contains Discord Emoji(s) which are unsupported by Sauerbraten. Only simple text messages are allowed!")
        elif len(message.content) > 259:
            break_message(message, fifo, 125)
        else:
            fifo.writelines('s_talkbot_say "" "[ {} ]:" "{}"\n'.format(message.author.name, str(message.content)))
            fifo.flush()


client.run(TOKEN)   # Runs the discord bot instance


atexit.register(exit_cleanup())
signal.signal(signal.SIGTERM, handle)
########################################################################################################################
