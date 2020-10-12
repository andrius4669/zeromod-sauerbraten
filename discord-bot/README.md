# DISCORD BOT FOR ZEROMOD-SAUERBRATEN
***
This discord bot was created in python specifically with zeromod-sauerbraten in mind and it is intended to work with zeromod only. This discord bot achieves the basic functionality of sending discord messages to zeromod sauerbraten servers and vice versa while preventing non text based messages from discord to be sent to sauerbraten. Basically the bot reads from server log and sends to discord and receives from discord and writes to sauer server stdin.





### REQUIRED CREDENTIALS
Make sure that you have initialized a discord bot by using the discord developer portal before proceeding. If not, either google or use this little [TUTORIAL](https://discordpy.readthedocs.io/en/latest/discord.html).
After that obtain your newly created bot **TOKEN** and **Channel ID**. Discord bot will use this channel of your guild to send messages back and forth.





### REQUIRED DEPENDENCIES
This bot requires that you have at least **Python version 3.5.3 or higher** installed.
This bot also requires **discord.py** module. To install, run:

````
pip3 install discord.py
````





### INSTALLATION AND CONFIGURATION
For ease and convenience, copy **discord_bot.py** and **bot.config** files to zeromod-sauerbraten directory in which your sauer server binary is present. Bot depends on fifo to feed discord messages to sauerbraten server. Make a fifo pipe in the same directory with:

```
mkfifo fifo
```

Then, you need to add the following lines to your **server-init.cfg** file to make the server accept commands from standard input and register the bot.

```
serveracceptstdin 1
talkbot 0 "DISCORD"

# In case you want to display location information of sauerbrante clients in discord:
geoip_log 1
```

After that, open **bot.config** file in a text editor and configure the bot with obtained credentials as described in that file.





### RUNNING
To run the server along with discord bot in a multi terminal environment you can do something like this:

```
# In terminal 1:
./sauer_server -glog < fifo

# In terminal 2:
python3 discord_bot
```

In case you want to run them as background processes, you can do something like:

```
nohup ./sauer_server -glog < fifo &
python3 discord_bot &
```
