# Build streamserver-cvn

This document describes steps to be taken when building `streamserver-cvn`,
a media streaming server, `canvon` attempt. For general information,
see the [[README]].


## Building in development environment

If you want to build in the terminal (console),
without the use of Qt Creator, follow these steps:

1. Clone this git repository (if not already done so).

    scm$ git clone https://.../streamserver-cvn.git

   Further steps assume the repository was
   cloned to directory `streamserver-cvn`.

2. Create an empty directory **outside** the git working copy,
   which will be used as external build directory.

    scm$ mkdir build-streamserver-cvn

3. Change current working directory to the build directory just created.

    scm$ cd build-streamserver-cvn
    scm/build-streamserver-cvn$

4. Run `qmake`, which sets up the `Makefile`.

    scm/build-streamserver-cvn$ qmake ../streamserver-cvn

   Note that, at this step, additional flags may be passed to qmake,
   like `DEFINES+=...` or `QMAKE_CXXFLAGS+=...`.
   To be sure that all those flags will be in effect during build,
   recreate the build directory (by deleting it and starting again
   at step 2./mkdir; or simply create a new one with a different name)
   if you'll ever need to change those flags.

5. Run `make`, which does the real build.

    scm/build-streamserver-cvn$ make

   Note that, at this step, additional flags may be given as well:

   E.g., to fully utilize a 4-core, 8-thread CPU, use `make -j8`, instead.
   This can speed up the perceived build speed considerably.
   On the other hand, this can make it hard to read compiler error messages
   and attribute them to the correct source file, as both directly before
   and directly after the compiler message, there may be unrelated
   other messages from make or another compiler instance.

That's it! You should now be able to
start the `./streamserver-cvn-cli/streamserver-cvn-cli FOO.ts`
which will stream the file (or data from the named pipe) `FOO.ts`
(which has to be in `MPEG-TS` (MPEG *Transport Stream*) format)
and make it available as http://_YOUR\_IP\_HERE_:8000 .

You can also run `ts-dump`, first. An MPEG-TS packet with the correct size
and starting with a sync-byte, but otherwise invalid data, is provided
in the repository in `testdata/one-empty-packet.bin`.

    scm/build-streamserver-cvn$ ./ts-dump/ts-dump ../streamserver-cvn/testdata/one-empty-packet.bin

It is expected to dump the packet and report an error in the packet.


## Building in Termux

In Termux, you'll need to first install the `x11-repo` package
(which adds a file in `../usr/etc/apt/sources.list.d/`).
You'll then install package `qt5-base-dev` from that other repos.

Then follow the steps as for [[#building-in-development-environment]],
except for (**N.B.: This assumes your Android device is ARM-based!**)
at step 4./qmake, pass an option to the C++ compiler (`clang`)
in the following way:

    scm/build-streamserver-cvn$ qmake QMAKE_CXXFLAGS+=--target=armv7l-linux-android ../streamserver-cvn

This raises the ARM architecture from 4 (unsupported by Qt as being
too old) to 7:

    $ echo "__ARM_ARCH" | cc -E - | tail -1
    4
    $ echo "__ARM_ARCH" | cc --target=armv7l-linux-android -E - | tail -1
    7

