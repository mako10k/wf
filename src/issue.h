#ifndef WF_ISSUE_H
#define WF_ISSUE_H

#include "auth.h"
#include "domain.h"

int wf_issue_command(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv);

#endif
