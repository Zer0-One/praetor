# praetor

[**Compiling**](https://github.com/Zer0-One/praetor/wiki/How-To-Build-Praetor)
**|** [**Reporting Bugs**](https://github.com/Zer0-One/praetor/wiki/Useful-Bug-Reports)
**|** [**Downloads**](https://whereisjenkins.wtf)

An IRC bot designed to be fast, robust, portable, and easily extensible. Unlike
other bots, praetor is written in C for efficiency, conforms closely to the
SUSv3 for maximum portability, and uses a Nagios-like IPC approach to allow you
to write plugins in any language you like.

Information regarding usage can be found in the manual page.

To-Do
-----

The current to-do list:

- [ ] Implement the IRC client functionality
- [ ] Write a plugin system
- [ ] Create a basic set of fun plugins in different languages, to demonstrate
  user's ability to write plugins in any language they like
    - [ ] ddate
    - [ ] markov
    - [ ] sed
    - [ ] acronym
- [ ] Build a utility to automatically generate plugin acls based on an
  interactive session with the user
