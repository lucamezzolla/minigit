/*
 * MiniGit - A tiny educational version control system written in C.
 *
 * This project is intentionally simple and is meant for learning:
 * - command-line parsing
 * - file I/O
 * - directory management
 * - content hashing
 * - object storage
 * - commit metadata
 * - restoring file versions
 *
 * It is NOT a replacement for Git.
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

#define MAX_PATH_LEN 512
#define MAX_LINE_LEN 1024
#define MAX_FILENAME_LEN 256

typedef enum {
    INDEX_ERROR = 0,
    INDEX_ADDED = 1,
    INDEX_UPDATED = 2
} IndexResult;

static int create_directory_if_missing(const char *path);
static int init_repository(void);

static unsigned long calculate_file_hash(const char *filename);
static int copy_file(const char *source_path, const char *destination_path);
static int save_file_object(const char *filename, unsigned long hash);

static IndexResult update_or_add_index_entry(const char *filename, unsigned long new_hash);
static int add_file(const char *filename);
static void show_status(void);

static int get_current_head(void);
static int get_next_commit_id(void);
static int update_head(int commit_id);
static int create_commit(const char *message);
static void show_log(void);

static int find_file_hash_in_commit(int commit_id, const char *filename, unsigned long *found_hash);
static void show_file_from_commit(int commit_id, const char *filename);
static int restore_file_from_commit(int commit_id, const char *filename);

static void print_usage(void);

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

/*
 * Educational hash function based on djb2.
 * This is simple and fast, but it is NOT cryptographically secure.
 */
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

static IndexResult update_or_add_index_entry(const char *filename, unsigned long new_hash) {
    FILE *index = fopen(INDEX_FILE, "r");
    FILE *temporary_index = fopen(".minigit/index.tmp", "w");

    if (temporary_index == NULL) {
        fprintf(stderr, "Error: failed to create temporary index file.\n");

        if (index != NULL) {
            fclose(index);
        }

        return INDEX_ERROR;
    }

    int found = 0;
    char indexed_filename[MAX_FILENAME_LEN];
    unsigned long indexed_hash;

    if (index != NULL) {
        while (fscanf(index, "%255s %lu", indexed_filename, &indexed_hash) == 2) {
            if (strcmp(indexed_filename, filename) == 0) {
                fprintf(temporary_index, "%s %lu\n", filename, new_hash);
                found = 1;
            } else {
                fprintf(temporary_index, "%s %lu\n", indexed_filename, indexed_hash);
            }
        }

        fclose(index);
    }

    if (!found) {
        fprintf(temporary_index, "%s %lu\n", filename, new_hash);
    }

    fclose(temporary_index);

    if (rename(".minigit/index.tmp", INDEX_FILE) != 0) {
        fprintf(stderr, "Error: failed to update index file.\n");
        return INDEX_ERROR;
    }

    return found ? INDEX_UPDATED : INDEX_ADDED;
}

static int add_file(const char *filename) {
    if (access(MINIGIT_DIR, F_OK) != 0) {
        fprintf(stderr, "Error: not a MiniGit repository. Run './minigit init' first.\n");
        return 0;
    }

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

static void show_status(void) {
    FILE *index = fopen(INDEX_FILE, "r");

    if (index == NULL) {
        printf("No files in the index.\n");
        return;
    }

    char filename[MAX_FILENAME_LEN];
    unsigned long saved_hash;

    while (fscanf(index, "%255s %lu", filename, &saved_hash) == 2) {
        unsigned long current_hash = calculate_file_hash(filename);

        if (current_hash == 0) {
            printf("%s -> deleted or unreadable\n", filename);
        } else if (current_hash == saved_hash) {
            printf("%s -> clean\n", filename);
        } else {
            printf("%s -> modified\n", filename);
        }
    }

    fclose(index);
}

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

static int create_commit(const char *message) {
    if (access(INDEX_FILE, F_OK) != 0) {
        fprintf(stderr, "Error: no index found. Add files before committing.\n");
        return 0;
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

    char filename[MAX_FILENAME_LEN];
    unsigned long hash;
    int file_count = 0;

    while (fscanf(index, "%255s %lu", filename, &hash) == 2) {
        fprintf(commit, "- %s %lu\n", filename, hash);
        file_count++;
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
        char indexed_filename[MAX_FILENAME_LEN];
        unsigned long hash;

        if (sscanf(line, "- %255s %lu", indexed_filename, &hash) == 2) {
            if (strcmp(indexed_filename, filename) == 0) {
                *found_hash = hash;
                fclose(commit);
                return 1;
            }
        }
    }

    fclose(commit);
    return 0;
}

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

static void print_usage(void) {
    printf("MiniGit - educational version control in C\n\n");
    printf("Usage:\n");
    printf("  ./minigit init\n");
    printf("  ./minigit add <file>\n");
    printf("  ./minigit status\n");
    printf("  ./minigit commit <message>\n");
    printf("  ./minigit log\n");
    printf("  ./minigit show <commit_id> <file>\n");
    printf("  ./minigit restore <commit_id> <file>\n\n");
    printf("Examples:\n");
    printf("  ./minigit init\n");
    printf("  ./minigit add main.c\n");
    printf("  ./minigit commit \"Initial commit\"\n");
    printf("  ./minigit show 1 main.c\n");
    printf("  ./minigit restore 1 main.c\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage();
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "init") == 0) {
        return init_repository() ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (strcmp(argv[1], "add") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: ./minigit add <file>\n");
            return EXIT_FAILURE;
        }

        return add_file(argv[2]) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (strcmp(argv[1], "status") == 0) {
        show_status();
        return EXIT_SUCCESS;
    }

    if (strcmp(argv[1], "commit") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: ./minigit commit <message>\n");
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
            fprintf(stderr, "Usage: ./minigit show <commit_id> <file>\n");
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
            fprintf(stderr, "Usage: ./minigit restore <commit_id> <file>\n");
            return EXIT_FAILURE;
        }

        int commit_id = atoi(argv[2]);

        if (commit_id <= 0) {
            fprintf(stderr, "Error: invalid commit id.\n");
            return EXIT_FAILURE;
        }

        return restore_file_from_commit(commit_id, argv[3]) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    fprintf(stderr, "Error: unknown command '%s'.\n\n", argv[1]);
    print_usage();

    return EXIT_FAILURE;
}
