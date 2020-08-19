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
    <a href="https://github.com/PazerOP/tf2_bot_detector/issues">Report a Bug</a>
    ·
    <a href="https://github.com/PazerOP/tf2_bot_detector/issues">Request a Feature</a>
    ·
    <a href="https://discord.gg/W8ZSh3Z">Join the Discord</a>
  </p>
</p>

<!-- TABLE OF CONTENTS -->
## Table of Contents

- [Table of Contents](#table-of-contents)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Installation](#installation)
  - [How to use](#how-to-use)
  - [How to update](#how-to-update)
- [FAQ](#faq)
  - [What does it do?](#what-does-it-do)
  - [Will this get me VAC banned?](#will-this-get-me-vac-banned)
  - [How does it work?](#how-does-it-work)
  - [How is the list of known cheaters curated?](#how-is-the-list-of-known-cheaters-curated)
  - [I don't like how the tool spams chat. Can I change that?](#i-dont-like-how-the-tool-spams-chat-can-i-change-that)
  - [I downloaded the tool but I don't see an executable. What did I do wrong?](#i-downloaded-the-tool-but-i-dont-see-an-executable-what-did-i-do-wrong)
  - [Help! The tool wont open!](#help-the-tool-wont-open)
  - [What do you think of using aimbot/cathook/esp/cheats against the bots?](#what-do-you-think-of-using-aimbotcathookespcheats-against-the-bots)
  - [I have a question that is not listed here!](#i-have-a-question-that-is-not-listed-here)
- [License](#license)
- [Contact](#contact)
- [Acknowledgements](#acknowledgements)
  - [Sponsors](#sponsors)


<!-- GETTING STARTED -->
## Getting Started

These instructions will give a quick overview of getting started with TF2BD. There is also a (very slightly out of date, but still useable) video by CrazyGunman#6724 [here][install-video].

### Prerequisites

>A note about 64 bit vs 32 bit: If your computer was made after 2011 it is 64 bit and you should use the 64 bit version.

This program requires [Microsoft Visual C++ Redistributable for Visual Studio 2015, 2017 and 2019][mscr-link] ([32bit version available here][mscr86-link])

### Installation

1. Download and install the [Microsoft Visual C++ Redistributable for Visual Studio 2015, 2017 and 2019][mscr-link]
2. Download the [latest release][latest-64] ([32bit version][latest-86])
3. Extract the zip in any location inside of your user folder (e.g. Downloads, Documents)

### How to use

1. Run tf2_bot_detector.exe before TF2
2. On first run it will ask you if you would like to allow internet connectivity and if you would like to be notified of future updates. Both are highly recommended

>Note: The tool should automatically determine your steamID and tf2 directory. If for whatever reason they are not detected correctly you can override the generated values in the settings menu

### How to update

Currently the program does not self update. You can update the program by extracting the new zip over the existing folder. You could also delete the old version and follow the installation instructions again.

<!-- FAQ -->
## FAQ

### What does it do?

TF2 Bot Detector calls a votekick against bots (and select human cheaters) on your team. If they are on the other team, sends a chat message telling the other team to kick their cheater.

### Will this get me VAC banned?

No. It does not modify the game or OS memory in any way. It is only using built-in functionality in the engine, exactly the way it was intended. Anecdotally, many users have been using this tool for a few months now without issue.

### How does it work?

It monitors the console output (saved to a log file) to get information about the game state. Invoking commands in the game is done via passing rcon commands to your client. Getting players in the current game is done via the `tf_lobby_debug` and `status` commands. Cheaters are identified by some rules but primarily by comparing players steamIDs against a list of known cheaters.

### How is the list of known cheaters curated?

The official list that is included with the program is maintained by Pazer exclusively. No user submissions are accepted at this time and it is unlikely that they will be in the future. While this approach this may seem limiting, it is to avoid false positives and to maintain the integrity of the project as a whole. There are some community player lists that can be added to your own detector. These are not maintained by Pazer. For more information on installing community lists go [here][wiki-customization-link].

### I don't like how the tool spams chat. Can I change that?

You can turn off chat warnings by unchecking the checkbox labeled "Enable Chat Warnings." By default if there are multiple tool users in the same server a "Bot Leader" is chosen and only their tool will send messages. There is no other way to customize chat messages outside of editing the code yourself.

### I downloaded the tool but I don't see an executable. What did I do wrong?

You likely downloaded the source code instead of the actual tool. Make sure you are downloading one of the .ZIPs that is not labeled "Source Code." There are two of them, one labeled with an x86 and one with an x64. If you don't know which one you want, you almost certainly want the one with the x64. For further instructions go [here][wiki-installation-link].

### Help! The tool wont open!

Make sure you have [Microsoft Visual C++ Redistributable for Visual Studio 2015, 2017 and 2019][mscr-link] installed. If you just installed it make to to restart your computer after to finish the installation. For further assistance either open an [issue][issues-url] on github or join our [discord][discord-link] for faster, community based support.

### What do you think of using aimbot/cathook/esp/cheats against the bots?

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
* [kajutzu](https://github.com/flohdieter)
* [minecraftpro123](https://github.com/Claxtian)
<!--$5-->
* [ClusterConsultant](https://github.com/ClusterConsultant)
* [KTachibanaM](https://github.com/KTachibanaM)
* [Admiral Bread Crumbs](https://github.com/AdmiralBreadCrumbs)
* [Czechball](https://github.com/Czechball)
* [moeb](https://github.com/moebkun)

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
[zip-image]: https://i.imgur.com/ZeCuUul.png
[github-sponsors-pazerop]: https://github.com/sponsors/PazerOP
[wiki-customization-link]: https://github.com/PazerOP/tf2_bot_detector/wiki/Customization#third-party-player-lists
[wiki-installation-link]: https://github.com/PazerOP/tf2_bot_detector/wiki/Getting-Started
[install-video]: https://www.youtube.com/watch?v=MbFDUmsUakQ
