#define _POSIX_C_SOURCE 200809L

#include "cmd.h"

#include "util.h"

#include <string.h>

enum wf_match_result wf_cmd_match(const struct wf_subcommand *table,
                                  const char *name,
                                  const struct wf_subcommand **out)
{
    const struct wf_subcommand *entry;
    const char *names[64];
    int i = 0;
    const char *matched = NULL;
    enum wf_match_result res;

    *out = NULL;

    if (table == NULL || name == NULL || name[0] == '\0') {
        return WF_MATCH_NONE;
    }

    for (entry = table; entry->name != NULL && i < 63; entry += 1) {
        names[i++] = entry->name;
    }
    names[i] = NULL;

    res = wf_match_prefix(names, name, &matched);
    if (res == WF_MATCH_EXACT || res == WF_MATCH_PREFIX) {
        for (entry = table; entry->name != NULL; entry += 1) {
            if (entry->name == matched) {
                *out = entry;
                return res;
            }
        }
        for (entry = table; entry->name != NULL; entry += 1) {
            if (wf_streq(entry->name, matched)) {
                *out = entry;
                return res;
            }
        }
    }
    return res;
}
