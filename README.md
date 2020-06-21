# TF2 Bot Detector
Automatically detects and votekicks cheaters/bots in TF2 casual.

**This program requires the [Microsoft Visual C++ Redistributable for Visual Studio 2015, 2017 and 2019](https://aka.ms/vs/16/release/vc_redist.x64.exe).**

Latest release: https://github.com/PazerOP/tf2_bot_detector/releases/latest

Support and chat discord: https://discord.gg/W8ZSh3Z

![Example Screenshot](https://user-images.githubusercontent.com/6569500/83914141-7e21e300-a725-11ea-9686-1b38cbbc35af.png)

### What does it do?
If a cheater is on your team, calls a votekick against them. If a cheater is on the other team, sends a chat message telling the other team to kick their cheater.

### How do I use it?
Just download the latest release and extract it somewhere (preferably not in your tf folder) and open the tool. It will prompt you to enter your Steam ID and the path to your tf folder. After that, running the tool and the game at the same time is all you need to do.

### Will this get me VAC banned?
No. It does not modify the game or OS memory in any way. It is only using built-in functionality in the engine, exactly the way it was intended. Anecdotally, myself and several other friends have been using it for several weeks with no issues.

### How does it work?
It monitors the console output (saved to a log file) to get information about the game state. Invoking commands in the game is done via the `-hijack` command line paramter. Getting players in the current game is done via the `tf_lobby_debug` and `status` commands. Cheaters are identified by their behavior and/or their Steam ID.

### Updates

This tool does not currently update automatically. Future releases may support that. For now, the latest release can always be found at https://github.com/PazerOP/tf2_bot_detector/releases/latest. New versions can simply be extracted over top of an existing version, overwriting all conflicting files. Your personal settings are stored in different files and will not be overwritten.

### Customization

Info about customizing the tool can be found here: https://github.com/PazerOP/tf2_bot_detector/wiki/Customization.

## Troubleshooting

You can always visit the discord to get support or suggest new features: https://discord.gg/W8ZSh3Z

### Sometimes I get these popups!

![src_engine_declined_request](https://user-images.githubusercontent.com/6569500/83913792-dc01fb00-a724-11ea-83c4-7b5aab364611.png)
![src_engine_not_running](https://user-images.githubusercontent.com/6569500/83913866-fc31ba00-a724-11ea-914c-5b36c9188d8c.png)

Ignore these. They will automatically close after 5 seconds. A fix to prevent these popups entirely will happen in an upcoming release.

## Credits

Code/concept by Matt "pazer" Haynie: https://twitter.com/PazerFromSilver<br>
Artwork/icon by S-Purple: https://twitter.com/spurpleheart (NSFW)
