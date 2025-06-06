G2CD - Gnutella 2 Server Daemon

Copyright (c) 2004-2015 Jan Seiffert  ->  kaffeemonster at googlemail dot com

This file is part of g2cd.

g2cd is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as
published by the Free Software Foundation, either version 3
of the License, or (at your option) any later version.

g2cd is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public
License along with g2cd.
If not, see (http://www.gnu.org/licenses/).

# 0. Introduction
This is a server-only implementation for the Gnutella 2 P2P-protocol
(g2.trillinux.org).

Gnutella and Gnutella 2 are self-organizing, "server-less" P2P-networks.
But, to maintain network-connectivity, some network nodes become,
dynamicaly, special nodes (Ultra-peers/Hubs).

The main idea behind this piece of software is to be able to set up a
dedicated Gnutella 2 Hub to maintain the G2 network connectivity.
With its ability to run on a typical server grade machine
(UNIX® system / 24-7 / static IP / high Bandwidth / SMP), it will
hopefully be able to improve the overall Gnutella 2 performance.

# 1. Status
This is very alpha state software.

It is able to:
* run, spawn several threads
* accept incoming connections
* handshake Gnutella 2 communication
* parse the packet-stream
* answer packets, searches, etc
but there is still a lot to do...

# 2. Setup

## 2.0 Installation

This is just called "Intallation" for convenience, there is just "nothing"
(proper, functioning, full-fledged deamon) to install, this is more a
description how to compile and run it.

### 2.0.0 System requirements

What you need:
* A typical UNIX® system (as in IEEE Std 1003.1 (POSIX), ISO/IEC 9945)
actually Linux®, \*BSD, Solaris® recommended (the poll-based network
event notification is just for prototyping, prop. broken)
* GCC as your compiler (A C99 capable C-Compiler and runtime system
is the minimum, but some GCC extensions are unfortunately very handy)
* gmake (GNU-make)
(note: it also works with a prehistoric Solaris make without any
fancy extensions, I'm pretty sure it should work with a
POSIX compliant make, but it does _not_ work with the FreeBSD
make wich sets `$@` wrong for libraries \(non-POSIX conforming\),
so i simply recomment gmake)
* Your systems header files (libc etc.)
* The zlib, library and header-files
* A dbm lib, library and header-files (Berkeley DB or something else
which provides `dbm_*` functions commonly available by `ndbm.h`)
(note: yes, the `dbm_*` interface is "obsolete", but it seems to
be widely available. It suits my simple needs and i found
it also provided on an old Solaris as system lib, with AT&T
copyright in the header, last change 1996, probably to
add `extern "C"`, so i would call it mature technology ;))

*highly recommended:*
* libcurl >= Version 7.19.4
(note: From time to time there may be the need to connect to a
Gnutella Web Cache (GWC) by http. A simple http connection
is very simple to establish, but the http protocol holds
lots of more complicated things in store (encodings, compression,
redirects, encryption, authentification, proxys, etc.).
For that reason the GWC-specification **demands** a **proper**
http client is used, not some home grown quick hack, because
otherwise harm for the GWC could ensue (by you hammering them
with useless/broken requests).  
libcurl is a everything and the kitchensink bloat, but it is
a proper http client, and so it's use is highly recommended)

recommended:
* An Editor
* Programming skills
* Using-the-system skills
* Brain 1.0

### 2.0.1 "Real" Install

G2CD can now also be installed "for real". `./configure` the source like
other packages (`--prefix`, etc), but add `--enable-release`, then compile,
then use `make install`.

`make DESTDIR="..."` install is supported.  
You may have to adapt the permissions on `/var/cache/g2cd`.

### 2.0.2 Additional blurp

What to do:  
First of all unpack the .tar.bz2 to some place.

Then you can try if it will compile on your system. Luckily we now
support the classic UNIX `./configure; make`

I am no wizard at autoconf, and just stuffed together some problems i
know where compilation can go wrong. The tests may even be brittle, but
better then nothing and to make your live easier (most of the time ;).

Check the output of configure a little bit. And have a look at the
compile messages (i silenced the build a little bit so you can see real
messages more clearly).

If you believe in wonders, there is a chance this piece of software will
simply compile on your machine.

You're kidding? But now, with this little test, there is a high chance,
that you have some error-messages from the compiler or configure, and,
using the recommended programming skills from above, fix it, until it
compiles ;)

