#ifndef WF_AUTH_H
#define WF_AUTH_H

#include "domain.h"

enum wf_role {
    WF_ROLE_USER = 1,
    WF_ROLE_ASSISTANT = 2
};

struct wf_user {
    char username[128];
    enum wf_role role;
};

const char *wf_role_name(enum wf_role role);
int wf_role_parse(const char *value, enum wf_role *role);
int wf_auth_create(const struct wf_domain *domain, const char *username, const char *role_name);
int wf_auth_passwd(const struct wf_domain *domain, const char *username, const char *role_name);
int wf_auth_env_export(const struct wf_domain *domain, const char *username);
int wf_auth_env_clear(const struct wf_domain *domain);
int wf_auth_exec(const struct wf_domain *domain, const char *username, int argc, char **argv);
int wf_auth_current_user(const struct wf_domain *domain, struct wf_user *user);

#endif
