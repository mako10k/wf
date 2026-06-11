#define _POSIX_C_SOURCE 200809L

#include "domain.h"

#include "i18n.h"
#include "storage.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

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
        fprintf(stderr, _("path too long\n"));
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

int wf_domain_base(char *path, size_t size)
{
    const char *xdg_base = wf_xdg_data_home();
    return wf_format_path(path, size, "%s/wf/domains", xdg_base, "");
}

static int wf_domain_read_path_file(const char *domain_root, char *out_path, size_t size)
{
    char path[PATH_MAX];
    FILE *f;
    int n;

    if (wf_format_path(path, sizeof(path), "%s/path", domain_root, "") != 0) {
        return 1;
    }
    f = fopen(path, "r");
    if (f == NULL) {
        /* not registered yet or missing */
        out_path[0] = '\0';
        return 0;
    }
    if (fgets(out_path, (int)size, f) == NULL) {
        fclose(f);
        out_path[0] = '\0';
        return 0;
    }
    fclose(f);
    n = (int)strlen(out_path);
    if (n > 0 && out_path[n-1] == '\n') {
        out_path[n-1] = '\0';
    }
    return 0;
}

int wf_domain_list(FILE *fp)
{
    char base[PATH_MAX];
    DIR *d;
    struct dirent *ent;
    char short_id[9];
    char recorded[PATH_MAX];

    if (wf_domain_base(base, sizeof(base)) != 0) {
        return 1;
    }
    d = opendir(base);
    if (d == NULL) {
        /* no domains yet */
        return 0;
    }
    while ((ent = readdir(d)) != NULL) {
        size_t len = strlen(ent->d_name);
        if (len != 64) {
            continue;
        }
        /* basic hex check */
        int i;
        for (i = 0; i < 64; i++) {
            char c = ent->d_name[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
                goto next;
            }
        }
        snprintf(short_id, sizeof(short_id), "%.8s", ent->d_name);
        {
            char root[PATH_MAX];
            if (wf_format_path(root, sizeof(root), "%s/%s", base, ent->d_name) != 0) {
                continue;
            }
            if (wf_domain_read_path_file(root, recorded, sizeof(recorded)) != 0) {
                recorded[0] = '\0';
            }
        }
        if (recorded[0]) {
            fprintf(fp, "%s  %s\n", short_id, recorded);
        } else {
            fprintf(fp, _("%s  (no path recorded)\n"), short_id);
        }
next:
        ;
    }
    closedir(d);
    return 0;
}

int wf_domain_current(const struct wf_domain *domain, FILE *fp)
{
    char short_id[9];
    snprintf(short_id, sizeof(short_id), "%.8s", domain->id);
    fprintf(fp, "domain: %s\n", short_id);
    fprintf(fp, "id:     %s\n", domain->id);
    fprintf(fp, "path:   %s\n", domain->cwd);
    fprintf(fp, "root:   %s\n", domain->root);
    return 0;
}

int wf_domain_create(struct wf_domain *domain)
{
    /* resolve already does mkdir + write path; we just call it */
    return wf_domain_resolve(domain);
}

static int wf_is_hex64(const char *s)
{
    size_t i;
    if (strlen(s) != 64) return 0;
    for (i = 0; i < 64; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
    }
    return 1;
}

