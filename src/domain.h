#ifndef WF_DOMAIN_H
#define WF_DOMAIN_H

#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

struct wf_domain {
    char cwd[PATH_MAX];
    char id[65];
    char root[PATH_MAX];
    char issues[PATH_MAX];
    char users[PATH_MAX];
    char sessions[PATH_MAX];
};

int wf_domain_resolve(struct wf_domain *domain);

#endif
