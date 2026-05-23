/*
 * MiniGit - A tiny educational content-addressed version control system written in C.
 *
 * This project is intentionally simple and is meant for learning:
 * - command-line parsing
 * - file I/O
 * - directory management
 * - content hashing
 * - object storage
 * - staging area
 * - commit metadata
 * - status inspection
 * - restoring older file versions
 * - checking out snapshots
 * - simple line-by-line diff
 *
 * It is NOT a replacement for Git.
 * This version is primarily designed for Linux/POSIX environments.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MINIGIT_DIR ".minigit"
#define OBJECTS_DIR ".minigit/objects"
#define COMMITS_DIR ".minigit/commits"
#define INDEX_FILE ".minigit/index"
#define HEAD_FILE ".minigit/HEAD"
#define TEMP_INDEX_FILE ".minigit/index.tmp"

#define MAX_PATH_LEN 512
#define MAX_LINE_LEN 1024
#define MAX_FILENAME_LEN 256

typedef enum {
    INDEX_ERROR = 0,
    INDEX_ADDED = 1,
    INDEX_UPDATED = 2
} IndexResult;

typedef enum {
    ENTRY_INVALID = 0,
    ENTRY_TRACK = 1,
    ENTRY_DELETE = 2
} EntryType;

/* CLI / repository lifecycle */
static void print_usage(void);
static int require_repository(void);
static int create_directory_if_missing(const char *path);
static int init_repository(void);

/* File hashing and object storage */
static unsigned long calculate_file_hash(const char *filename);
static int copy_file(const char *source_path, const char *destination_path);
static int save_file_object(const char *filename, unsigned long hash);

/* Entry parsing / formatting */
static EntryType parse_entry_line(const char *line, char *filename, unsigned long *hash);
static void write_track_entry(FILE *file, const char *filename, unsigned long hash);
static void write_delete_entry(FILE *file, const char *filename);

/* Index / staging area */
static IndexResult update_or_add_index_entry(const char *filename, unsigned long new_hash);
static int stage_delete_index_entry(const char *filename);
static int add_file(const char *filename);
static int rm_file(const char *filename);

/* Commit handling */
static int get_current_head(void);
static int get_next_commit_id(void);
static int update_head(int commit_id);
static int find_file_hash_in_commit(int commit_id, const char *filename, unsigned long *found_hash);
static int commit_contains_delete(int commit_id, const char *filename);
static int index_matches_commit(int commit_id);
static int create_commit(const char *message);
static void show_log(void);

/* Status */
static void show_working_tree_changes(int *changes_count);
static void show_staged_changes(int *staged_count);
static void show_status(void);

/* Snapshot / checkout helpers */
static int file_is_tracked_in_commit(int commit_id, const char *filename);
static int cleanup_tracked_files_not_in_commit(int commit_id);
static int rewrite_index_from_commit(int commit_id);

/* Read / restore / checkout old versions */
static void show_file_from_commit(int commit_id, const char *filename);
static int restore_file_from_commit(int commit_id, const char *filename);
static int checkout_commit(int commit_id);

/* Diff */
static void diff_file(const char *filename);

/* CLI / repository lifecycle */

static void print_usage(void) {
    printf("MiniGit - educational version control in C\n\n");
    printf("Usage:\n");
    printf("  minigit init\n");
    printf("  minigit add <file>\n");
    printf("  minigit rm <file>\n");
    printf("  minigit status\n");
    printf("  minigit commit <message>\n");
    printf("  minigit log\n");
    printf("  minigit show <commit_id> <file>\n");
    printf("  minigit restore <commit_id> <file>\n");
    printf("  minigit checkout <commit_id>\n");
    printf("  minigit diff <file>\n\n");

    printf("Examples:\n");
    printf("  minigit init\n");
    printf("  minigit add main.c\n");
    printf("  minigit commit \"Initial commit\"\n");
    printf("  minigit status\n");
    printf("  minigit show 1 main.c\n");
    printf("  minigit restore 1 main.c\n");
    printf("  minigit checkout 1\n");
    printf("  minigit diff main.c\n");
}

