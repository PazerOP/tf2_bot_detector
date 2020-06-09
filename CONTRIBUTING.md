## Contributing to Cathook or other Nullworks Projects

### Prerequisites

- A working up-to-date cathook installation
- C++ knowledge
- Git knowledge
- Ability to ask for help (Feel free to create empty pull-request or PM a maintainer in [Telegram](https://t.me/nullworks))

### Setting up cathook

Cathook reduces its git repository size automatically by using a "shallow repository". This makes developing difficult by not providing branches and omitting the commit history.

The easiest way to undo this is by running the `developer` script located in `scripts`.

### Pull requests and Branches

In order to send code back to the official cathook repository, you must first create a copy of cathook on your github account ([fork](https://help.github.com/articles/creating-a-pull-request-from-a-fork/)) and then [create a pull request](https://help.github.com/articles/creating-a-pull-request-from-a-fork/) back to cathook.

Cathook developement is performed on multiple branches. Changes are then pull requested into master. By default, changes merged into master will not roll out to stable build users unless the `stable` tag is updated.

### Code-Style

The nullworks code-style can be found in `code-style.md`.

### Building

Cathook has already created the directory `build` where cmake files are located. By default, you can build cathook by running `cmake .. && make -j$(grep -c '^processor' /proc/cpuinfo)` in the `build` directory. You can then test your changes by using one of the attach scripts at your disposal.
