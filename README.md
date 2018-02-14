# praetor

[Building](https://github.com/Zer0-One/praetor#Building)
| [Reporting Bugs](https://github.com/Zer0-One/praetor/wiki/Writing-Useful-Bug-Reports)
| [Downloads](https://whereisjenkins.wtf)

[![Build Status](https://build.zer0-one.net/buildStatus/icon?job=praetor_release_linux_x86-64)](https://build.zer0-one.net/job/praetor_release_linux_x86-64/)

An IRC bot designed to be robust, portable, and easily extensible. Unlike other
bots, praetor is written in C for efficiency, conforms closely to the SUSv3 for
maximum portability, and uses a language-neutral IPC approach to allow you to
write plugins in any language you like.

Information regarding usage can be found in the manual page.

## Building

### Dependencies

Before building praetor, you will need to have installed
[`libjansson`](https://github.com/akheron/jansson) and
[`libressl`](https://github.com/libressl-portable/portable), as well as their
headers.

### Compiling

After ensuring that dependencies are taken care of, most users will just want
to run `make all`. For a description of each Makefile target, see the table
below.

Makefile targets         | Description
------------------------ | ---------------------------------------------------------
`make` or `make praetor` | Builds praetor
`make praetor-debug`     | Builds a debug version of praetor
`make docs`              | Generates API documentation
`make test`              | Builds and runs unit tests
`make analysis`          | Builds praetor and runs static analysis; dumps results in the 'analysis' folder
`make clean`             | Deletes generated binaries, documentation, and unit tests
`make all`               | `make clean` & `make docs` & `make praetor` & `make test`
`make all-debug`         | `make clean` & `make docs` & `make praetor-debug` & `make test`

## Installation

No packages yet; you'll have to build it yourself. Once praetor is stable, I'll
create packages for every major distro.

## License

    Copyright (c) 2015-2018 David Zero
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
    ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
    
    The views and conclusions contained in the software and documentation are those
    of the authors and should not be interpreted as representing official policies,
    either expressed or implied, of the FreeBSD Project.
