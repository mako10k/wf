#define _POSIX_C_SOURCE 200809L

#include "storage.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int wf_mkdir_p(const char *path)
{
    char buffer[4096];
    char *cursor;

    if (strlen(path) >= sizeof(buffer)) {
        fprintf(stderr, "path too long\n");
        return 1;
    }
    strcpy(buffer, path);

    for (cursor = buffer + 1; *cursor != '\0'; cursor += 1) {
        if (*cursor == '/') {
            *cursor = '\0';
            if (mkdir(buffer, 0700) != 0 && errno != EEXIST) {
                perror(buffer);
                return 1;
            }
            *cursor = '/';
        }
    }
    if (mkdir(buffer, 0700) != 0 && errno != EEXIST) {
        perror(buffer);
        return 1;
    }
    return 0;
}

int wf_write_text_file_if_missing(const char *dir, const char *name, const char *value)
{
    char path[4096];
    FILE *file;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (access(path, F_OK) == 0) {
        return 0;
    }
    file = fopen(path, "w");
    if (file == NULL) {
        perror(path);
        return 1;
    }
    fprintf(file, "%s\n", value);
    if (fclose(file) != 0) {
        perror(path);
        return 1;
    }
    return 0;
}

int wf_escape_field(const char *input, char **out)
{
    size_t length = 0;
    const char *cursor;
    char *escaped;
    char *writer;

    if (input == NULL) {
        input = "";
    }
    for (cursor = input; *cursor != '\0'; cursor += 1) {
        length += (*cursor == '\\' || *cursor == '\t' || *cursor == '\n' || *cursor == '\r') ? 2 : 1;
    }
    escaped = malloc(length + 1);
    if (escaped == NULL) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    writer = escaped;
    for (cursor = input; *cursor != '\0'; cursor += 1) {
        if (*cursor == '\\') {
            *writer++ = '\\';
            *writer++ = '\\';
        } else if (*cursor == '\t') {
            *writer++ = '\\';
            *writer++ = 't';
        } else if (*cursor == '\n') {
            *writer++ = '\\';
            *writer++ = 'n';
        } else if (*cursor == '\r') {
            *writer++ = '\\';
            *writer++ = 'r';
        } else {
            *writer++ = *cursor;
        }
    }
    *writer = '\0';
    *out = escaped;
    return 0;
}

int wf_unescape_field(const char *input, char **out)
{
    char *decoded;
    char *writer;
    const char *cursor;

    decoded = malloc(strlen(input) + 1);
    if (decoded == NULL) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    writer = decoded;
    for (cursor = input; *cursor != '\0'; cursor += 1) {
        if (*cursor == '\\' && cursor[1] != '\0') {
            cursor += 1;
            if (*cursor == 't') {
                *writer++ = '\t';
            } else if (*cursor == 'n') {
                *writer++ = '\n';
            } else if (*cursor == 'r') {
                *writer++ = '\r';
            } else {
                *writer++ = *cursor;
            }
        } else {
            *writer++ = *cursor;
        }
    }
    *writer = '\0';
    *out = decoded;
    return 0;
}

int wf_split_tsv(char *line, char **fields, size_t max_fields)
{
    size_t count = 0;
    char *cursor = line;

    while (count < max_fields) {
        fields[count++] = cursor;
        cursor = strchr(cursor, '\t');
        if (cursor == NULL) {
            break;
        }
        *cursor = '\0';
        cursor += 1;
    }
    return (int)count;
}

int wf_replace_file(const char *path, FILE **file)
{
    char temp_path[4096];

    snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);
    *file = fopen(temp_path, "w");
    if (*file == NULL) {
        perror(temp_path);
        return 1;
    }
    return 0;
}

int wf_commit_replace(const char *path, FILE *file)
{
    char temp_path[4096];

    snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);
    if (fclose(file) != 0) {
        perror(temp_path);
        return 1;
    }
    if (rename(temp_path, path) != 0) {
        perror(path);
        return 1;
    }
    return 0;
}
