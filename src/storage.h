#ifndef WF_STORAGE_H
#define WF_STORAGE_H

#include <stddef.h>
#include <stdio.h>

int wf_mkdir_p(const char *path);
int wf_write_text_file_if_missing(const char *dir, const char *name, const char *value);
int wf_escape_field(const char *input, char **out);
int wf_unescape_field(const char *input, char **out);
int wf_split_tsv(char *line, char **fields, size_t max_fields);
int wf_replace_file(const char *path, FILE **file);
int wf_commit_replace(const char *path, FILE *file);

#endif