static int require_repository(void) {
    if (access(MINIGIT_DIR, F_OK) != 0 ||
        access(OBJECTS_DIR, F_OK) != 0 ||
        access(COMMITS_DIR, F_OK) != 0 ||
        access(HEAD_FILE, F_OK) != 0) {
        fprintf(stderr, "Error: not a MiniGit repository. Run 'minigit init' first.\n");
        return 0;
    }

    return 1;
}

static int create_directory_if_missing(const char *path) {
    if (mkdir(path, 0700) == 0) {
        return 1;
    }

    if (errno == EEXIST) {
        return 1;
    }

    return 0;
}

static int init_repository(void) {
    if (!create_directory_if_missing(MINIGIT_DIR) ||
        !create_directory_if_missing(OBJECTS_DIR) ||
        !create_directory_if_missing(COMMITS_DIR)) {
        fprintf(stderr, "Error: failed to initialize repository directories.\n");
        return 0;
    }

    if (access(HEAD_FILE, F_OK) == 0) {
        printf("MiniGit repository already initialized.\n");
        return 1;
    }

    FILE *head = fopen(HEAD_FILE, "w");

    if (head == NULL) {
        fprintf(stderr, "Error: failed to create HEAD file.\n");
        return 0;
    }

    fprintf(head, "0\n");
    fclose(head);

    printf("Initialized empty MiniGit repository in %s/\n", MINIGIT_DIR);
    return 1;
}

/* File hashing and object storage */

static unsigned long calculate_file_hash(const char *filename) {
    FILE *file = fopen(filename, "rb");

    if (file == NULL) {
        return 0;
    }

    unsigned long hash = 5381;
    int c;

    while ((c = fgetc(file)) != EOF) {
        hash = ((hash << 5) + hash) + (unsigned char)c;
    }

    fclose(file);
    return hash;
}

static int copy_file(const char *source_path, const char *destination_path) {
    FILE *source = fopen(source_path, "rb");

    if (source == NULL) {
        return 0;
    }

    FILE *destination = fopen(destination_path, "wb");

    if (destination == NULL) {
        fclose(source);
        return 0;
    }

    int c;

    while ((c = fgetc(source)) != EOF) {
        if (fputc(c, destination) == EOF) {
            fclose(source);
            fclose(destination);
            return 0;
        }
    }

    fclose(source);
    fclose(destination);
    return 1;
}

static int save_file_object(const char *filename, unsigned long hash) {
    char object_path[MAX_PATH_LEN];

    if (snprintf(object_path, sizeof(object_path), "%s/%lu.obj", OBJECTS_DIR, hash) >= (int)sizeof(object_path)) {
        fprintf(stderr, "Error: object path is too long.\n");
        return 0;
    }

    if (access(object_path, F_OK) == 0) {
        return 1;
    }

    return copy_file(filename, object_path);
}

/* Entry parsing / formatting */

static EntryType parse_entry_line(const char *line, char *filename, unsigned long *hash) {
    char legacy_filename[MAX_FILENAME_LEN];
    unsigned long legacy_hash;

    if (sscanf(line, "TRACK %255s %lu", filename, hash) == 2) {
        return ENTRY_TRACK;
    }

    if (sscanf(line, "DELETE %255s", filename) == 1) {
        return ENTRY_DELETE;
    }

    /*
     * Backward compatibility with older MiniGit files:
     * index line:  file.txt 12345
     * commit line: - file.txt 12345
     */
    if (sscanf(line, "- %255s %lu", legacy_filename, &legacy_hash) == 2) {
        strcpy(filename, legacy_filename);
        *hash = legacy_hash;
        return ENTRY_TRACK;
    }

    if (sscanf(line, "%255s %lu", legacy_filename, &legacy_hash) == 2) {
        strcpy(filename, legacy_filename);
        *hash = legacy_hash;
        return ENTRY_TRACK;
    }

    return ENTRY_INVALID;
}

static void write_track_entry(FILE *file, const char *filename, unsigned long hash) {
    fprintf(file, "TRACK %s %lu\n", filename, hash);
}