Ok, since I'm a real buddy, first some "expert"-tips where to search
first. Hopefully with these even someone new to programming will get it
also working (muhahaha).
* Move your recommended editor to the `config_auto.make` file. Most people
  will simply have problems when some parameter or program may not fit
  their enviroment. This file is the output of configure! So be carefull,
  it may get overwritten by configure again. If this solves your problem,
  the question is why autoconf didn't do it right for you.
* If the "setup" did not do the trick for you, you now really should
  have some programming experience, the ability to understand
  compiler-error-msg. (hahaha). You may work you're way down to what
  failed, it may prop. be something in the "compat" logic. Look for ifdef
  inside the code and what failed and what's in `config_auto.h` \(also
  `./configure` output\), maybe a search through your system header can
  help to find the missing magic knob.
* If you read this, you are on your own... You have to fix it, alone.
  work-around's go into `other.h` (mostly), the switches for them into
  `config.h` or better into autoconf (`configure.ac`). Don't forget to diff
  and send it to me. Oh, and i like my diffs in "-u".

If you had success compiling it, you are nearly ready to start.

ATM g2cd will "stay" in it's directory and there is no install. This is
for easier testing so you can start the app right from where you are.

Fine, ok, whats next? Oh, sure, start it. (You know -> `./g2cd`)

Be warned that g2cd may now crash on you.

If it does not crash, what now?  
There are not many things to see, until some G2-Client connects, and as
long as this software is so limited, this surely will not happen by
accident. So grep a G2-client and connect to your new "hub" (side note:
Warning! This software listens on port 5000 on all interfaces in your
machine. Your development machine isn't directly connected to the
Internet?). I personally use Shareaza with Wine. If you connect, you
should see some debug-text (Edit: no, you will not see anything, this
gets noisy with lots of connections, i turned it off...).

To stop it, use `CTRL-C`, you should see it going down.

You could check out the config file `g2cd.conf` to look at/change the
configuration (be carefull, this file can be overwritten if you do
development, you maybe want to keep a backup of it in this case).

It is recommended you configure a guid and some profile things for your
hub to make it unique. Refer to the config file how to do so. If you do
not want to generate a guid by hand, you can simply start g2cd the first
time without a guid, it will generate one and print it. Then you can c&p
this guid into your config

OK, not impressed?  
Look at the Version number... now it's your turn. Again, fire up your
editor, use your programming skills, write some code to accomplish the
needed functionality, send the `diff -u` to me. That's how it looks
like at time.

---

N.B.:  
If you think this is all to complicated and hard, please note that this
software is only narrowly tested. So there are enough problems down the
road to make your life miserable.

And to make matters worse, i was to clever for my own good, and build
in a lot of complicated parts.  
So it is even very likely the software will simply crash on you or
create broken output (if it survives that...).

Yes, it is alpha software, but more problematic: portability.
You can run into nasty compiler problems, the Intel Compiler Version
i tried simply crashed on the code, the Sun Compiler generated brocken
code in several places (yes, after disabling a lot of cleverness even
with -O0), clang/llvm being the only shining example by working
sometimes, most of the time it also generates some broken code around
inline asm, if not crashing all by itself. Did i mention binutils
changing something in the address calculations, so now some jumps where
suddenly off by 3 bytes?

And then you can run into other stuff (ex. crash on FreeBSD because
of an to early `write(2)`, prop. before the threading lib was completely
started, maybe wrong linking?).

I can not vouch for all versions of GCC, but at least with all versions
i tested it didn't let me down. The last test with 2.x was loooong ago.
4.6 and 4.8.4 get tested on my systems (older versions before them) and
my collection of cross compiler (ia64, powerpc, powerpc64, arm, alpha).

Apropros other archs: I can only test x86, and that only on older systems.
\<irony\>So i deliberatly deposited some broken code for other archs and
those guys with their new x86 (AVX) to trip over.\</irony\>

You can disable most of this brokenness and activate some workarounds
with configure, but not all of it...

## 2.1 Misc Install

G2CD does not bootstrap itself into the G2 network ATM. Besides that
the code needs to be written, this allows testing on a single computer
without disturbing the network "out there".

