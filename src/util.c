#define _POSIX_C_SOURCE 200809L

#include "util.h"

#include <openssl/rand.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

char *wf_strdup(const char *value)
{
    size_t length;
    char *copy;

    if (value == NULL) {
        value = "";
    }
    length = strlen(value);
    copy = malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, value, length + 1);
    return copy;
}

int wf_streq(const char *left, const char *right)
{
    return left != NULL && right != NULL && strcmp(left, right) == 0;
}

int wf_join_args(int argc, char **argv, int start, char **out)
{
    size_t length = 0;
    char *joined;
    char *cursor;
    int index;

    *out = NULL;
    if (start >= argc) {
        fprintf(stderr, "missing text\n");
        return 1;
    }

    for (index = start; index < argc; index += 1) {
        length += strlen(argv[index]);
        if (index + 1 < argc) {
            length += 1;
        }
    }

    joined = malloc(length + 1);
    if (joined == NULL) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    cursor = joined;
    for (index = start; index < argc; index += 1) {
        size_t part_length = strlen(argv[index]);
        memcpy(cursor, argv[index], part_length);
        cursor += part_length;
        if (index + 1 < argc) {
            *cursor = ' ';
            cursor += 1;
        }
    }
    *cursor = '\0';
    *out = joined;
    return 0;
}

int wf_time_now_iso(char *buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm tm_value;

    if (gmtime_r(&now, &tm_value) == NULL) {
        return 1;
    }
    if (strftime(buffer, size, "%Y-%m-%dT%H:%M:%SZ", &tm_value) == 0) {
        return 1;
    }
    return 0;
}

int wf_random_hex(char *buffer, size_t hex_chars)
{
    static const char alphabet[] = "0123456789abcdef";
    size_t bytes_len = (hex_chars + 1) / 2;
    unsigned char *bytes;
    size_t index;

    bytes = malloc(bytes_len);
    if (bytes == NULL) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    if (RAND_bytes(bytes, (int)bytes_len) != 1) {
        free(bytes);
        fprintf(stderr, "failed to read random bytes\n");
        return 1;
    }
    for (index = 0; index < hex_chars; index += 1) {
        unsigned char byte = bytes[index / 2];
        buffer[index] = alphabet[(index % 2 == 0) ? (byte >> 4) : (byte & 0x0f)];
    }
    buffer[hex_chars] = '\0';
    free(bytes);
    return 0;
}

void wf_sha256_hex(const char *input, char out[65])
{
    static const char alphabet[] = "0123456789abcdef";
    unsigned char digest[SHA256_DIGEST_LENGTH];
    size_t index;

    SHA256((const unsigned char *)input, strlen(input), digest);
    for (index = 0; index < SHA256_DIGEST_LENGTH; index += 1) {
        out[index * 2] = alphabet[digest[index] >> 4];
        out[index * 2 + 1] = alphabet[digest[index] & 0x0f];
    }
    out[64] = '\0';
}

void wf_trim_newline(char *text)
{
    size_t length;

    if (text == NULL) {
        return;
    }
    length = strlen(text);
    while (length > 0 && (text[length - 1] == '\n' || text[length - 1] == '\r')) {
        text[length - 1] = '\0';
        length -= 1;
    }
}

int wf_read_password_once(const char *prompt, char **out)
{
    FILE *tty;
    struct termios old_term;
    struct termios new_term;
    char buffer[1024];

    *out = NULL;
    tty = fopen("/dev/tty", "r+");
    if (tty == NULL) {
        perror("/dev/tty");
        return 1;
    }
    fputs(prompt, tty);
    fflush(tty);
    if (tcgetattr(fileno(tty), &old_term) != 0) {
        perror("tcgetattr");
        fclose(tty);
        return 1;
    }
    new_term = old_term;
    new_term.c_lflag &= (tcflag_t)~ECHO;
    if (tcsetattr(fileno(tty), TCSAFLUSH, &new_term) != 0) {
        perror("tcsetattr");
        fclose(tty);
        return 1;
    }
    if (fgets(buffer, sizeof(buffer), tty) == NULL) {
        tcsetattr(fileno(tty), TCSAFLUSH, &old_term);
        fputc('\n', tty);
        fclose(tty);
        fprintf(stderr, "failed to read password\n");
        return 1;
    }
    tcsetattr(fileno(tty), TCSAFLUSH, &old_term);
    fputc('\n', tty);
    fclose(tty);
    wf_trim_newline(buffer);
    if (buffer[0] == '\0') {
        fprintf(stderr, "password cannot be empty\n");
        return 1;
    }
    *out = wf_strdup(buffer);
    if (*out == NULL) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    return 0;
}

int wf_read_password_twice(const char *prompt1, const char *prompt2, char **out)
{
    char *first;
    char *second;

    *out = NULL;
    if (wf_read_password_once(prompt1, &first) != 0) {
        return 1;
    }
    if (wf_read_password_once(prompt2, &second) != 0) {
        free(first);
        return 1;
    }
    if (strcmp(first, second) != 0) {
        free(first);
        free(second);
        fprintf(stderr, "passwords do not match\n");
        return 1;
    }
    free(second);
    *out = first;
    return 0;
}