static void write_delete_entry(FILE *file, const char *filename) {
    fprintf(file, "DELETE %s\n", filename);
}

/* Index / staging area */

static IndexResult update_or_add_index_entry(const char *filename, unsigned long new_hash) {
    FILE *index = fopen(INDEX_FILE, "r");
    FILE *temporary_index = fopen(TEMP_INDEX_FILE, "w");

    if (temporary_index == NULL) {
        fprintf(stderr, "Error: failed to create temporary index file.\n");

        if (index != NULL) {
            fclose(index);
        }

        return INDEX_ERROR;
    }

    int found = 0;
    char line[MAX_LINE_LEN];

    if (index != NULL) {
        while (fgets(line, sizeof(line), index) != NULL) {
            char current_filename[MAX_FILENAME_LEN];
            unsigned long current_hash;
            EntryType type = parse_entry_line(line, current_filename, &current_hash);

            if (type == ENTRY_INVALID) {
                continue;
            }

            if (strcmp(current_filename, filename) == 0) {
                write_track_entry(temporary_index, filename, new_hash);
                found = 1;
            } else if (type == ENTRY_TRACK) {
                write_track_entry(temporary_index, current_filename, current_hash);
            } else if (type == ENTRY_DELETE) {
                write_delete_entry(temporary_index, current_filename);
            }
        }

        fclose(index);
    }

    if (!found) {
        write_track_entry(temporary_index, filename, new_hash);
    }

    fclose(temporary_index);

    if (rename(TEMP_INDEX_FILE, INDEX_FILE) != 0) {
        fprintf(stderr, "Error: failed to update index file.\n");
        return INDEX_ERROR;
    }

    return found ? INDEX_UPDATED : INDEX_ADDED;
}

static int stage_delete_index_entry(const char *filename) {
    FILE *index = fopen(INDEX_FILE, "r");
    FILE *temporary_index = fopen(TEMP_INDEX_FILE, "w");

    if (temporary_index == NULL) {
        if (index != NULL) {
            fclose(index);
        }

        fprintf(stderr, "Error: failed to create temporary index file.\n");
        return 0;
    }

    int tracked = 0;
    char line[MAX_LINE_LEN];

    if (index != NULL) {
        while (fgets(line, sizeof(line), index) != NULL) {
            char current_filename[MAX_FILENAME_LEN];
            unsigned long current_hash;
            EntryType type = parse_entry_line(line, current_filename, &current_hash);

            if (type == ENTRY_INVALID) {
                continue;
            }

            if (strcmp(current_filename, filename) == 0) {
                tracked = 1;
                write_delete_entry(temporary_index, filename);
            } else if (type == ENTRY_TRACK) {
                write_track_entry(temporary_index, current_filename, current_hash);
            } else if (type == ENTRY_DELETE) {
                write_delete_entry(temporary_index, current_filename);
            }
        }

        fclose(index);
    }

    fclose(temporary_index);

    if (!tracked) {
        remove(TEMP_INDEX_FILE);
        return 0;
    }

    if (rename(TEMP_INDEX_FILE, INDEX_FILE) != 0) {
        fprintf(stderr, "Error: failed to update index file.\n");
        return 0;
    }

    return 1;
}

static int add_file(const char *filename) {
    if (access(filename, F_OK) != 0) {
        fprintf(stderr, "Error: file '%s' does not exist.\n", filename);
        return 0;
    }

    unsigned long hash = calculate_file_hash(filename);

    if (hash == 0) {
        fprintf(stderr, "Error: failed to calculate file hash.\n");
        return 0;
    }

    if (!save_file_object(filename, hash)) {
        fprintf(stderr, "Error: failed to save file object.\n");
        return 0;
    }

    IndexResult result = update_or_add_index_entry(filename, hash);

    if (result == INDEX_ERROR) {
        return 0;
    }

    if (result == INDEX_ADDED) {
        printf("Added '%s' to the index.\n", filename);
    } else {
        printf("Updated '%s' in the index.\n", filename);
    }

    return 1;
}

