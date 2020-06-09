#!/usr/bin/env bash

# Credit to LWSS for doing this shit in Fuzion, looked and pasted a stuff from his preload meme

# This assumes the presence of the ~/.steam symlink
GAMEROOT=$(realpath $HOME/.steam/steam/steamapps/common/Team\ Fortress\ 2)
GAMEEXE="hl2_linux"

if [ ! -d "$GAMEROOT" ]; then
    echo -e "\e[91mCould not find Team Fortress 2 directory. Check if Steam symlink at $HOME/.steam exists.\e[39m"
    exit -1
fi

export LD_LIBRARY_PATH="$GAMEROOT"\
":$GAMEROOT/bin"\
":$GAMEROOT/bin/linux32"\
":$GAMEROOT/tf/bin"\
":$HOME/.steam/ubuntu12_32"\
":$HOME/.steam/ubuntu12_32/linux32"\
":$HOME/.steam/ubuntu12_32/panorama"\
":$HOME/.steam/bin32"\
":$HOME/.steam/bin32/linux32"\
":$HOME/.steam/bin32/panorama"\
":/lib/i386-linux-gnu"\
":/usr/lib/i386-linux-gnu/"\
":/usr/lib64/atlas"\
":/usr/lib64/bind99"\
":/usr/lib64/iscsi"\
":/usr/lib64/nx"\
":/usr/lib64/qt-3.3/lib"\
":/usr/lib"\
":/usr/lib32"\
":/lib64"\
":/lib32"\
":/usr/lib/i686"\
":/usr/lib/sse2"\
":/lib64/tls"\
":/lib64/sse2"\
":/usr/lib/steam"\
":/usr/lib32/steam"\
":$HOME/.steam/ubuntu12_32/steam-runtime/i386/usr/lib/i386-linux-gnu"\
":$HOME/.steam/ubuntu12_32/steam-runtime/i386/lib/i386-linux-gnu"\
":$HOME/.steam/ubuntu12_32/steam-runtime/amd64/lib/x86_64-linux-gnu"\
":$HOME/.steam/ubuntu12_32/steam-runtime/amd64/lib"\
":$HOME/.steam/ubuntu12_32/steam-runtime/amd64/usr/lib/x86_64-linux-gnu"\
":$HOME/.steam/ubuntu12_32/steam-runtime/amd64/usr/lib"\
":$HOME/.steam/ubuntu12_32/steam-runtime/i386/lib/i386-linux-gnu"\
":$HOME/.steam/ubuntu12_32/steam-runtime/i386/lib"\
":$HOME/.steam/ubuntu12_32/steam-runtime/i386/usr/lib/i386-linux-gnu"\
":$HOME/.steam/ubuntu12_32/steam-runtime/i386/usr/lib"\
":$HOME/.steam/ubuntu12_32/steam-runtime/amd64/lib/x86_64-linux-gnu"\
":$HOME/.steam/ubuntu12_32/steam-runtime/amd64/lib"\
":$HOME/.steam/ubuntu12_32/steam-runtime/amd64/usr/lib/x86_64-linux-gnu"\
":$HOME/.steam/ubuntu12_32/steam-runtime/amd64/usr/lib"\
":$HOME/.steam/ubuntu12_64"\
":$HOME/.steam/bin32/steam-runtime/i386/usr/lib/i386-linux-gnu"\
":$HOME/.steam/bin32/steam-runtime/i386/lib/i386-linux-gnu"\
":$HOME/.steam/bin32/steam-runtime/amd64/lib/x86_64-linux-gnu"\
":$HOME/.steam/bin32/steam-runtime/amd64/lib"\
":$HOME/.steam/bin32/steam-runtime/amd64/usr/lib/x86_64-linux-gnu"\
":$HOME/.steam/bin32/steam-runtime/amd64/usr/lib"\
":$HOME/.steam/bin32/steam-runtime/i386/lib/i386-linux-gnu"\
":$HOME/.steam/bin32/steam-runtime/i386/lib"\
":$HOME/.steam/bin32/steam-runtime/i386/usr/lib/i386-linux-gnu"\
":$HOME/.steam/bin32/steam-runtime/i386/usr/lib"\
":$HOME/.steam/bin32/steam-runtime/amd64/lib/x86_64-linux-gnu"\
":$HOME/.steam/bin32/steam-runtime/amd64/lib"\
":$HOME/.steam/bin32/steam-runtime/amd64/usr/lib/x86_64-linux-gnu"\
":$HOME/.steam/bin32/steam-runtime/amd64/usr/lib"\
":$HOME/.steam/bin64"\
":$HOME/.steam/bin"

# Set file descriptor limit
ulimit -n 2048

export LD_PRELOAD
LD_PRELOAD="$(realpath .)/bin/libcathook.so:/usr/lib/i386-linux-gnu/libstdc++.so.6:/usr/lib32/libstdc++.so.6:launcher.so"

# Enable nVidia threaded optimizations
export __GL_THREADED_OPTIMIZATIONS=1
# Enable Mesa threaded shader compiles
export multithread_glsl_compiler=1

cd "$GAMEROOT"

STATUS=42
while [ $STATUS -eq 42 ]; do
	if [ "${DEBUGGER}" == "gdb" ] || [ "${DEBUGGER}" == "cgdb" ]; then
	    echo -e "\e[92mSuccess! Launching TF2 using ${DEBUGGER} as debugger.\e[39m"

		ARGSFILE=$(mktemp "$(whoami)".hl2.gdb.XXXX)
		echo b main > "$ARGSFILE"

		# Set the LD_PRELOAD varname in the debugger, and unset the global version. This makes it so that
		#   gameoverlayrenderer.so and the other preload objects aren't loaded in our debugger's process.
		echo set env LD_PRELOAD=$LD_PRELOAD >> "$ARGSFILE"
		echo show env LD_PRELOAD >> "$ARGSFILE"
		unset LD_PRELOAD

		echo run $@ >> "$ARGSFILE"
		echo show args >> "$ARGSFILE"
		${DEBUGGER} "${GAMEROOT}"/${GAMEEXE} -x -steam -game "tf" "$ARGSFILE"
		rm "$ARGSFILE"
	else
	    echo -e "\e[92mSuccess! Launching TF2.\e[39m"
		${DEBUGGER} "${GAMEROOT}"/${GAMEEXE} -steam -game "tf" "$@"
	fi
	STATUS=$?
done

exit ${STATUS}
