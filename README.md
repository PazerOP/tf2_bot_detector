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
- [License](#license)
- [Contact](#contact)
- [Acknowledgements](#acknowledgements)
  - [Sponsors](#sponsors)


<!-- GETTING STARTED -->
## Getting Started

These instructions will give a quick overview of getting started with TF2BD.

### Prerequisites

>A note about 64 bit vs 32 bit: If your computer was made after 2011 it is 64 bit and you should use the 64 bit version.

This program requires [Microsoft Visual C++ Redistributable for Visual Studio 2015, 2017 and 2019][mscr-link] ([32bit version available here][mscr86-link])

### Installation

1. Go to the [releases page][releases-link]
2. Download the zip
   ![zip image][zip-image]
3. Extract the zip in any location outside of your tf directory

### How to use

1. Run tf2_bot_detector.exe before TF2
2. On first run it will ask you if you would like to allow internet connectivity and if you would like to be notified of future updates. Both are heavily recommended

>Note: The tool should automatically determine your steamID and tf2 directory. If for whatever reason they are not detected correctly you can override the generated values in the settings menu

### How to update

Currently the program does not self update. You can update the program by extracting the new zip over the existing folder. You could also delete the old version and follow the installation instructions again.

<!-- FAQ -->
## FAQ

### What does it do?

If a cheater is on your team, calls a votekick against them. If a known cheater is on the other team, sends a chat message telling the other team to kick their cheater.

### Will this get me VAC banned?

No. It does not modify the game or OS memory in any way. It is only using built-in functionality in the engine, exactly the way it was intended. Anecdotally, myself and several other friends have been using it for several weeks with no issues.

### How does it work?

It monitors the console output (saved to a log file) to get information about the game state. Invoking commands in the game is done via the `-hijack` command line paramter. Getting players in the current game is done via the `tf_lobby_debug` and `status` commands. Cheaters are identified by their behavior and/or their Steam ID.

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
<!--$5-->
* [Admiral Bread Crumbs](https://github.com/AdmiralBreadCrumbs)
* [ClusterConsultant](https://github.com/ClusterConsultant)
* [KTachibanaM](https://github.com/KTachibanaM)

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
[discord-link]: https://discord.gg/W8ZSh3Z
[mscr-link]: https://aka.ms/vs/16/release/vc_redist.x64.exe
[mscr86-link]: https://aka.ms/vs/16/release/vc_redist.x86.exe
[zip-image]: https://user-images.githubusercontent.com/6569500/85929969-8de89f00-b86d-11ea-859e-2632a1034ea7.png
[github-sponsors-pazerop]: https://github.com/sponsors/PazerOP