static int rm_file(const char *filename) {
    if (!stage_delete_index_entry(filename)) {
        fprintf(stderr, "Error: file '%s' is not tracked.\n", filename);
        return 0;
    }

    if (access(filename, F_OK) == 0) {
        if (remove(filename) != 0) {
            fprintf(stderr, "Error: failed to remove '%s' from the working tree.\n", filename);
            return 0;
        }
    }

    printf("Staged deletion of '%s'.\n", filename);
    return 1;
}

/* Commit handling */

static int get_current_head(void) {
    FILE *head = fopen(HEAD_FILE, "r");

    if (head == NULL) {
        return -1;
    }

    int current_id = 0;

    if (fscanf(head, "%d", &current_id) != 1) {
        fclose(head);
        return -1;
    }

    fclose(head);
    return current_id;
}

static int get_next_commit_id(void) {
    int current_id = get_current_head();

    if (current_id < 0) {
        return -1;
    }

    return current_id + 1;
}

static int update_head(int commit_id) {
    FILE *head = fopen(HEAD_FILE, "w");

    if (head == NULL) {
        return 0;
    }

    fprintf(head, "%d\n", commit_id);
    fclose(head);
    return 1;
}

static int find_file_hash_in_commit(int commit_id, const char *filename, unsigned long *found_hash) {
    char commit_path[MAX_PATH_LEN];

    if (snprintf(commit_path, sizeof(commit_path), "%s/%d.txt", COMMITS_DIR, commit_id) >= (int)sizeof(commit_path)) {
        return 0;
    }

    FILE *commit = fopen(commit_path, "r");

    if (commit == NULL) {
        return 0;
    }

    char line[MAX_LINE_LEN];

    while (fgets(line, sizeof(line), commit) != NULL) {
        char current_filename[MAX_FILENAME_LEN];
        unsigned long current_hash;
        EntryType type = parse_entry_line(line, current_filename, &current_hash);

        if (type == ENTRY_TRACK && strcmp(current_filename, filename) == 0) {
            *found_hash = current_hash;
            fclose(commit);
            return 1;
        }
    }

    fclose(commit);
    return 0;
}

static int commit_contains_delete(int commit_id, const char *filename) {
    char commit_path[MAX_PATH_LEN];

    if (snprintf(commit_path, sizeof(commit_path), "%s/%d.txt", COMMITS_DIR, commit_id) >= (int)sizeof(commit_path)) {
        return 0;
    }

    FILE *commit = fopen(commit_path, "r");

    if (commit == NULL) {
        return 0;
    }

    char line[MAX_LINE_LEN];

    while (fgets(line, sizeof(line), commit) != NULL) {
        char current_filename[MAX_FILENAME_LEN];
        unsigned long hash;
        EntryType type = parse_entry_line(line, current_filename, &hash);

        if (type == ENTRY_DELETE && strcmp(current_filename, filename) == 0) {
            fclose(commit);
            return 1;
        }
    }

    fclose(commit);
    return 0;
}

static int index_matches_commit(int commit_id) {
    FILE *index = fopen(INDEX_FILE, "r");

    if (index == NULL) {
        return 0;
    }

    char line[MAX_LINE_LEN];
    int entries = 0;

    while (fgets(line, sizeof(line), index) != NULL) {
        char filename[MAX_FILENAME_LEN];
        unsigned long index_hash;
        EntryType type = parse_entry_line(line, filename, &index_hash);

        if (type == ENTRY_INVALID) {
            continue;
        }

        if (type == ENTRY_DELETE) {
            if (!commit_contains_delete(commit_id, filename)) {
                fclose(index);
                return 0;
            }

            entries++;
            continue;
        }

        if (type == ENTRY_TRACK) {
            unsigned long commit_hash;

            if (!find_file_hash_in_commit(commit_id, filename, &commit_hash)) {
                fclose(index);
                return 0;
            }

            if (index_hash != commit_hash) {
                fclose(index);
                return 0;
            }

            entries++;
        }
    }

    fclose(index);
    return entries > 0;
}

