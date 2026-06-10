#ifndef WF_CMD_H
#define WF_CMD_H

#include "domain.h"
#include "util.h"

struct wf_subcommand {
    const char *name;
    int (*fn)(const struct wf_domain *domain, int argc, char **argv);
};

enum wf_match_result wf_cmd_match(const struct wf_subcommand *table,
                                  const char *name,
                                  const struct wf_subcommand **out);

#endif
