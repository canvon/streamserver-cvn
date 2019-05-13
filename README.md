# streamserver-cvn

A media streaming server (`cvn` aka `canvon` attempt),
which distributes audio/video **MPEG-TS** (MPEG _Transport Stream_)
read via pipe to HTTP streaming clients.

That is, you can produce a stream (e.g., a screencast from your Desktop)
with some other program, then feed it to streamserver-cvn which will
let other people access/see it by pointing their media player of choice
(e.g., *`mpv`*, *VLC*, ...) at an URL such as http://_YOUR\_IP\_HERE_:8000 .

It's based on *Qt*, but does not use or provide any GUI functionality. (yet?)
(Here it's just a way of having a full framework at one's fingertips
while using C++, plus optionally having an event loop easily.)


## Development environment

At the time of writing (2019-05-13),
the `streamserver-cvn` project is developed by `canvon`
on the operating system (OS) *Debian 9* (a GNU/Linux distribution),
with the C++ compiler *GCC `g++` 6.3.0*
and the cross-platform C++ application framework *Qt 5.7.1*.

Usually, the code is developed (written, auto-completed)
in the Integrated Development Environment (IDE) *Qt Creator (4.2.0)*,
but especially when working on another machine,
e.g. remotely via SSH (Secure Shell, remote terminal access),
the terminal (console) text editor *VIM (e.g., 8.0)*
is used instead.

When you happen to be using such an environment, too, compilation
and use should be working as expected. Lower versions (especially
of Qt or the C++ compiler) will likely not work.

For building in the development environment,
see [[BUILDING#building-in-development-environment]].


## Termux App on Android

Within the Termux App on the Android (mostly: smartphone, tablet, TV)
operating system, a Debian-like environment exists (even uses APT
and `dpkg`). Recently (as of mid-2019), the possibility of having
an *X11*-based graphical environment was added, which also brought along
precompiled *Qt 5* (see above), thus making use of `streamserver-cvn`
on my smartphone a theoretical possibility.

It turns out compiling the project was quite possible (after fixing
two missing includes, see commit 17e33b2eee7ba94673c39adbab0940f642045282),
and the resulting binaries worked. (Initial test made 2019-05-11.)

`streamserver-cvn` was tested with these versions:
* Android 7
* Termux 0.68
* clang 8.0.0
* Qt 5.11.2

For building in Termux, see [[BUILDING#building-in-termux]].

For producing compatible input for `streamserver-cvn`, you may try to
use the `mpv` video player in encoding mode; e.g.:

    scm/build-streamserver-cvn$ mpv --loop=inf ~/storage/.../FOO.mkv \
      --o=- --of=mpegts | ./streamserver-cvn-cli/streamserver-cvn-cli \
      /dev/fd/0

Note the `--o=-` which enables encoding mode with output to stdout,
and the `--of=mpegts` which specifies the output format. The `mpv` output
is then piped into `streamserver-cvn-cli`, which is instructed to
serve its HTTP stream from `/dev/fd/0`, otherwise known as stdin.