static int create_commit(const char *message) {
    if (access(INDEX_FILE, F_OK) != 0) {
        fprintf(stderr, "Error: no index found. Add files before committing.\n");
        return 0;
    }

    int current_head = get_current_head();

    if (current_head > 0 && index_matches_commit(current_head)) {
        printf("Nothing to commit.\n");
        return 1;
    }

    int commit_id = get_next_commit_id();

    if (commit_id == -1) {
        fprintf(stderr, "Error: failed to read HEAD.\n");
        return 0;
    }

    char commit_path[MAX_PATH_LEN];

    if (snprintf(commit_path, sizeof(commit_path), "%s/%d.txt", COMMITS_DIR, commit_id) >= (int)sizeof(commit_path)) {
        fprintf(stderr, "Error: commit path is too long.\n");
        return 0;
    }

    FILE *commit = fopen(commit_path, "w");

    if (commit == NULL) {
        fprintf(stderr, "Error: failed to create commit file.\n");
        return 0;
    }

    fprintf(commit, "commit: %d\n", commit_id);
    fprintf(commit, "message: %s\n", message);
    fprintf(commit, "files:\n");

    FILE *index = fopen(INDEX_FILE, "r");

    if (index == NULL) {
        fclose(commit);
        fprintf(stderr, "Error: failed to read index file.\n");
        return 0;
    }

    char line[MAX_LINE_LEN];
    int file_count = 0;

    while (fgets(line, sizeof(line), index) != NULL) {
        char filename[MAX_FILENAME_LEN];
        unsigned long hash;
        EntryType type = parse_entry_line(line, filename, &hash);

        if (type == ENTRY_DELETE) {
            write_delete_entry(commit, filename);
            file_count++;
        } else if (type == ENTRY_TRACK) {
            write_track_entry(commit, filename, hash);
            file_count++;
        }
    }

    fclose(index);
    fclose(commit);

    if (file_count == 0) {
        fprintf(stderr, "Error: nothing to commit.\n");
        return 0;
    }

    if (!update_head(commit_id)) {
        fprintf(stderr, "Error: failed to update HEAD.\n");
        return 0;
    }

    printf("Created commit %d.\n", commit_id);
    return 1;
}

static void show_log(void) {
    int head = get_current_head();

    if (head < 0) {
        fprintf(stderr, "Error: failed to read HEAD.\n");
        return;
    }

    if (head == 0) {
        printf("No commits yet.\n");
        return;
    }

    for (int commit_id = head; commit_id >= 1; commit_id--) {
        char commit_path[MAX_PATH_LEN];

        if (snprintf(commit_path, sizeof(commit_path), "%s/%d.txt", COMMITS_DIR, commit_id) >= (int)sizeof(commit_path)) {
            continue;
        }

        FILE *commit = fopen(commit_path, "r");

        if (commit == NULL) {
            continue;
        }

        char line[MAX_LINE_LEN];

        while (fgets(line, sizeof(line), commit) != NULL) {
            if (strncmp(line, "commit:", 7) == 0 || strncmp(line, "message:", 8) == 0) {
                printf("%s", line);
            }
        }

        printf("\n");
        fclose(commit);
    }
}

/* Status */

static void show_working_tree_changes(int *changes_count) {
    FILE *index = fopen(INDEX_FILE, "r");

    if (index == NULL) {
        printf("No files in the index.\n");
        return;
    }

    char line[MAX_LINE_LEN];

    while (fgets(line, sizeof(line), index) != NULL) {
        char filename[MAX_FILENAME_LEN];
        unsigned long index_hash;
        EntryType type = parse_entry_line(line, filename, &index_hash);

        if (type != ENTRY_TRACK) {
            continue;
        }

        unsigned long working_tree_hash = calculate_file_hash(filename);

        if (working_tree_hash == 0) {
            if (*changes_count == 0) {
                printf("Changes not staged for commit:\n");
            }

            printf("  deleted: %s\n", filename);
            (*changes_count)++;
        } else if (working_tree_hash != index_hash) {
            if (*changes_count == 0) {
                printf("Changes not staged for commit:\n");
            }

            printf("  modified: %s\n", filename);
            (*changes_count)++;
        }
    }

    fclose(index);
}

