# praetor

[Building](https://github.com/Zer0-One/praetor#Building)
| [Reporting Bugs](https://github.com/Zer0-One/praetor/wiki/Writing-Useful-Bug-Reports)
| [Downloads](https://whereisjenkins.wtf)

An IRC bot designed to be fast, robust, portable, and easily extensible. Unlike
other bots, praetor is written in C for efficiency, conforms closely to the
SUSv3 for maximum portability, and uses a Nagios-like IPC approach to allow you
to write plugins in any language you like.

Information regarding usage can be found in the manual page.

## Building

### Dependencies

Before building praetor, you will need to have installed
[`libjansson`](https://github.com/akheron/jansson) and
[`libressl`](https://github.com/libressl-portable/portable), as well as their
headers. If you don't have them, you may run `make deps` to build and install
them.

### Compiling

Most users will just want to run `make all`. For a description of each Makefile
target, see the table below.

Makefile targets         | Description
------------------------ | ---------------------------------------------------------
`make` or `make praetor` | Builds praetor
`make praetor-debug`     | Builds a debug version of praetor
`make deps`              | Downloads and builds dependencies
`make docs`              | Generates API documentation
`make test`              | Builds and runs unit tests
`make clean`             | Deletes generated binaries, documentation, and unit tests
`make all`               | `make clean` & `make docs` & `make praetor` & `make test`
`make all-debug`         | `make clean` & `make docs` & `make praetor-debug` & `make test`

## Installation

No packages yet.

## To-Do

The current to-do list:

- [ ] Implement praetor as a daemon, complete with init scripts for OpenRC, systemd, etc.
- [ ] Implement a unit testing framework
- [ ] Create a JSON config file format, parse it into usable structures
- [ ] Implement IRC client functionality
- [ ] Write a language-independent plugin system
- [ ] Create a basic set of fun plugins in different languages, to demonstrate
  user's ability to write plugins in any language they like
    - [ ] ddate, announces the ddate at midnight
    - [ ] markov, a markov-chain based chat application
    - [ ] sed, performs sed replacements on previous messages
    - [ ] acronym, deciphers unknown acronyms
- [ ] Implement functionality to automatically generate praetor's configuration
  based on an interactive IRC session with an admin user
