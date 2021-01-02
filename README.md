<!-- PROJECT SHIELDS -->
<!--
*** I'm using markdown "reference style" links for readability.
*** Reference links are enclosed in brackets [ ] instead of parentheses ( ).
*** See the bottom of this document for the declaration of the reference variables
*** for contributors-url, forks-url, etc. This is an optional, concise syntax you may use.
*** https://www.markdownguide.org/basic-syntax/#reference-style-links
-->
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![MIT License][license-shield]][license-url]
[![Build Status][build-shield]][actions-build-url]
[![Discord Status][discord-shield]][discord-link]


<!-- PROJECT LOGO -->
<br />
<p align="center">
  <a href="https://github.com/PazerOP/tf2_bot_detector">
    <img src="https://raw.githubusercontent.com/PazerOP/tf2_bot_detector/master/tf2_bot_detector/Art/TF2BotDetector.ico" alt="Logo" width="100" height="100">
  </a>

  <h3 align="center">TF2 Bot Detector</h3>

  <p align="center">
    Automatically detects and votekicks cheaters/bots in TF2 casual.
    <!-- commented until there is documentation at the wiki
    <br />
    <a href="https://github.com/PazerOP/tf2_bot_detector/wiki"><strong>Explore the docs »</strong></a>
    <br />
    -->
    <br />
    <a href="https://tf2bd-util.pazer.us/Redirect/AppInstaller?source=https://tf2bd-util.pazer.us/AppInstaller/Public.msixbundle">Install</a>
    ·
    <a href="https://github.com/PazerOP/tf2_bot_detector/issues/new?assignees=&labels=Priority%3A+Medium%2C+Type%3A+Bug&template=bug_report.md&title=%5BBUG%5D">Report a Bug</a>
    ·
    <a href="https://github.com/PazerOP/tf2_bot_detector/issues/new?assignees=&labels=Priority%3A+Low%2C+Type%3A+Enhancement&template=feature_request.md&title=">Request a Feature</a>
    ·
    <a href="https://discord.gg/W8ZSh3Z">Join the Discord</a>
  </p>
</p>

<!-- TABLE OF CONTENTS -->
## Table of Contents