static void show_staged_changes(int *staged_count) {
    int current_head = get_current_head();

    FILE *index = fopen(INDEX_FILE, "r");

    if (index == NULL) {
        return;
    }

    char line[MAX_LINE_LEN];

    while (fgets(line, sizeof(line), index) != NULL) {
        char filename[MAX_FILENAME_LEN];
        unsigned long index_hash;
        EntryType type = parse_entry_line(line, filename, &index_hash);

        if (type == ENTRY_INVALID) {
            continue;
        }

        if (type == ENTRY_DELETE) {
            if (*staged_count == 0) {
                printf("Changes staged for commit:\n");
            }

            printf("  deleted: %s\n", filename);
            (*staged_count)++;
            continue;
        }

        if (type == ENTRY_TRACK) {
            unsigned long commit_hash;

            if (current_head <= 0 || !find_file_hash_in_commit(current_head, filename, &commit_hash)) {
                if (*staged_count == 0) {
                    printf("Changes staged for commit:\n");
                }

                printf("  new file: %s\n", filename);
                (*staged_count)++;
            } else if (index_hash != commit_hash) {
                if (*staged_count == 0) {
                    printf("Changes staged for commit:\n");
                }

                printf("  modified: %s\n", filename);
                (*staged_count)++;
            }
        }
    }

    fclose(index);
}

static void show_status(void) {
    int staged_count = 0;
    int changes_count = 0;

    show_staged_changes(&staged_count);
    show_working_tree_changes(&changes_count);

    if (staged_count == 0 && changes_count == 0) {
        printf("Working tree clean.\n");
    }
}

/* Snapshot / checkout helpers */

static int file_is_tracked_in_commit(int commit_id, const char *filename) {
    unsigned long hash;
    return find_file_hash_in_commit(commit_id, filename, &hash);
}

static int cleanup_tracked_files_not_in_commit(int commit_id) {
    FILE *index = fopen(INDEX_FILE, "r");

    if (index == NULL) {
        return 1;
    }

    char line[MAX_LINE_LEN];

    while (fgets(line, sizeof(line), index) != NULL) {
        char filename[MAX_FILENAME_LEN];
        unsigned long hash;
        EntryType type = parse_entry_line(line, filename, &hash);

        if (type == ENTRY_TRACK && !file_is_tracked_in_commit(commit_id, filename)) {
            if (access(filename, F_OK) == 0 && remove(filename) != 0) {
                fclose(index);
                fprintf(stderr, "Error: failed to remove obsolete file '%s'.\n", filename);
                return 0;
            }
        }
    }

    fclose(index);
    return 1;
}

static int rewrite_index_from_commit(int commit_id) {
    char commit_path[MAX_PATH_LEN];

    if (snprintf(commit_path, sizeof(commit_path), "%s/%d.txt", COMMITS_DIR, commit_id) >= (int)sizeof(commit_path)) {
        fprintf(stderr, "Error: commit path is too long.\n");
        return 0;
    }

    FILE *commit = fopen(commit_path, "r");
    FILE *index = fopen(INDEX_FILE, "w");

    if (commit == NULL || index == NULL) {
        if (commit != NULL) fclose(commit);
        if (index != NULL) fclose(index);
        fprintf(stderr, "Error: failed to rewrite index from commit.\n");
        return 0;
    }

    char line[MAX_LINE_LEN];

    while (fgets(line, sizeof(line), commit) != NULL) {
        char filename[MAX_FILENAME_LEN];
        unsigned long hash;
        EntryType type = parse_entry_line(line, filename, &hash);

        if (type == ENTRY_TRACK) {
            write_track_entry(index, filename, hash);
        }
    }

    fclose(commit);
    fclose(index);
    return 1;
}

/* Read / restore / checkout old versions */