You can bootstrap "by hand" with a http client and the URL of a
Gnutella Web Cache. This way you will attract leaves and sooner or
later other hubs.

An example how to do that with wget:  
```sh
wget -v -S -U G2CD -O - "<some_gwc_url>?update=1&ip=<your_ip>:<your_port>&x_leaves=0&net=gnutella2&client=G2CD&version=0.0.0.11&ping=1"
```

Because this is "good enough" for the moment there is not much
presure to write bootstrap code and giving up control over the
bootstrap.

For real test runs \(and/or real usage\), there're maybe things which
have to be configured on the system you want to use G2CD on.

### 2.1.0 WARNING, manual intervention needed!

> There are two important regions where maybe manual intervention by
> YOU is needed and which needs to be checked by YOU.

Please read the following two subsections.

### 2.1.1 Resources

The first issue are system rescources:  
* You have to make sure your internet connection has enough
  bandwidth to sustain the traffic generated by a G2 hub.
* Also make sure you are not firewalled, P2P-hampered by your
  ISP and are not running behind a NAT.  
  G2CD WILL NOT WORK BEHIND A NAT.
* Your CPU should be beefy enough but ESPECIALLY you should
  have enough RAM, the state of all the leave connections wants
  to be handeld.  
  Swap and you are dead...
* Another critical point are file handles. We need a file
  handle for every TCP connection + several other. Normally
  there is a hard limit on open file handles for normal users
  enforced by the kernel to prevent system DoS, This limit is
  system dependend but mostly 1024 on Linux®. You have to raise
  it if you want to use more than say 990 connections. Please
  refer to your system skills how to do so. (hint: /etc/limits).

But don't get fooled by this file handle number! There may be
mechanisms which still limit the number of open files. For ex.
Old Linux® Kernel didn't allow more than these 1024, even if
you raised the hard limit, it was simply a NOP.

Stories from the trenches:  
On one test system, which was a vserver, i could not open more
than 290 connections, prop. because the underlying host OS
prevented more \(DoS prevention?\). This sucked big time, because
it kicked me out of my ssh session, the system was basically
behaving like in a swap storm at that point, file handles got
serviced very flaky \(this vserver connection limiting seems to
be ... not very intelligent\).  
Luckily this dropped a lot of leave connections, so i managed
to relogin and stop G2CD and restarted with a lower connection
count.

And still you can get bitten by other foo:  
15 minutes later the same system crashed hard \(drop dead from
one moment to the next. You could not ping it or something else,
hard reset from the management console needed...\). After a little
back and forth and more hard lockups it seems G2CD was hammering
a locking primitive to hard \(it was a 4 way SMP, actually the
hammering was lite, there was no great congestion, but this one
primitive seems to be seldomly used...\). I could prevent that
problem by changing the locking of G2CD in the network region
a little bit (basically moving the lock from user space to
kernel space).

Outdated, patched to death, "Enterprise" systems, yummie!

Please refer to the config file for some more comments about
resources.

### 2.1.2 TCP/IP woes

The second issue is the internet "at large":
This problem surfaces esp. with Linux®, but other systems are not
immune to these effects. You maybe have to change some basic system
settings concerning the TCP/IP stack to make it "fly".

G2CD, by it's very purpose, accumulates a lot of TCP connections.
Since packet loss is expected and transparently rectified by TCP,
TCP/IP stacks try, in accordance to the standard, to resend _all_
lost (un-ACK-ed) TCP segments.

But there is a problem: DUN.  
Our connections are mostly to hosts with an unstable link on "dail
up networks" \(mind the quotes\). Unstable does not only mean the
classic "Who yanked the cable?"/"link got hung up", but the more
prevailing plagues of the modern internet:
* bad router/equipment (cheapo CPE, WiFi and other stuff)
* network mumbo jumbo \(braindead ISPs, l337 transparent proxys
and enhancers\)
* blackholes, broken routing 
* \(p2p overwhelmed\) NAT \(no comment...\), these days "carrier grade"
* flodded links \(asymmetric, no QOS, badly configured, zombie\)
* broken/misconfigured \(personal\) firewalls \(example WinXP SP2 FW:
unclean close \(WinAPI fudge, lost RST \(see flodded links...\)\),
still it goes silent, and thats the default on LOTS of systems\)
It's unbelivable how many broken systems are out there on the
internet.

