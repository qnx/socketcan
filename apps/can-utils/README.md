# can-utils

A "port" of the can-utils project to work with QNX SocketCAN (as exposed by the drivers in this repo).

No changes to the can-utils source code were made, but several headers that it expects to find were added. These mostly just thunk through to the actual headers/values used on QNX.

At the time of writing, both cangen and candump were used and seem to work well. Other utilities will also likely work, as long as they compile.

# Sync'ing

can-utils is a submodule of the larger SocketCAN repo. If you didn't initialize SocketCAN with `--recursive` you can do it after the fact by running:
```
git submodule update --init
```
in you local SocketCAN repo.

# Building
can-utils uses CMake for its build. A convenience script, `configure.sh` has been included to call CMake with the correct parameters to configure a build for QNX. The only argument it requires is the target architecture, so it can pick the correct toolchain file. It should be one of either 'aarch64' or 'x86_64'.

To create the build, source your SDP,  make a build directory, change into it and from there run the configure script.

For example:
```
source <path_to_sdp>/qnxsdp-env.sh
mkdir build-aarch64
cd build-aarch64
../configure.sh aarch64
```

Then just execute the build as normal:
```
make
```

