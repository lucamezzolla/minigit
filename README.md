# MiniGit

MiniGit is a tiny educational content-addressed version control system written in C, primarily designed for Linux environments.

The project was developed and tested on Linux using GCC and POSIX APIs.

It is **not** a replacement for Git.  
The goal of this project is to learn how a version control system can be built from basic concepts:

- command-line parsing
- file I/O
- directory management
- content hashing
- object storage
- staging area
- commit metadata
- status inspection
- restoring older file versions
- checking out snapshots
- simple line-by-line diff

## Features

MiniGit currently supports:

- initializing a local repository
- adding files to an index/staging area
- removing tracked files
- saving file contents as objects
- creating commits
- Git-like staged and unstaged change detection
- viewing the commit log
- showing a file from a previous commit
- restoring a single file from a previous commit
- checking out a full commit snapshot
- simple line-by-line diff between the index and the working tree
- repository validation before command execution
- prevention of empty commits when no changes are staged

## Repository Structure

After running:

```bash
minigit init
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

Optional system-wide installation on Linux:

```bash
sudo cp minigit /usr/local/bin/
```

After that, you can run MiniGit from any directory:

```bash
minigit status
```

## Usage

### Initialize a repository

```bash
minigit init
```

### Add a file

```bash
minigit add file.txt
```

This command:

1. checks if the file exists
2. calculates its hash
3. stores the file content in `.minigit/objects/`
4. updates `.minigit/index`

MiniGit uses the index as a staging area.

When you run:

```bash
minigit add file.txt
```

the current version of the file is stored in the index and becomes staged for the next commit.

### Remove a tracked file

```bash
minigit rm file.txt
```

This command removes the file from the working tree and removes it from the index.

### Check status

```bash
minigit status
```

Possible output:

```text
Changes staged for commit:
  modified: file.txt

Changes not staged for commit:
  modified: file.txt

Working tree clean.
```

The status command compares:

```text
Working Tree
Index
HEAD
```

This allows MiniGit to detect staged changes, unstaged changes, deleted files, and clean working trees.

### Create a commit

```bash
minigit commit "Initial commit"
```

This creates a new file inside `.minigit/commits/`.

If the index already matches the latest commit, MiniGit prints:

```text
Nothing to commit.
```

### Show the commit log

```bash
minigit log
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
minigit show 1 file.txt
```

This prints the version of `file.txt` stored in commit `1`.

### Restore a single file from a previous commit

```bash
minigit restore 1 file.txt
```

This restores only `file.txt` from commit `1`.

### Checkout a full commit snapshot

```bash
minigit checkout 1
```

This restores all files tracked by commit `1` and updates `HEAD`.

This is a simplified version of the snapshot checkout concept used by real version control systems.

### Show a simple diff

```bash
minigit diff file.txt
```

This compares the staged version of `file.txt` in the index with the current working tree version.

The current diff implementation is intentionally simple and compares files line by line.

## Example Workflow

```bash
gcc -Wall -Wextra -pedantic -std=c11 minigit.c -o minigit

./minigit init

echo "version 1" > file.txt
./minigit add file.txt
./minigit commit "First version"

echo "version 2" > file.txt
./minigit status
./minigit diff file.txt

./minigit add file.txt
./minigit commit "Second version"

./minigit log

./minigit show 1 file.txt

./minigit restore 1 file.txt
cat file.txt

./minigit checkout 2
cat file.txt
```

Expected output after restoring commit `1`:

```text
version 1
```

Expected output after checking out commit `2`:

```text
version 2
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

MiniGit internally manages three states:

```text
Working Tree
Index (staging area)
HEAD (latest commit)
```

The `status` command compares these states to detect:

- staged changes
- unstaged changes
- deleted files
- clean working trees

The `checkout` command restores a full tracked snapshot from a selected commit.

The `restore` command restores a single file from a selected commit.

The `diff` command compares the indexed version of a file with the current working tree version.

## Important Limitations

MiniGit is intentionally simple.

It does **not** currently support:

- branches
- merge
- recursive directory tracking
- file names containing spaces
- cryptographic hashing
- remote repositories
- push / pull
- deleting files from older snapshots automatically during checkout
- partial staging
- advanced diff algorithms
- commit parent chains or DAG history

## Educational Notes

The hash function used in this project is based on `djb2`.

It is useful for learning, but it is not secure.  
Real Git historically used SHA-1 and also supports SHA-256 in newer repositories.

MiniGit is designed to expose the internal ideas behind version control systems in a simple and readable way.

## License

This project is released for educational purposes.  
You can use, modify, and share it freely.