These plagues, besides of other pain they inflict, tend to
_silently_ kill TCP connections. You start to accumulate dead
connections, and do not recognise it because the other end does
not even tell you you have a dead connection on TCP retries
\("ICMP, that's a super bad hacker protocol!" **facepalm**\).

But i get's worse: TCP shutdown also involves a handshake like on
connect, which means it needs a packet round trip. If one of those
plagues loses track of the TCP connection to early \("Hey, there was
a `FIN`, drop all recources associated with this connection"\), which
is _very_ likely, **TCP shutdown is stuck**.

Linux® does _not_ handle this very gracefully \(google for "linux
`FIN_WAIT2` stuck", and no, it's not the half closed problem, there
is no half close in G2CD, it's the `FIN/ACK` lost problem, you are
more likely to hit it the more connections to DUN hosts you have,
can also hit mailservers under zombie fire etc.\).

Due to exponential backoff on retries and absolutly no answer from
the other side, connections in this state tend to linger around for
a _very_ long time, eating kernel resources \(fd/socket limit, tcp
cache, timers etc.\), G2CD resources, and even bandwidth with all the
retries and traffic we still try to send to these connection because
we do not get an `RST/ACK/ICMP` port closed. \(see netstat for connections
with a large send-q or in state `FIN_WAIT2`\)

Note that the problem does not lie in the `tcp_fin_timeout`.

In essence you want to get a "connection reset by peer". There
are days where you wish you can still laugh about this joke: "I
want to find this peer, and then i will reset his connection".
If "peer" only would reset our connections...

The 15 retries configured by default on Linux® \(and/or other
systems\) may be standard conformant and were needed/good in the
70's, or for interstellar connections, but means these connections
will linger around for approx. 11 minutes. Maybe longer, up to
546 minutes \(9 days...\), depending on the backoff strategy.

Unfortunatly G2CD can do nothing about that, there is no per
connection setting we can frob. On Linux® YOU have to configure the
_system wide_ retry count in `/proc/sys/net/ipv4/tcp_retries2`.

A number of 6 or 7 retries is recommended and successfully tested
on the authors machines with no other adverse effect. You can make
this setting permanent by editing `/etc/sysctl.conf`.  
But beware: This is a system wide setting affecting all network
programms affecting all TCP retries. There are reports this causes
problems with backup solutions/rsync which seem to go unresponsive
for a long time due to disc IO.

There were patches to split the normal retries from the `FIN` retries,
but they "didn't made it". Also they just helped with "stuck in
`FIN_WAIT2`, not with dead connections in general.

As you can see: The only thing that helps you in both regions
is testing. "Hammering" a system like G2CD does (lots of rescource
usage, "massive" (lol) multithreading) turns up a lot of "hidden
limits" & problems, unfortunatly...

## 2.2 DBus

G2CD can connect to DBus to export some data at runtime about it's
status. For this you need to compile G2CD with DBus support \(should
happen automatically if you have the DBus develop stuff \(header, etc\)
installed\), enable it in the config file, and configure DBus to allow
connections from/to G2CD. The last part is normally done in an config
file in `/etc/dbus/system.d/whatever.conf`.  
Such a config file could look like this:

```xml
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
	<policy context="default">
		<allow own="org.g2cd"/>
		<allow send_destination="org.g2cd" send_interface="org.freedesktop.DBus.Introspectable"/>
		<allow send_destination="org.g2cd" send_interface="org.g2cd"/>
	</policy>
</busconfig>
```

But be carefull, this is just a sample, this config does not contain
any access restrictions, it is only good to open up the data exchange
during development. Also do not forget to restart DBus after creating
the config file.  
If everything is working you can query G2CD while it is running like so:

```sh
$ dbus-send --system --dest=org.g2cd --print-reply /org/g2cd org.g2cd.version method return sender=:1.72 -> dest=:1.73 reply_serial=2 string "0.0.00.11"`
```

Other Methods are `get_num_connections`, `get_num_hubs`, `list_hubs`,
`list_cons`.

I created this interface to get a little "easier" access to \(more\) info
at runtime than `SIGUSR{1|2}` would allow \(and to free up these two signals
again for more classical uses\). Easy in this context is relative, DBus
is not easy, but creating a home grown IPC/RPC sheme is BS in these days.
Also, this way maybe some runtime knobs could be implemented. Patches
welcome, but beware, i don't want to export/fiddle every internal of G2CD
to this interface.

# 3. Bugs / TODO
Main Bug: Program not finished
Second Bug: Hard to build/install
Other Bugs:
* seems not 100% SMP safe...
* there is a veeery nasty livelock in the timer code, by tree corruption.
I have not the faintest idea where it comes from \(locking safety or stray write error\)
* seldom crashes \(after 220h Uptime\)
* to be filled in

TODO:
* complete packet-handling
* more timeout-handling
* DoS prevention
* UDP query limiting
* bootstrap
* lots of other things
* really test the BSD-kqueue wrapper \(some BSD'ler around?\)
* really test Solaris-`/dev/poll` & event port wrapper
* portability issues of the build \(make, compiler, shell\)
* make the autoconf much better...
* lots of other stuff
* DBus interface is prop. buggy, deadlockable, eats your kitten, etc.

What's needed?  
Programming wizards, the tooth fairy and santa claus would be nice ;) 
An expert for autoconf to make the building simpler... but you have to
know, I'm not an autoconf fan \(i simply gobbeled something together to
give people at least a chance to compile it\).  
People with different systems, different linux-distris/unices on
different arch'es, to test the build and function.
* Some portability-gurus, checking the *code* for portability-problems.
* Some security-gurus checking for security concerns.

Scratch those last two points, some of it is deliberate, i don't need
evangelization, i need _solutions_ to _real_ problems.

## 3.1 What do my eyes see? WIN32, WTF???

You may find some `#ifdef WIN32` troughout the codebase. Does that mean
G2CD runs on Windows or is a port planed or what?

The simple answer is: **NO.**

The longer answer is:  
Because i'm a nice guy and it was a typical rainy april day, i sat down
and litteratly bulldozered down the five pages of errors from _every_
source file to only some "small" errors (often the reason for pages and
pages of compiler blurp is a simple mismatch).  
I did this because i know where the problems are and _maybe_ the best
way to fix them, maybe had an interresting read, and i wanted to cross
check my suspicions. Not because i really wanted to port the code.  
This is just a little work to get it through the compiler. So if
someone really feels the urge to port it (loco?), he does not have to
start at square one.  
After another round of "rainy day", and to bring this in sync with the
config foo work i've done, it now compiles with (modified) mingw, links,
and even starts up (and stays up).

But it does **NOT** work, and there are enough errors left.

Some blurb from the top of my head:
* Do not try to use VC, it will not work. This is C99 code (yes, with
GNU extentions...), VC will not eat it. And it is no C++ code, so
do not try VC++. Get yourself mingw.
* The build system will not work on win, so you better cross compile
from Linux to Windows. `./configure --host=mingw32` is your friend
* The mingw header export some functions in their `stdlib.h` (as the orig.
windows header?), which collide with my functions, namely `itoa`, `ltoa`,
`ulltoa` etc. You could get rid of them by defining a slew of
`_NO_OLDNAMES`, `__NO_INLINE__`, etc., but this will remove other functions
we need. Simply `#if 0` them in the mingw headers. (Yes, modify the
system supplied header). This may be fixed in the meantime, but maybe not.
* Get zlib and Berkley DB for mingw, they are available out there.
(you can compile zlib with/into g2cd, but you still need BDB)

Even with this "magic" shortcut, some stuff is simply **broken**, because
the code is not written for Windows. The emulations put in place are also
very rough and broken.

Which brings us to the real win problems:
* G2CD expects a _working_ and thread local `errno`. Win likes to use
`(WSA)GetLastError`, have fun...
* Networking is esp. broken, because of this. And don't forget to
redefine all the `errno`-values from `WSAWHATEVER` to the normal ones.
* G2CD uses pthreads. I added a small wrapper, but it is:
  * prop. broken
  * uses `errno`, which will not work on win  
It starts and so on, but it's not water tight and does hang on shutdown.
* I dumped some more compat stuff in `my_pthread.h/.c`, which is prop.
also broken.
* There is some more functionality needed which needs to be proper
emulated (does `udpfromto` work? `socketpair` needs some love with `iocp`,
`sigaction` is just the sledgehammer aproach, `errno` (an always working
`strerror`), etc, etc).
* Did i mention the million of other bugs?

### 3.1.1 The major road block

All this aside, porting to Windows has a Major road block:

   * `poll`
   * `epoll`
  
`poll` is normaly only needed in "small" quantities, so it could be
emulated with `select` or `WaitForMutlibleObject/WSAAsyncSelect`. But
the problem is: if you then would try to emulate `epoll` with `poll`
(which is also not really working...), you would ran into the issue
that win only allows 64 sockets to be monitored by these APIs.  
Vista and higher (Server 2008, Win7, etc.) finally have a `WSAPoll`,
which is the real `poll` (what took you so long?), only with another
name, but prop. the same restriction.

Some emulation/wrapper for `poll` is now in place, which makes g2cd
tick, but for `epoll`, i don't know how to "fix" that. Emulation by `poll`
would only work on Vista and higher, and is not recommended. There
seems to be no other nice API, which only leaves us with completion
ports.

Completion ports are nice and dandy, only there is a little problem,
they mix three things (to achive async io):

* async transport to/from userland buffers with …
* … large scale event notification by …
* … managing a worker pool churning along all those events

Point 2. is whats needed, because i build point 3. myself. Using the NT
Kernel supplied goodness would be nice and an option, but point 1.
really ruins our party.

Async transport is a nice idea, and surely gives a nice boost for
most apps which churn out a lot of data, but G2CD is incompatible
from the ground up with this, because:

* our complete event control flow is wrong for this
* because we do not churn much data.

We have lots and lots of connections, but they are mostly idle.
Every few seconds a connection gets some data. The sum of connections
creates the traffic. For that reason, connections do not have a
fixed send/recveive buffer. When connections are ready, they temporaly
get a buffer, data is written/read, and then we try to send/consume all
the data in that buffer in one go, which works most of the time. This
way we can handle thousands of connection with a handful of buffer.
For async transport some core logic in G2CD has to be rewritten to
stick a fixed buffer on every connection.

Or completions ports persuaded not to do async io. But the API really
does not look like it can do this, everything has to be done with some
special funcs which take an `OVERLAPPED`-struct.  
Or some middle layer has to be added which translates the "syncronous"
io into async IO and extracts the events (some kind of double buffering
behind the scenes).

# 4. Coding-Style

OK, maybe I'm not a talented programmer and it's presumptuous to come
around with such a thing, I surely have to be happy if someone decides to
write just a simple line of code for this, but... at some time you come to
a point where you don't want to mess with ugly code anymore, at least for
your own project...

So, lets get started:  
First of all some words about my develop-environment. No fancy IDE, just
(G)Vim, some shells and a browser to view man-pages/search the web.  
I really *please* you to use *real* tabs. No soft-tabs, no other fancy
technique (they all fail), only real plain tabs. With this everyone could
adjust his favorite indentation-width, thats my believe. I use 3 as
indentation-width.  
And with this you got my position in the great flame-wars, you will find
me on the pro-Vi and pro-Tab side. I'm aware of the discussion about
"tabs-have-to-be-8", but the world is not Black-and-White :).  
Which editor you use, I don't really care, you could use a chalkboard,
as long as the results are digitally processable and it is capable of
producing real tabs.  
Another point where i won't discuss is the position of a "one-liner"
after a keyword (or places where you not open a block). It comes on a new
line (thats the important part), one indent (the not so important part).
To illustrate it:

