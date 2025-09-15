# MYSH
Custom unix shell with network features

It supports job control, pipes, variable expansion, and built-in networking commands  
such as starting a server, connecting clients, and sending messages.

------------------------------------------------------------
Features
------------------------------------------------------------
- Core Shell Functionality:
  - Execute system commands (via `/bin` and `/usr/bin`)
  - Supports pipes (`|`) between commands
  - Foreground and background jobs (`&`)
  - Built-in `exit` and error handling

- Job Control:
  - Track running background processes
  - Notify when jobs complete
  - Clean job management with signal handling

- Variables:
  - Define and expand variables (`VAR=value`, `$VAR`)
  - Dynamic replacement during command parsing

- Networking Commands:
  - `start-server <port>`: launch a TCP server
  - `close-server`: terminate the running server
  - `start-client <port> <host>`: connect to a server and chat
  - `send <port> <host> <message>`: send a one-time message

- Signal Handling:
  - `Ctrl+C` shows a new prompt without quitting
  - `SIGTERM` and `SIGHUP` cleanly exit the shell and server

------------------------------------------------------------
Project Structure
------------------------------------------------------------
.
├── mysh.c        # Main shell loop, jobs, parsing, pipes, execution
├── commands.c    # Networking commands (server/client/message handling)
├── builtins.c    # Built-in commands (exit, cd, etc.)
├── variables.c   # Variable assignment, lookup, and expansion
├── io_helpers.*  # Input/output utility functions
└── README.md     # Documentation

------------------------------------------------------------
Requirements
------------------------------------------------------------
- GCC (C compiler)
- POSIX-compliant environment (Linux, macOS)
- Networking requires available TCP ports

Build with:
```bash
gcc -Wall -Wextra -o mysh mysh.c commands.c builtins.c variables.c io_helpers.c
```

------------------------------------------------------------
Usage
------------------------------------------------------------
Start the shell:
```bash
./mysh
```

Examples:
```bash
mysh$ VAR=hello
mysh$ echo $VAR
hello

mysh$ ls | grep .c
builtins.c
commands.c
mysh.c
variables.c

mysh$ start-server 5000
mysh$ start-client 5000 127.0.0.1
mysh$ send 5000 127.0.0.1 "Hello from MySH!"
```

- `Ctrl+C` → cancels current input and shows prompt again  
- `exit` → quits the shell  

------------------------------------------------------------
Author
------------------------------------------------------------
Developed by Eshaan Bhathal as a systems programming project in C.
