#ifndef WF_ISSUE_H
#define WF_ISSUE_H

#include "auth.h"
#include "domain.h"

#include <stdio.h>

int wf_issue_command(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv);

/* Resolve a partial issue ID (exact or unique prefix of 2+ chars) to the full 16-char ID.
 * Returns 0 on success (resolved written to resolved[]), non-zero on error (message already printed). */
int wf_issue_resolve_id(const struct wf_domain *domain, const char *partial, char resolved[65]);

void wf_issue_usage(FILE *fp);

void wf_issue_print_completion_words(FILE *fp);

#endif