```c
	while(foo)
		x++;`
```
or:
```c
	if(a <= b)
		bar();
	else
		y = baz();
```

And, that's the good news, this style doesn't have an occult source. No,
it has a real technical reason (one of the only style-things I know of,
which has a reason besides taste), which, hopefully, convinces you:  
Try to set a breakpoint in your debugger at the "one-liner" and *not*
on the keyword when they are both written on one line...  
From this reason you could also see the importance of the form of the
style. The newline is what technically matters, the indent is taste.

That are the main facts. You will see the other things when you look at
the Source (hopefully, i am getting messy...):
1. Let's get to the most complicated thing: curly braces
   I like the "new-block-braces" on a line of their own with the same
   indent as the keyword above (which incorporates very fine with
   folding ;). This gives the clear visual feeling of a "new block".
   No rule without exceptions:  
   * one liner do not have a brace (exception from the exception:
     sometimes a debug statement may vanish depending on compilation
     options, and an `if nothing else nothing` may set up the
     compiler and control flow...)
   * two and three liner get the opening brace on the same line,
     this is to prevent visual clutter on short statement, mostly
     error handling (like `logg("foo"); return false;`)
2. I like lower-case letters (do you want to do keyboard-gymnastics with
   shift?) filenames are a exception (you write them only sometimes, with
   tab completion), constants are all-upper-case.
3. I like under-scores, especially because i like to build namespaces  
   (Hint: this means avoid CaMeLCaSe and how to name functions)
4. I like something *between* u\*\*x-shortness (`su`, `ls`, `cd`) and talking
   names (`this_is_the_most_freaqently_used_variable`). Hmmm, hard to
   explain, sigh, you need a feeling for it.
5. Unfortunately, i didn't do it enough, but in the normal case I like
   comments, but don't comment the obvious (OK, again a hard thing, what's
   obvious and what's not...)
6. This one gets a little bit complicated: I'm *not* going to evangelize
   about the 80 chars per line (even if i am now fighting with printing),
   on most text terminals today we have got a little more. *But* your line
   has to end somewhere[1]. One *exception* are comments, they should be
   split at 64 chars. Why? Makes a nicer read and when the source ever
   gets wraped at 72 chars for any reason (ex.: email over f\*\*\*ed up web
   frontend), the compiler will still eat (word wise) split C constructs,
   but yell at spurious words fallen of a comments (and not in a comment
   any more). This can be prevented a little bit with obeying '15.' below.
   [1] Think of a modern worker with X and a display of 1024x768. (So,
   saying it fits on your dual-headed 1920x2480 workspace is not an
   argument...)
7. It's OK to be a little paranoid with parenthesis, as long as you don't
   build a lisp-like parenthesis-labyrinth. Try to reduce them but it's
   perfect to be cautious.
8. No parenthesis at the `return`-statement, this is not a function-call.
9. I know that the standard assures that a zero will evaluate to a perfect
   null-pointer, but: `NULL` for pointers, `0` for ints, `'\0'` for chars,
   to help the reader.
10. If you see a `sizeof(char)` -> sorry. I know it will always evaluate to
    one, it was surely done for the reader on *appropriate* places...
11. casts are a bad thing, avoid them (side note: This is C source,
    whoever casts `*alloc` or compiles it with a C++ compiler will be shoot ;)
12. I don't like mega-macro mumbo-jumbo on the one hand (ever saw average
    hashing code which unfolds it rounds in macros? Uarg. Or multi type
    auto code generation (C++ template like)? *self shoot*), but the preprocessor-
    magic was the thing I mostly missed in Java... so make a sensible
    decision.
13. Hmmm, while we're at the preprocessor, defines and ifdefs and such alike
    are also indented. The hash mark stays on column 1 (some compiler are
    picky about that), but the text is indented, with one *space* per level.
14. You can automatically generate a TODO list from the source. Besides of
    working on things on this list, it is even more important to put things
    on the list, especially the long term unsolved things, or you will
    forget about it. So feel free to add items to the list, but respect the
    syntax, or it will not show up at all or the right way:
    * item must start with the literale `// TODO: `
    * item must be at column 1
    * item must be a one liner, on a line of its own  
      -> if you have to say a lot, simply make it longer or split it up in a
         brief description and a explanation
    
    Use it, it really helps a lot, drop a quick TODO while things a still
    "hot" in your brain cells, to get things off your back, before you
    forget (the details) about it.
