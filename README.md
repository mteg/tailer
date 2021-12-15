# Synopsis
`tailer` is a small commandline utility that sorts out escape sequences (= terminal control codes) in your
telnet/script/ssh session dumps.

# The problem
This *almost* works:

`$ telnet 172.30.32.254 | tee telnet.log`

Unless you are a robot, if you record your terminal session with `script` or make a log of your ssh/telnet with `tee`,
you will inherently have to deal with escape sequences in your log. Cursor left, cursor right, clean screen etc.
These are harmless if you re-play the log to your terminal via `cat`. But if you would rather grep the log or
process it in other automated ways, you will run into all sorts of `ESC[1Dohw cableESC[8DESC[1Dhw`. The
content you are grepping for may even not be there! For example, if you interactively type `shw` and
then use arrow keys to correct `shw` into `show`, your session log will record `shw`, `ESC[D`
(which is cursor left), `o`. There is no way for `grep` to figure out `shwESC[Do` is actually a `show`.

# The solution
`tailer` is here to help. 

You can either pass your log through `tailer` to clean it up:
```
$ tailer < telnet.log 
```
or dispose with `tee` and run `tailer` interactively, making it record and clean up your input as you go:
```
$ tailer -f telnet.log -- telnet 172.30.32.254
```

Unicode is supported, as long as your locale (environment variables etc.) is set up properly. 

# Options
```
$ tailer -h
Usage: ./tailer [options] [-f <output file>] [-- <command> [arguments]]
   -a       Append instead of creating a new file
   -W <col> Override terminal width
   -H <row> Override terminal height
   -i       Ignore resizes
   -p <pfx> Prefix all lines with the specified string
   -t       Add timestamp to every line
```

# How it works?
`tailer` maintains its own, virtual terminal screen image (only in memory, not displaying it anywhere). Only `TERM=ansi` is supported. 

Every input byte is forwarded into that virtual terminal, updating its state accordingly. The linefeed character
(0x0a aka '\n') triggers the actual tailing action. Upon detecting a linefeed, the terminal contents is dumped
line by line, starting from the first line up to the cursor position. Trailing spaces are removed from every dumped line.
After everything is dumped, the virtual terminal is cleared and reset (cursor back at 0,0).

# Known issues
- `tailer` is line oriented, it will not work well with pseudo-GUI applications like `mc` 
- the virtual terminal `tailer` runs should have the same dimensions as the real thing you are using 
  - `tailer` uses `ioctl(TIOCGWINSZ)` to determine your terminal size, which works only when standard input is a terminal
  - if you are piping a file through it, `tailer` has no means of determining the original terminal dimensions; for best results it is recommended to provide them with `-W` and `-H` options   
- There is an inherent line length limit `L` corresponding to the total terminal capacity (for example, in case of a 80x25 terminal, `L` equals some 2000 characters, give or take 1 or 2)
  - Longer lines will get through the interactive mode, however only the last `L` characters will be logged 

# License
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

Neither the name of the copyright holder nor the names of contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS, COPYRIGHT HOLDERS, OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Acknowledgements
`tailer` uses awesome `libtmt` by Rob King. See https://github.com/deadpixi/libtmt 