int wf_domain_resolve_by_id(struct wf_domain *domain, const char *id_or_short)
{
    char base[PATH_MAX];
    char full[65];
    char root[PATH_MAX];
    char recorded[PATH_MAX];

    memset(domain, 0, sizeof(*domain));

    if (wf_domain_base(base, sizeof(base)) != 0) {
        return 1;
    }

    if (wf_is_hex64(id_or_short)) {
        snprintf(full, sizeof(full), "%s", id_or_short);
    } else {
        /* prefix match against existing */
        DIR *d = opendir(base);
        struct dirent *ent;
        char idbuf[4096][65];
        const char *cands[4096+1];
        int n = 0;
        const char *matched = NULL;
        enum wf_match_result res;

        if (d == NULL) return 1;
        while ((ent = readdir(d)) != NULL && n < 4096) {
            if (strlen(ent->d_name) == 64 && wf_is_hex64(ent->d_name)) {
                snprintf(idbuf[n], sizeof(idbuf[n]), "%s", ent->d_name);
                cands[n] = idbuf[n];
                n++;
            }
        }
        closedir(d);
        cands[n] = NULL;

        res = wf_match_prefix(cands, id_or_short, &matched);
        if (res == WF_MATCH_EXACT || res == WF_MATCH_PREFIX) {
            snprintf(full, sizeof(full), "%s", matched);
        } else {
            fprintf(stderr, _("domain not found or ambiguous: %s\n"), id_or_short);
            return 1;
        }
    }

    if (wf_format_path(root, sizeof(root), "%s/%s", base, full) != 0) {
        return 1;
    }
    if (wf_domain_read_path_file(root, recorded, sizeof(recorded)) != 0) {
        recorded[0] = '\0';
    }

    /* fill the struct similar to normal resolve */
    snprintf(domain->id, sizeof(domain->id), "%s", full);
    snprintf(domain->root, sizeof(domain->root), "%s", root);
    snprintf(domain->cwd, sizeof(domain->cwd), "%s", recorded[0] ? recorded : _("(unknown)"));
    if (wf_format_path(domain->issues, sizeof(domain->issues), "%s/%s", root, "issues") != 0 ||
        wf_format_path(domain->users, sizeof(domain->users), "%s/%s", root, "users.tsv") != 0 ||
        wf_format_path(domain->sessions, sizeof(domain->sessions), "%s/%s", root, "sessions.tsv") != 0) {
        return 1;
    }
    /* ensure dirs exist if operating on it */
    if (wf_mkdir_p(domain->issues) != 0) {
        return 1;
    }
    return 0;
}

int wf_domain_delete(const char *id_or_short)
{
    char base[PATH_MAX];
    char full_id[65];
    char root[PATH_MAX];
    /* For simplicity in delete, if no arg use current? But to delete we need id.
       If id_or_short is NULL or empty, we can error or require explicit. */
    if (id_or_short == NULL || id_or_short[0] == '\0') {
        fprintf(stderr, _("usage: wf domain delete <id|short-id>\n"));
        fprintf(stderr, _("See 'wf help semantics' for how domain IDs and short prefixes work.\n"));
        return 1;
    }

    if (wf_domain_base(base, sizeof(base)) != 0) {
        return 1;
    }

    if (wf_is_hex64(id_or_short)) {
        snprintf(full_id, sizeof(full_id), "%s", id_or_short);
    } else {
        /* collect existing domain ids and use prefix match */
        DIR *d;
        struct dirent *ent;
        char idbuf[4096][65];
        const char *cands[4096 + 1];
        int n = 0;
        const char *matched = NULL;
        enum wf_match_result res;

        d = opendir(base);
        if (d == NULL) {
            fprintf(stderr, _("no domains to delete\n"));
            return 1;
        }
        while ((ent = readdir(d)) != NULL && n < 4096) {
            if (strlen(ent->d_name) == 64 && wf_is_hex64(ent->d_name)) {
                snprintf(idbuf[n], sizeof(idbuf[n]), "%s", ent->d_name);
                cands[n] = idbuf[n];
                n++;
            }
        }
        closedir(d);
        cands[n] = NULL;

        res = wf_match_prefix(cands, id_or_short, &matched);
        if (res == WF_MATCH_EXACT || res == WF_MATCH_PREFIX) {
            snprintf(full_id, sizeof(full_id), "%s", matched);
        } else if (res == WF_MATCH_AMBIGUOUS) {
            fprintf(stderr, _("ambiguous domain id: %s. See 'wf help semantics'.\n"), id_or_short);
            return 1;
        } else {
            fprintf(stderr, _("domain not found: %s. See 'wf help concepts' (domain section).\n"), id_or_short);
            return 1;
        }
    }

    if (wf_format_path(root, sizeof(root), "%s/%s", base, full_id) != 0) {
        return 1;
    }

    /* safety: refuse to delete if it doesn't look like a wf domain root */
    char pathfile[PATH_MAX];
    if (wf_format_path(pathfile, sizeof(pathfile), "%s/path", root, "") != 0) {
        return 1;
    }
    if (access(pathfile, F_OK) != 0) {
        fprintf(stderr, _("refusing to delete: %s does not look like a wf domain. See 'wf help concepts'.\n"), root);
        return 1;
    }

    /* remove the entire domain tree */
    /* simple recursive remove using rm -rf for lightness; production would walk */
    {
        char cmd[PATH_MAX + 32];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", root);
        if (system(cmd) != 0) {
            perror("rm -rf domain");
            return 1;
        }
    }
    fprintf(stderr, _("deleted domain %s\n"), full_id);
    return 0;
}
