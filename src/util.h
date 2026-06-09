#ifndef WF_UTIL_H
#define WF_UTIL_H

#include <stddef.h>

int wf_join_args(int argc, char **argv, int start, char **out);
char *wf_strdup(const char *value);
int wf_streq(const char *left, const char *right);
int wf_time_now_iso(char *buffer, size_t size);
int wf_random_hex(char *buffer, size_t hex_chars);
void wf_sha256_hex(const char *input, char out[65]);
void wf_trim_newline(char *text);
int wf_read_password_twice(const char *prompt1, const char *prompt2, char **out);
int wf_read_password_once(const char *prompt, char **out);

#endif