15. Even if C99 provides C++ like comments (`//`) and it is one of the
    mostly available extension to pre-C99 compiler, I _now_ like the
    classic `/**/` comments more, I'm still trying to clean up my source.
    They are more visual pleasing (as in "Hello! Here! Comment!") and
    prevent the problem from '6.' above. Exception is the TODO thingy.
    The reason i liked `//` comments more, comment out code chunks with
    `/**/`, can be achieved with `#if 0 - #endif` nicely.
    Multiline `/**/` comments have a star on each line at the front.
    Depending on length the start (`/*`) is on a line of it's own, so
    is the end (`*/`).
16. A little bit late, but... in C99 you can intermix code and variable
    defines, but: again we can get backward compatibility for no cost but
    writing it in the classic C90 way (at the beginning of a block). Since
    this cannot be fixed with a sed -> make it so. It also helps a little
    bit with the "how much variables are in here" question.
17. I didn't mention whitespace till now... Upsie...  
    That's also a hard one, so only some quick notes. Keywords and function
    calls do not get any whitespace before the (. Otherwise expressions are
    made lighter with whitepspace, i don't like super dense `a+c*6?g*i:g%9`.
    Whitespace is used generously to align the code (like putting = on the
    same column over several lines) to give the code a uniform look.
18. There are maybe places, where something differs from the normal layout.
    This is maybe an accident, i changed my mind how such a thing has to
    look and i didn't looked over it again, or it was my intention, since
    it was OK for this situation. -> ups, deadlock ;) (did i mention that
    i am getting messy?)

