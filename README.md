# TF2 Bot Detector
Automatically detects and votekicks cheaters/bots in TF2 casual.

![Example Screenshot](https://user-images.githubusercontent.com/6569500/83914141-7e21e300-a725-11ea-9686-1b38cbbc35af.png)

### What does it do?
If a cheater is on your team, calls a votekick against them. If a cheater is on the other team, sends a chat message telling the other team to kick their cheater.

### How does it work?
It monitors the console output (saved to a log file) to get information about the game state. Invoking commands in the game is done via the `-hijack` command line paramter. Getting players in the current game is done via the `tf_lobby_debug` and `status` commands. Cheaters are identified by their behavior and/or their Steam ID.

### Will this get me VAC banned?
No. It does not modify the game or OS memory in any way. It is only using built-in functionality in the engine, exactly the way it was intended. Anecdotally, myself and several other friends have been using it for several weeks with no issues.

### How do I use it?
Just download the latest release and extract it somewhere (preferably not in your tf folder) and open the tool. It will prompt you to enter your Steam ID and the path to your tf folder. After that, running the tool and the game at the same time is all you need to do.

## Troubleshooting

### Sometimes I get these popups!

![src_engine_declined_request](https://user-images.githubusercontent.com/6569500/83913792-dc01fb00-a724-11ea-83c4-7b5aab364611.png)
![src_engine_not_running](https://user-images.githubusercontent.com/6569500/83913866-fc31ba00-a724-11ea-914c-5b36c9188d8c.png)

Ignore these. A fix to close these dialogs automatically will happen in an upcoming release.
