#define _POSIX_C_SOURCE 200809L

#include "cmd.h"

#include "util.h"

#include <stdio.h>
#include <string.h>

enum wf_cmd_match_result wf_cmd_match(const struct wf_subcommand *table,
                                      const char *name,
                                      const struct wf_subcommand **out)
{
    const struct wf_subcommand *entry;
    const struct wf_subcommand *exact = NULL;
    const struct wf_subcommand *prefix_match = NULL;
    int prefix_count = 0;
    size_t name_len;

    *out = NULL;

    if (name == NULL || name[0] == '\0' || table == NULL) {
        return WF_CMD_MATCH_NONE;
    }

    name_len = strlen(name);

    /* First pass: look for exact match (highest priority) */
    for (entry = table; entry->name != NULL; entry += 1) {
        if (wf_streq(entry->name, name)) {
            exact = entry;
            break;
        }
    }
    if (exact != NULL) {
        *out = exact;
        return WF_CMD_MATCH_EXACT;
    }

    /* Second pass: collect unique prefix matches */
    for (entry = table; entry->name != NULL; entry += 1) {
        if (strncmp(entry->name, name, name_len) == 0) {
            prefix_count += 1;
            prefix_match = entry;
        }
    }

    if (prefix_count == 1) {
        *out = prefix_match;
        return WF_CMD_MATCH_PREFIX;
    }
    if (prefix_count > 1) {
        return WF_CMD_MATCH_AMBIGUOUS;
    }
    return WF_CMD_MATCH_NONE;
}