Non-format things:
* Security is a main concern. We are writing a server here. So:
```c
while(not_sure)
			not_sure = do_think(AGAIN, upsides, downsides, &problem);
```
* I don't want to support every system that ever was, but everything should
  be written with portability in mind, since this software should be able
  to run on big-iron and big-iron is surely different from your PC (Hey, be
  honest, you always dreamed of seeing your little soft running on a
  brutal-bignum-SMP-64bit-mainframe from IBM® (you know, such a thing with
  thousands of LEDs on its front) with tera-bytes of RAM and an exotic OS
  you never heard of ;). And mostly, portability will cost you nothing but
  using your Brain 1.0 and not hack together some code fast (also look at
  the points below).  
  (side note: such a portability hurdle today widely encountered is the
  AMD64 architecture, enough programs have issues about types "suddenly"
  being 64 Bit)
* I try to avoid BadHacks[tm], especially because of the portability-thing,
  so should you (see the fall-back-thing below). But to be honest, I surely
  make a lot of mistakes doing so, sometimes even deliberately ^^.  
  Until now I could think of at least three places. Thats why I also cry
  out loud for a portability-guru which can give some real world advise
  and not "don't do that"...
* Don't make any presumptions about data-types, especially about the basic
  types (most beloved: `int`). Become familiar with the type system according
  to the C standard. Use the appropriate data-type for the task, there are
  lots of predefined `_t` types which "guarantee" you some property (and
  hopefully portability and maybe security). (side note: C99 knows boolean,
  use it, i hate those hordes of functions returning `int` for *just*
  OK/ERROR...)