static void show_file_from_commit(int commit_id, const char *filename) {
    unsigned long hash;

    if (!find_file_hash_in_commit(commit_id, filename, &hash)) {
        fprintf(stderr, "Error: file '%s' was not found in commit %d.\n", filename, commit_id);
        return;
    }

    char object_path[MAX_PATH_LEN];

    if (snprintf(object_path, sizeof(object_path), "%s/%lu.obj", OBJECTS_DIR, hash) >= (int)sizeof(object_path)) {
        fprintf(stderr, "Error: object path is too long.\n");
        return;
    }

    FILE *object = fopen(object_path, "rb");

    if (object == NULL) {
        fprintf(stderr, "Error: object '%s' was not found.\n", object_path);
        return;
    }

    int c;

    while ((c = fgetc(object)) != EOF) {
        putchar(c);
    }

    fclose(object);
}

static int restore_file_from_commit(int commit_id, const char *filename) {
    unsigned long hash;

    if (!find_file_hash_in_commit(commit_id, filename, &hash)) {
        fprintf(stderr, "Error: file '%s' was not found in commit %d.\n", filename, commit_id);
        return 0;
    }

    char object_path[MAX_PATH_LEN];

    if (snprintf(object_path, sizeof(object_path), "%s/%lu.obj", OBJECTS_DIR, hash) >= (int)sizeof(object_path)) {
        fprintf(stderr, "Error: object path is too long.\n");
        return 0;
    }

    if (!copy_file(object_path, filename)) {
        fprintf(stderr, "Error: failed to restore '%s'.\n", filename);
        return 0;
    }

    printf("Restored '%s' from commit %d.\n", filename, commit_id);
    return 1;
}

static int checkout_commit(int commit_id) {
    char commit_path[MAX_PATH_LEN];

    if (snprintf(commit_path, sizeof(commit_path), "%s/%d.txt", COMMITS_DIR, commit_id) >= (int)sizeof(commit_path)) {
        fprintf(stderr, "Error: commit path is too long.\n");
        return 0;
    }

    FILE *commit = fopen(commit_path, "r");

    if (commit == NULL) {
        fprintf(stderr, "Error: commit %d was not found.\n", commit_id);
        return 0;
    }

    if (!cleanup_tracked_files_not_in_commit(commit_id)) {
        fclose(commit);
        return 0;
    }

    char line[MAX_LINE_LEN];

    while (fgets(line, sizeof(line), commit) != NULL) {
        char filename[MAX_FILENAME_LEN];
        unsigned long hash;
        EntryType type = parse_entry_line(line, filename, &hash);

        if (type == ENTRY_DELETE) {
            if (access(filename, F_OK) == 0) {
                remove(filename);
            }

            continue;
        }

        if (type == ENTRY_TRACK) {
            char object_path[MAX_PATH_LEN];

            if (snprintf(object_path, sizeof(object_path), "%s/%lu.obj", OBJECTS_DIR, hash) >= (int)sizeof(object_path)) {
                fclose(commit);
                fprintf(stderr, "Error: object path is too long.\n");
                return 0;
            }

            if (!copy_file(object_path, filename)) {
                fclose(commit);
                fprintf(stderr, "Error: failed to checkout '%s'.\n", filename);
                return 0;
            }
        }
    }

    fclose(commit);

    if (!rewrite_index_from_commit(commit_id)) {
        return 0;
    }

    if (!update_head(commit_id)) {
        fprintf(stderr, "Error: failed to update HEAD.\n");
        return 0;
    }

    printf("Checked out commit %d.\n", commit_id);
    return 1;
}

/* Diff */

