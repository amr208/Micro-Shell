# A Minimal Unix-like Shell

## Table of Contents
1. [Overview](#overview)
2. [Features](#features)
3. [Built-in Commands](#built-in-commands)
4. [Redirection](#redirection)
5. [Variable Handling](#variable-handling)
6. [Compilation](#compilation)
7. [Usage](#usage)
8. [Limitations](#limitations)
9. [Code Structure](#code-structure)
10. [Future Improvements](#future-improvements)

## Overview

FemtoShell is a lightweight Unix-like shell implementation written in C. It provides basic shell functionality including command execution, I/O redirection, and environment variable handling. The shell was developed as an educational project to demonstrate core operating system concepts.

## Features

- Command line interface with prompt showing current directory
- Execution of external programs
- Built-in commands for common operations
- I/O redirection (>, 2>, <)
- Environment variable support
- Simple error handling

## Built-in Commands

### `echo [args...]`
Prints arguments to stdout with variable expansion:
```bash
echo Hello $USER
```

### `cd [directory]`
Changes working directory:
```bash
cd /path/to/directory
cd $HOME
```

### `pwd`
Prints current working directory

### `export [name=value]`
Sets environment variables:
```bash
export PATH=/usr/bin
export MYVAR=value
```

### `exit`
Terminates the shell

## Redirection

FemtoShell supports basic I/O redirection:

- Output redirection (`>`):
  ```bash
  ls -l > output.txt
  ```

- Error redirection (`2>`):
  ```bash
  gcc code.c 2> errors.log
  ```

- Input redirection (`<`):
  ```bash
  sort < input.txt
  ```

## Variable Handling

- Local variables (set with `name=value` syntax)
- Environment variables (set with `export`)
- Variable expansion in commands (`$VARNAME`)

## Compilation

Compile with gcc:
```bash
gcc -o femtoshell shell.c
```

## Usage

Run the executable:
```bash
./femtoshell
```

Example session:
```bash
My Own Shell: /home/user >> ls -l > files.txt
My Own Shell: /home/user >> cat files.txt
[contents of directory listing]
My Own Shell: /home/user >> exit
```

## Limitations

- Limited error handling
- No support for pipes (`|`)
- No command history
- Limited argument parsing capabilities
- No tab completion

## Code Structure

Key components:
- `main()`: Command loop and input processing
- Built-in command handlers
- Redirection handling
- Variable management (local and environment)
- External command execution

## Future Improvements

1. Add pipe support
2. Implement command history
3. Add tab completion
4. Improve error handling
5. Add scripting capabilities
6. Implement job control
7. Add signal handling

## License

This project is open-source and available for educational use.
