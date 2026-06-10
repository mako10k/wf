#ifndef WF_DOMAIN_H
#define WF_DOMAIN_H

#include <limits.h>
#include <stddef.h>
#include <stdio.h>

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

/* Domain management UX (inspired by secdat) */
int wf_domain_base(char *path, size_t size);
int wf_domain_list(FILE *fp);
int wf_domain_current(const struct wf_domain *domain, FILE *fp);
int wf_domain_create(struct wf_domain *domain);  /* ensure + write path */
int wf_domain_delete(const char *id_or_short);   /* by short prefix or full id; uses current if NULL/empty */
int wf_domain_resolve_by_id(struct wf_domain *domain, const char *id_or_short);

#endif