static void diff_file(const char *filename) {
    FILE *index = fopen(INDEX_FILE, "r");

    if (index == NULL) {
        fprintf(stderr, "Error: no index found.\n");
        return;
    }

    char line[MAX_LINE_LEN];
    unsigned long index_hash = 0;
    int found = 0;
    int staged_delete = 0;

    while (fgets(line, sizeof(line), index) != NULL) {
        char indexed_filename[MAX_FILENAME_LEN];
        unsigned long hash;
        EntryType type = parse_entry_line(line, indexed_filename, &hash);

        if (type == ENTRY_DELETE && strcmp(indexed_filename, filename) == 0) {
            staged_delete = 1;
            found = 1;
            break;
        }

        if (type == ENTRY_TRACK && strcmp(indexed_filename, filename) == 0) {
            index_hash = hash;
            found = 1;
            break;
        }
    }

    fclose(index);

    if (!found) {
        fprintf(stderr, "Error: file '%s' is not staged.\n", filename);
        return;
    }

    if (staged_delete) {
        printf("File '%s' is staged for deletion.\n", filename);
        return;
    }

    char object_path[MAX_PATH_LEN];

    if (snprintf(object_path, sizeof(object_path), "%s/%lu.obj", OBJECTS_DIR, index_hash) >= (int)sizeof(object_path)) {
        fprintf(stderr, "Error: object path is too long.\n");
        return;
    }

    FILE *snapshot = fopen(object_path, "r");
    FILE *working = fopen(filename, "r");

    if (snapshot == NULL) {
        fprintf(stderr, "Error: index object for '%s' was not found.\n", filename);
        return;
    }

    if (working == NULL) {
        fclose(snapshot);
        fprintf(stderr, "Error: cannot open working file '%s'.\n", filename);
        return;
    }

    char snapshot_line[MAX_LINE_LEN];
    char working_line[MAX_LINE_LEN];
    int line_number = 1;
    int differences = 0;

    while (1) {
        char *snapshot_result = fgets(snapshot_line, sizeof(snapshot_line), snapshot);
        char *working_result = fgets(working_line, sizeof(working_line), working);

        if (snapshot_result == NULL && working_result == NULL) {
            break;
        }

        if (snapshot_result == NULL || working_result == NULL ||
            strcmp(snapshot_line, working_line) != 0) {
            printf("Line %d differs:\n", line_number);
            printf("  INDEX : %s", snapshot_result ? snapshot_line : "(no line)\n");
            printf("  WORK  : %s", working_result ? working_line : "(no line)\n");
            printf("\n");
            differences++;
        }

        line_number++;
    }

    fclose(snapshot);
    fclose(working);

    if (differences == 0) {
        printf("No differences found.\n");
    }
}

/* Main command dispatcher */

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage();
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "init") != 0) {
        if (!require_repository()) {
            return EXIT_FAILURE;
        }
    }

    if (strcmp(argv[1], "init") == 0) {
        return init_repository() ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (strcmp(argv[1], "add") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: minigit add <file>\n");
            return EXIT_FAILURE;
        }

        return add_file(argv[2]) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (strcmp(argv[1], "rm") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: minigit rm <file>\n");
            return EXIT_FAILURE;
        }

        return rm_file(argv[2]) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (strcmp(argv[1], "status") == 0) {
        show_status();
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "commit") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: minigit commit <message>\n");
            return EXIT_FAILURE;
        }

        return create_commit(argv[2]) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (strcmp(argv[1], "log") == 0) {
        show_log();
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "show") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: minigit show <commit_id> <file>\n");
            return EXIT_FAILURE;
        }

        int commit_id = atoi(argv[2]);

        if (commit_id <= 0) {
            fprintf(stderr, "Error: invalid commit id.\n");
            return EXIT_FAILURE;
        }

        show_file_from_commit(commit_id, argv[3]);
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "restore") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Usage: minigit restore <commit_id> <file>\n");
            return EXIT_FAILURE;
        }

        int commit_id = atoi(argv[2]);

        if (commit_id <= 0) {
            fprintf(stderr, "Error: invalid commit id.\n");
            return EXIT_FAILURE;
        }

        return restore_file_from_commit(commit_id, argv[3]) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (strcmp(argv[1], "checkout") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: minigit checkout <commit_id>\n");
            return EXIT_FAILURE;
        }

        int commit_id = atoi(argv[2]);

        if (commit_id <= 0) {
            fprintf(stderr, "Error: invalid commit id.\n");
            return EXIT_FAILURE;
        }

        return checkout_commit(commit_id) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (strcmp(argv[1], "diff") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: minigit diff <file>\n");
            return EXIT_FAILURE;
        }

        diff_file(argv[2]);
        return EXIT_SUCCESS;
    }

    fprintf(stderr, "Error: unknown command '%s'.\n\n", argv[1]);
    print_usage();

    return EXIT_FAILURE;
}