* Never read a multi-byte value at once from a byte-organized buffer. Read
  it byte-by-byte, don't assume any alignment. I think the only platform
  which "only" slightly penalizes unaligned reads by performance is x86, other
  platforms give you a SIGBUS (or emulate the read with a system trap
  -> extra slow and not portable (Linux® on $PROCESSOR may do it, FreeBSD®
  on $PROCESSOR maybe not)), core dump, game over.  
  Sidenote: PPC can do unaligned, but it is "complicated" (processors for
  embedded use vs. server, BE mode it's OK, LE mode...). ARM comes from
  an "Unaligned is Verboten!" world view to change to "Yeah! Unaligned!".
  Only problem is, while x86 introduced an alignment check flag with the 486,
  which is never used because of legacy, 25 years ago, they have that problem
  the other way round, to much legacy stuff depending on it/setting it, and not
  enough IT-millenia to let the cruft die out/let the good news be heard.  
  I made `{get|put}_unaligned` macros similar to those from the Linux® kernel,
  they may be of good use in some circumstances (and they offload the whole
  access foo to the compiler and generate nice asm on x86), but they need
  porting to other compilers than GCC.
* Furthermore we have to deal with endianess. Some protocol-values have a
  fixed endianess, some have the endianess of the sending host and we have
  our own (at write time of code unknown) on the receiving side. So
  byte-by-byte handling is ideal to also swap endianess while reading the
  value. We also have code for that `{get|put}_unaligned_{endian|le|be}`.
* If you want to use any tricks, you must provide a fall back anyway, so
  simply write it the first time around.

# Z. Legal
Trademark Legend:

UNIX is a registered trademark of The Open Group in the USA and other countries.  
IBM is a registered trademarks of International Business Machines Corporation in the United States, other countries, or both.  
Linux is a registered trademark of Linus Torvalds in the United States.  
FreeBSD is a registered trademark of The FreeBSD Foundation in the US and other countries.  
Other company, product, or service names may be trademarks or service marks of others.  

