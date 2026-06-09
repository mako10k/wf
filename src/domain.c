#define _POSIX_C_SOURCE 200809L

#include "domain.h"

#include "storage.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *wf_xdg_data_home(void)
{
    const char *xdg = getenv("XDG_DATA_HOME");
    const char *home;
    static char fallback[PATH_MAX];

    if (xdg != NULL && xdg[0] != '\0') {
        return xdg;
    }

    home = getenv("HOME");
    if (home == NULL || home[0] == '\0') {
        return ".";
    }
    snprintf(fallback, sizeof(fallback), "%s/.local/share", home);
    return fallback;
}

static int wf_format_path(char *buffer, size_t size, const char *format, const char *left, const char *right)
{
    int written = snprintf(buffer, size, format, left, right);

    if (written < 0 || (size_t)written >= size) {
        fprintf(stderr, "path too long\n");
        return 1;
    }
    return 0;
}

int wf_domain_resolve(struct wf_domain *domain)
{
    char hash_input[PATH_MAX + 16];

    memset(domain, 0, sizeof(*domain));
    if (getcwd(domain->cwd, sizeof(domain->cwd)) == NULL) {
        perror("getcwd");
        return 1;
    }

    snprintf(hash_input, sizeof(hash_input), "wf:%s", domain->cwd);
    wf_sha256_hex(hash_input, domain->id);
    if (wf_format_path(domain->root, sizeof(domain->root), "%s/wf/domains/%s", wf_xdg_data_home(), domain->id) != 0 ||
        wf_format_path(domain->issues, sizeof(domain->issues), "%s/%s", domain->root, "issues") != 0 ||
        wf_format_path(domain->users, sizeof(domain->users), "%s/%s", domain->root, "users.tsv") != 0 ||
        wf_format_path(domain->sessions, sizeof(domain->sessions), "%s/%s", domain->root, "sessions.tsv") != 0) {
        return 1;
    }

    if (wf_mkdir_p(domain->issues) != 0) {
        return 1;
    }
    if (wf_write_text_file_if_missing(domain->root, "path", domain->cwd) != 0) {
        return 1;
    }
    return 0;
}