- [Table of Contents](#table-of-contents)
- [Installation](#installation)
  - ["One-Click" Installation (recommended)](#one-click-installation-recommended)
  - [Manual/Portable Install](#manualportable-install)
  - [Advanced Installation](#advanced-installation)
- [General Usage](#general-usage)
  - [How to launch TF2BD](#how-to-launch-tf2bd)
  - [First run](#first-run)
  - [Using TF2BD](#using-tf2bd)
  - [How to update](#how-to-update)
- [Common Tweaks](#common-tweaks)
- [FAQ](#faq)
  - [What is TF2 Bot Detector?](#what-is-tf2-bot-detector)
  - [What ISN'T TF2 Bot Detector?](#what-isnt-tf2-bot-detector)
  - [Will this get me VAC banned?](#will-this-get-me-vac-banned)
  - [How does it work?](#how-does-it-work)
  - [How is the list of known cheaters curated?](#how-is-the-list-of-known-cheaters-curated)
  - [I don't like how the tool spams chat. Can I change that?](#i-dont-like-how-the-tool-spams-chat-can-i-change-that)
  - [I downloaded the tool but I don't see an executable. What went wrong?](#i-downloaded-the-tool-but-i-dont-see-an-executable-what-went-wrong)
  - [This doesn't detect anything!](#this-doesnt-detect-anything)
  - [Help! The tool wont open!](#help-the-tool-wont-open)
  - [What do you think of using aimbot/cathook/esp/bots/cheats against the bots?](#what-do-you-think-of-using-aimbotcathookespbotscheats-against-the-bots)
  - [I have a question that is not listed here!](#i-have-a-question-that-is-not-listed-here)
- [License](#license)
- [Contact](#contact)
- [Acknowledgements](#acknowledgements)
  - [Sponsors](#sponsors)


<!-- GETTING STARTED -->
## Installation

### "One-Click" Installation (recommended)

If you are on Windows 10 1809 or newer, just click this link: [Install][msix-install-link]

### Manual/Portable Install

1. Download and install the [Microsoft Visual C++ Redistributable for Visual Studio 2015, 2017 and 2019][mscr-link]
2. Download the [latest release][latest-64]
3. Extract the zip in any location inside of your user folder (e.g. Downloads, Documents, Desktop, etc)

### Advanced Installation

If you would like more information about installation or the above options are not quite what you need, visit the more detailed and technical [installation wiki][installation-wiki]

## General Usage

### How to launch TF2BD
If you are using the installed version simply launch the app from the shortcut found in your start menu.

If you are using the portable version double click `tf2_bot_detector.exe` in the folder you downloaded from the installation section.

### First run
When you first run the tool it will ask if you would like to allow internet connectivity and which update channel you would like. These can be changed later in the settings menu. The recommended options are automatically selected and you may just click next.

![first-launch]

It is **strongly** recommended to set up your Steam Web API key. More information can be found [here][api-wiki].

> Note: If TF2BD is unable to find your Steam folder, your TF2's tf folder, or your Steam ID, it will ask you for the missing information before anything else

### Using TF2BD

You can right click any entry in the TF2BD scoreboard to mark or unmark them. If they are on your team, you can also vote kick them from here without having to mark them.

![right-click]

If you have set up your Steam Web API (instructions [here][api-wiki]) you can hover over entries in the TF2BD you will be presented with a window showing Steam Community information about that account.

![hover]

You can quickly pause all functionality (chat warnings, vote-kicks, auto-marks, etc) by checking the "Pause" box.

![pause-box]

There are several of these check boxes, hover over them for more information.
![chat-warning]

### How to update

With default settings TF2BD can self update. When the tool opens it will check for updates, if there is an update in your release channel you will be prompted to either update or continue without updating.

![update-image]

>Note: Player lists and rule sets are updated separately from the tool itself. As long as internet connectivity is allowed those lists will be updated

## Common Tweaks

TF2BD allows for a fair amount of customization (with even more planned). Currently the most popular community tweak is the addition of community rule lists and player lists. More information about these can be found [here][customization-wiki].

> **Community lists are not maintained by this project!** While being on the official project wiki implies endorsement by the project, ultimately we cannot guarantee quality, accuracy, or truthfulness of any player lists or rule sets. Please use responsibly.

If you are an advanced user you are more than welcome to create your own lists. You should be familiar with json in general before getting started but there are many people in the [discord][discord-link] who would be happy to help.

<!-- FAQ -->
## FAQ

### What is TF2 Bot Detector?

TF2 Bot Detector is a standalone application that calls a votekick against known bots and cheaters on your team. If they are on the other team, it will send a chat message telling the other team to kick their cheater.

### What ISN'T TF2 Bot Detector?

TF2BD isn't a perfect solution. It does not (and can not) perfectly detect every single cheater. TF2BD also isn't finished. This is an ongoing project that will continue to be improved with new features, more powerful detection, and better customization.

### Why would I want this? It's easy to identify and kick bots!

While true, this automates the process to a large degree. It also makes identifying name stealing bots trivial. With the addition of maintaining a data base of cheaters you have encountered you can "remember" any and all cheaters you have encountered. This is handy to identify the more subtle human cheaters.

### Will this get me VAC banned?

No. It does not modify the game or OS memory in any way. It is only using built-in functionality in the engine, *exactly* the way it was intended. Anecdotally, many users have been using this tool for many months now without issue.

### How does it work?

It monitors the console output (saved to a log file) to get information about the game state. Invoking commands in the game is done via passing rcon commands to your client. Getting players in the current game is done via the `tf_lobby_debug` and `status` commands. Cheaters are identified by some rules but primarily by comparing players steamIDs against a list of known cheaters.

### How is the list of known cheaters curated?

The official list that is included with the program is maintained by Pazer exclusively. No user submissions are accepted at this time and it is unlikely that they will be in the future. While this approach this may seem limiting, it is to avoid false positives and to maintain the integrity of the project as a whole. There are some community player lists that can be added to your own detector. These are not maintained by Pazer. For more information on installing community lists go [here][wiki-customization-link].

### I don't like how the tool spams chat. Can I change that?

You can turn off chat warnings by unchecking the checkbox labeled "Enable Chat Warnings." By default if there are multiple tool users in the same server, a "Bot Leader" is chosen and only their tool will send messages. There is no other way to customize chat messages outside of editing the code yourself which is not advised as it will break some functionality. User message customization is currently in the works.

### I downloaded the tool but I don't see an executable. What went wrong?

You likely downloaded the source code instead of the actual tool. Make sure you are downloading one of the .ZIPs that is not labeled "Source Code." There are two of them, one labeled with an x86 and one with an x64. If you don't know which one you want, you almost certainly want the one with the x64. For further instructions go [here][getting-started-wiki].

### This doesn't detect anything!

Right now the base player list and rule set is very very limited due to an abundance of caution. There are third party community lists and rules that are run by people who are not Pazer. While the general community trusts them, it is important to remember that these are not official resources and not under direct control of this project. For more information on installing community lists go [here][wiki-customization-link].

### Help! The tool wont open!

Make sure you have [Microsoft Visual C++ Redistributable for Visual Studio 2015, 2017 and 2019][mscr-link] installed. If you just installed it make to to restart your computer after to finish the installation. If that does not work try following the steps located [here][getting-started-wiki].

For further assistance either open an [issue][issues-url] on github or join our [discord][discord-link] for faster, community based support.

### What do you think of using aimbot/cathook/esp/bots/cheats against the bots?

This project does not advocate for the use of cheating in any fashion. Putting aside the obvious moral issues with using cheats, that would introduce the possibility of a VAC ban. This project is committed to maintaining the safety of its users.

### I have a question that is not listed here!

Take a look at the [wiki][wiki-link]. There is not a ton there right now but that will be the location of all future documentation. If you can't find your answer there, stop by the [discord][discord-link].

<!-- LICENSE -->
## License

Distributed under the MIT License. See [`LICENSE`][license-url] for more information.

<!-- CONTACT -->
## Contact
Project Discord: [https://discord.gg/W8ZSh3Z][discord-link]

[Issues page][issues-link]

<!-- ACKNOWLEDGEMENTS -->
## Acknowledgements
* Code/concept by [Matt "pazer" Haynie](https://github.com/PazerOP/) - [@PazerFromSilver](https://twitter.com/PazerFromSilver)
* Artwork/icon by S-Purple - [@spurpleheart](https://twitter.com/spurpleheart) (NSFW)
* Documentation by [Nicholas "ClusterConsultant" Flamel](https://github.com/ClusterConsultant)

### Sponsors
Huge thanks to the people sponsoring this project via [GitHub Sponsors][github-sponsors-pazerop]:
<!--$10-->
* [Crazy Gunman](https://github.com/CrazyGunman2C4U)
* [camp3r101](https://github.com/camp3r101)
* [bgausden](https://github.com/bgausden)
* [Symthos](https://github.com/Symthos)

<!--$5-->
* [ClusterConsultant](https://github.com/ClusterConsultant)
* [Admiral Bread Crumbs](https://github.com/AdmiralBreadCrumbs)
* [Czechball](https://github.com/Czechball)
* [RedNightmare](https://github.com/RedNightmare)
* [Public Toilet](https://github.com/PoliteYeti)

<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[stars-shield]: https://img.shields.io/github/stars/PazerOP/tf2_bot_detector
[stars-url]: https://github.com/PazerOP/tf2_bot_detector/stargazers
[issues-shield]: https://img.shields.io/github/issues/PazerOP/tf2_bot_detector
[issues-url]: https://github.com/PazerOP/tf2_bot_detector/issues
[license-shield]: https://img.shields.io/github/license/PazerOP/tf2_bot_detector
[license-url]: https://github.com/PazerOP/tf2_bot_detector/blob/master/LICENSE
[actions-build-url]: https://github.com/PazerOP/tf2_bot_detector/actions?query=workflow%3Abuild
[build-shield]: https://github.com/PazerOP/tf2_bot_detector/workflows/build/badge.svg
[discord-shield]: https://img.shields.io/discord/716525494421553243?label=discord&logo=discord
[repo-link]: https://github.com/PazerOP/tf2_bot_detector
[wiki-link]: https://github.com/PazerOP/tf2_bot_detector/wiki
[issues-link]: https://github.com/PazerOP/tf2_bot_detector/issues
[releases-link]: https://github.com/PazerOP/tf2_bot_detector/releases
[latest-64]: https://pazerop.github.io/tf2_bot_detector/releases_redirect/?cpu=x64
[latest-86]: https://pazerop.github.io/tf2_bot_detector/releases_redirect/?cpu=x86
[discord-link]: https://discord.gg/W8ZSh3Z
[mscr-link]: https://aka.ms/vs/16/release/vc_redist.x64.exe
[mscr86-link]: https://aka.ms/vs/16/release/vc_redist.x86.exe
[msix-install-link]: https://tf2bd-util.pazer.us/Redirect/AppInstaller?source=https://tf2bd-util.pazer.us/AppInstaller/Public.msixbundle
[zip-image]: https://i.imgur.com/ZeCuUul.png
[github-sponsors-pazerop]: https://github.com/sponsors/PazerOP
[wiki-customization-link]: https://github.com/PazerOP/tf2_bot_detector/wiki/Customization#third-party-player-lists
[wiki-installation-link]: https://github.com/PazerOP/tf2_bot_detector/wiki/Getting-Started
[install-video]: https://www.youtube.com/watch?v=MbFDUmsUakQ
[getting-started-wiki]: https://github.com/PazerOP/tf2_bot_detector/wiki/Getting-Started
[right-click]: https://i.imgur.com/dOFnkNL.png
[hover]: https://user-images.githubusercontent.com/6569500/90000207-b8878f00-dc44-11ea-938a-8f802630703d.png
[pause-box]: https://i.imgur.com/oxTCNH2.png
[chat-warning]: https://i.imgur.com/5AJvZJG.png
[first-launch]: https://i.imgur.com/oic9NbS.png
[update-image]: https://i.imgur.com/q95NMVy.png
[installation-wiki]: https://github.com/PazerOP/tf2_bot_detector/wiki/Installation
[api-wiki]: https://github.com/PazerOP/tf2_bot_detector/wiki/Integrations:-Steam-API
