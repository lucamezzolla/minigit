# MiniGit

MiniGit is a tiny educational version control system written in C.

It is **not** a replacement for Git.  
The goal of this project is to learn how a version control system can be built from basic concepts:

- command-line parsing
- file I/O
- directory management
- content hashing
- object storage
- commit metadata
- restoring older file versions

## Features

MiniGit currently supports:

- initializing a local repository
- adding files to an index
- saving file contents as objects
- creating commits
- showing the status of tracked files
- viewing the commit log
- showing a file from a previous commit
- restoring a file from a previous commit
- repository validation before command execution
- prevention of empty commits when no changes are staged

## Repository Structure

After running:

```bash
./minigit init
```

MiniGit creates:

```text
.minigit/
├── HEAD
├── index
├── objects/
└── commits/
```

### `HEAD`

Stores the latest commit number.

Example:

```text
2
```

### `index`

Stores the staged files and their hashes.

Example:

```text
file.txt 249889038256978411
```

### `objects/`

Stores real file contents.

Example:

```text
.minigit/objects/249889038256978411.obj
```

### `commits/`

Stores commit metadata.

Example:

```text
.minigit/commits/1.txt
```

Commit file example:

```text
commit: 1
message: Initial commit
files:
- file.txt 249889038256978411
```

## Build

Compile with GCC:

```bash
gcc -Wall -Wextra -pedantic -std=c11 minigit.c -o minigit
```

## Usage

### Initialize a repository

```bash
./minigit init
```

### Add a file

```bash
./minigit add file.txt
```

This command:

1. checks if the file exists
2. calculates its hash
3. stores the file content in `.minigit/objects/`
4. updates `.minigit/index`

### Check status

```bash
./minigit status
```

Possible output:

```text
file.txt -> clean
file.txt -> modified
file.txt -> deleted or unreadable
```

### Create a commit

```bash
./minigit commit "Initial commit"
```

This creates a new file inside `.minigit/commits/`.

If the index already matches the latest commit, MiniGit prints:

```text
Nothing to commit.
```

### Show the commit log

```bash
./minigit log
```

Example:

```text
commit: 2
message: Second version

commit: 1
message: Initial commit
```

### Show a file from a previous commit

```bash
./minigit show 1 file.txt
```

This prints the version of `file.txt` stored in commit `1`.

### Restore a file from a previous commit

```bash
./minigit restore 1 file.txt
```

This restores `file.txt` from commit `1`.

## Example Workflow

```bash
gcc -Wall -Wextra -pedantic -std=c11 minigit.c -o minigit

./minigit init

echo "version 1" > file.txt
./minigit add file.txt
./minigit commit "First version"

echo "version 2" > file.txt
./minigit add file.txt
./minigit commit "Second version"

./minigit log

./minigit show 1 file.txt

./minigit restore 1 file.txt
cat file.txt
```

Expected final output:

```text
version 1
```

## How It Works

MiniGit uses a simplified content-addressed storage model.

When a file is added:

```text
file content -> hash -> object file
```

For example:

```text
file.txt -> 249889038256978411 -> .minigit/objects/249889038256978411.obj
```

A commit does not store the full file directly.  
It stores the filename and the hash of the object representing that version.

This makes it possible to retrieve older versions later.

## Important Limitations

MiniGit is intentionally simple.

It does **not** currently support:

- branches
- merge
- diff
- recursive directory tracking
- file names containing spaces
- cryptographic hashing
- remote repositories
- push / pull
- deleting tracked files from commits
- full project checkout

## Educational Notes

The hash function used in this project is based on `djb2`.

It is useful for learning, but it is not secure.  
Real Git historically used SHA-1 and also supports SHA-256 in newer repositories.

## License

This project is released for educational purposes.  
You can use, modify, and share it freely.
