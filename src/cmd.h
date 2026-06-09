#ifndef WF_CMD_H
#define WF_CMD_H

#include "domain.h"

struct wf_subcommand {
    const char *name;
    int (*fn)(const struct wf_domain *domain, int argc, char **argv);
};

enum wf_cmd_match_result {
    WF_CMD_MATCH_EXACT = 0,
    WF_CMD_MATCH_PREFIX,
    WF_CMD_MATCH_NONE,
    WF_CMD_MATCH_AMBIGUOUS,
};

enum wf_cmd_match_result wf_cmd_match(const struct wf_subcommand *table,
                                      const char *name,
                                      const struct wf_subcommand **out);

#endif
