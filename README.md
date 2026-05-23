# MiniGit

MiniGit is a tiny educational content-addressed version control system written in C.

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

This version is primarily designed for Linux/POSIX environments.

---

# Features

MiniGit currently supports:

- initializing a local repository
- adding files to an index
- staging file deletions
- saving file contents as objects
- creating commits
- tracking deleted files inside commits
- Git-like staged and unstaged change detection
- viewing the commit log
- showing a file from a previous commit
- restoring a file from a previous commit
- checking out repository snapshots
- simple line-by-line diff inspection
- repository validation before command execution
- prevention of empty commits when no changes are staged

---

# Repository Structure

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

---

## HEAD

Stores the latest commit number.

Example:

```text
2
```

---

## index

Stores the staged files and their hashes.

The index can also contain staged deletions.

Example:

```text
main.c 123456789
README.md 987654321
DELETE old.txt
```

---

## objects/

Stores real file contents.

Example:

```text
.minigit/objects/249889038256978411.obj
```

---

## commits/

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

Deletion example:

```text
DELETE old.txt
```

---

# Build

Compile with GCC:

```bash
gcc -Wall -Wextra -pedantic -std=c11 minigit.c -o minigit
```

Install globally on Linux:

```bash
sudo cp minigit /usr/local/bin/
```

After that you can run:

```bash
minigit
```

from anywhere.

---

# Usage

## Initialize a repository

```bash
minigit init
```

## Add a file

```bash
minigit add file.txt
```

This command:

1. checks if the file exists
2. calculates its hash
3. stores the file content inside `.minigit/objects/`
4. updates the staging index

---

## Remove a tracked file

```bash
minigit rm file.txt
```

This command:

1. removes the file from the working tree
2. stages the deletion inside the index
3. allows the deletion to be committed later

---

## Check repository status

```bash
minigit status
```

Possible output:

```text
Changes staged for commit:
  modified: file.txt
  deleted: old.txt

Changes not staged for commit:
  modified: main.c
```

Or:

```text
Working tree clean.
```

---

## Create a commit

```bash
minigit commit "Initial commit"
```

This creates a new commit snapshot.

If the index already matches the latest commit:

```text
Nothing to commit.
```

---

## View commit log

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

---

## Show a file from a previous commit

```bash
minigit show 1 file.txt
```

---

## Restore a file from a previous commit

```bash
minigit restore 1 file.txt
```

---

## Checkout a repository snapshot

```bash
minigit checkout 1
```

This restores all tracked files from commit 1.

---

## Compare working tree with staged version

```bash
minigit diff file.txt
```

Example:

```text
Line 2 differs:
  INDEX : old line
  WORK  : new line
```

If the file is staged for deletion:

```text
File 'file.txt' is staged for deletion.
```

---

# Example Workflow

```bash
gcc -Wall -Wextra -pedantic -std=c11 minigit.c -o minigit

minigit init

echo "version 1" > file.txt

minigit add file.txt

minigit commit "First version"

echo "version 2" >> file.txt

minigit status

minigit diff file.txt

minigit add file.txt

minigit commit "Second version"

minigit log

minigit show 1 file.txt

minigit restore 1 file.txt
```

---

# Deletion Workflow Example

```bash
echo "temporary file" > old.txt

minigit add old.txt

minigit commit "Add old file"

minigit rm old.txt

minigit status

minigit commit "Remove old file"
```

---

# How It Works

MiniGit uses a simplified content-addressed storage model.

When a file is added:

```text
file content -> hash -> object file
```

For example:

```text
file.txt -> 249889038256978411 -> .minigit/objects/249889038256978411.obj
```

A commit stores:

- filenames
- hashes
- deletion markers

representing the repository snapshot.

MiniGit internally manages three states:

```text
Working Tree
Index (staging area)
HEAD (latest commit)
```

The status command compares these states to detect:

- staged changes
- unstaged changes
- staged deletions
- clean working trees

---

# Important Limitations

MiniGit is intentionally simple.

It does not currently support:

- branches
- merge
- recursive directory tracking
- file names containing spaces
- cryptographic hashing
- remote repositories
- push / pull
- partial staging
- binary diff visualization
- full repository cleanup during checkout

---

# Educational Notes

The hash function used in this project is based on djb2.

It is useful for learning, but it is not secure.

Real Git historically used SHA-1 and also supports SHA-256 in newer repositories.

---

# License

This project is released for educational purposes.

You can use, modify, and share it freely.
