#define _POSIX_C_SOURCE 200809L

#include "issue.h"

#include "storage.h"
#include "util.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct wf_issue {
    char id[65];
    char *content;
    char creator[128];
    char status[16];
    char approver[128];
    char *approval_comment;
    char created_at[32];
    char updated_at[32];
};

static void wf_issue_free(struct wf_issue *issue)
{
    free(issue->content);
    free(issue->approval_comment);
    memset(issue, 0, sizeof(*issue));
}

static int wf_valid_status(const char *status)
{
    return wf_streq(status, "open") || wf_streq(status, "approved") || wf_streq(status, "rejected") ||
           wf_streq(status, "closed") || wf_streq(status, "hold");
}

static int wf_issue_path(const struct wf_domain *domain, const char *id, char *path, size_t size)
{
    int written;

    if (strstr(id, "/") != NULL || strlen(id) != 16) {
        fprintf(stderr, "invalid issue id\n");
        return 1;
    }
    written = snprintf(path, size, "%s/%s.tsv", domain->issues, id);
    if (written < 0 || (size_t)written >= size) {
        fprintf(stderr, "path too long\n");
        return 1;
    }
    return 0;
}

static int wf_issue_comments_path(const struct wf_domain *domain, const char *id, char *path, size_t size)
{
    int written;

    if (strstr(id, "/") != NULL || strlen(id) != 16) {
        fprintf(stderr, "invalid issue id\n");
        return 1;
    }
    written = snprintf(path, size, "%s/%s.comments.tsv", domain->issues, id);
    if (written < 0 || (size_t)written >= size) {
        fprintf(stderr, "path too long\n");
        return 1;
    }
    return 0;
}

int wf_issue_resolve_id(const struct wf_domain *domain, const char *partial, char resolved[65])
{
    DIR *dir;
    struct dirent *entry;
    char idbuf[4096][17];
    const char *names[4096 + 1];
    int n = 0;
    size_t plen;
    const char *matched = NULL;
    enum wf_match_result res;

    if (partial == NULL || partial[0] == '\0') {
        fprintf(stderr, "missing issue id\n");
        return 1;
    }
    plen = strlen(partial);
    if (strstr(partial, "/") != NULL) {
        fprintf(stderr, "invalid issue id\n");
        return 1;
    }
    if (plen < 2) {
        fprintf(stderr, "issue id too short (minimum 2 characters). See 'wf help semantics'.\n");
        return 1;
    }

    dir = opendir(domain->issues);
    if (dir == NULL) {
        perror(domain->issues);
        return 1;
    }
    while ((entry = readdir(dir)) != NULL) {
        size_t length = strlen(entry->d_name);
        if (length <= 4 || strcmp(entry->d_name + length - 4, ".tsv") != 0 ||
            strstr(entry->d_name, ".comments.tsv") != NULL) {
            continue;
        }
        int idlen = (int)(length - 4);
        if (idlen != 16) {
            continue;
        }
        if (n >= 4096) {
            continue;
        }
        memcpy(idbuf[n], entry->d_name, 16);
        idbuf[n][16] = '\0';
        names[n] = idbuf[n];
        n += 1;
    }
    closedir(dir);
    names[n] = NULL;

    res = wf_match_prefix(names, partial, &matched);
    if (res == WF_MATCH_EXACT || res == WF_MATCH_PREFIX) {
        snprintf(resolved, 65, "%s", matched);
        return 0;
    }
    if (res == WF_MATCH_AMBIGUOUS) {
        fprintf(stderr, "ambiguous issue id: %s. See 'wf help semantics'.\n", partial);
        return 1;
    }
    fprintf(stderr, "issue not found: %s. See 'wf help semantics'.\n", partial);
    return 1;
}

static int wf_issue_load(const struct wf_domain *domain, const char *id, struct wf_issue *issue)
{
    char path[PATH_MAX];
    FILE *file;
    char line[8192];
    char *fields[8];
    char *content = NULL;
    char *approval = NULL;

    memset(issue, 0, sizeof(*issue));
    if (wf_issue_path(domain, id, path, sizeof(path)) != 0) {
        return 1;
    }
    file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "issue not found: %s\n", id);
        return 1;
    }
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        fprintf(stderr, "corrupt issue: %s\n", id);
        return 1;
    }
    fclose(file);
    wf_trim_newline(line);
    if (wf_split_tsv(line, fields, 8) != 8) {
        fprintf(stderr, "corrupt issue: %s\n", id);
        return 1;
    }
    if (wf_unescape_field(fields[1], &content) != 0 || wf_unescape_field(fields[5], &approval) != 0) {
        free(content);
        free(approval);
        return 1;
    }
    snprintf(issue->id, sizeof(issue->id), "%s", fields[0]);
    issue->content = content;
    snprintf(issue->creator, sizeof(issue->creator), "%s", fields[2]);
    snprintf(issue->status, sizeof(issue->status), "%s", fields[3]);
    snprintf(issue->approver, sizeof(issue->approver), "%s", fields[4]);
    issue->approval_comment = approval;
    snprintf(issue->created_at, sizeof(issue->created_at), "%s", fields[6]);
    snprintf(issue->updated_at, sizeof(issue->updated_at), "%s", fields[7]);
    if (!wf_valid_status(issue->status)) {
        fprintf(stderr, "corrupt issue status: %s\n", issue->status);
        wf_issue_free(issue);
        return 1;
    }
    return 0;
}

static int wf_issue_save(const struct wf_domain *domain, const struct wf_issue *issue)
{
    char path[PATH_MAX];
    FILE *file;
    char *content = NULL;
    char *approval = NULL;
    int rc = 0;

    if (wf_issue_path(domain, issue->id, path, sizeof(path)) != 0) {
        return 1;
    }
    if (wf_escape_field(issue->content, &content) != 0 || wf_escape_field(issue->approval_comment, &approval) != 0) {
        free(content);
        free(approval);
        return 1;
    }
    if (wf_replace_file(path, &file) != 0) {
        free(content);
        free(approval);
        return 1;
    }
    fprintf(file, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
            issue->id,
            content,
            issue->creator,
            issue->status,
            issue->approver,
            approval,
            issue->created_at,
            issue->updated_at);
    if (wf_commit_replace(path, file) != 0) {
        rc = 1;
    }
    free(content);
    free(approval);
    return rc;
}

static int wf_issue_add_comment(const struct wf_domain *domain, const struct wf_user *user, const char *id, const char *comment)
{
    char path[PATH_MAX];
    char timestamp[32];
    char *escaped = NULL;
    FILE *file;

    if (wf_issue_comments_path(domain, id, path, sizeof(path)) != 0) {
        return 1;
    }
    if (wf_time_now_iso(timestamp, sizeof(timestamp)) != 0 || wf_escape_field(comment, &escaped) != 0) {
        free(escaped);
        return 1;
    }
    file = fopen(path, "a");
    if (file == NULL) {
        perror(path);
        free(escaped);
        return 1;
    }
    fprintf(file, "%s\t%s\t%s\t%s\n", timestamp, user->username, wf_role_name(user->role), escaped);
    free(escaped);
    if (fclose(file) != 0) {
        perror(path);
        return 1;
    }
    return 0;
}

static int wf_issue_create(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    struct wf_issue issue;
    char *content = NULL;
    char id[17];

    if (user->role != WF_ROLE_ASSISTANT) {
        fprintf(stderr, "permission denied: only Assistant can create issues\n");
        return 1;
    }
    if (wf_join_args(argc, argv, 2, &content) != 0) {
        return 1;
    }
    if (wf_random_hex(id, 16) != 0) {
        free(content);
        return 1;
    }
    memset(&issue, 0, sizeof(issue));
    snprintf(issue.id, sizeof(issue.id), "%s", id);
    issue.content = content;
    snprintf(issue.creator, sizeof(issue.creator), "%s", user->username);
    snprintf(issue.status, sizeof(issue.status), "open");
    issue.approver[0] = '\0';
    issue.approval_comment = wf_strdup("");
    if (issue.approval_comment == NULL) {
        wf_issue_free(&issue);
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    if (wf_time_now_iso(issue.created_at, sizeof(issue.created_at)) != 0) {
        wf_issue_free(&issue);
        return 1;
    }
    snprintf(issue.updated_at, sizeof(issue.updated_at), "%s", issue.created_at);
    if (wf_issue_save(domain, &issue) != 0) {
        wf_issue_free(&issue);
        return 1;
    }
    printf("%s\n", issue.id);
    wf_issue_free(&issue);
    return 0;
}

static int wf_issue_list(const struct wf_domain *domain)
{
    DIR *dir;
    struct dirent *entry;

    dir = opendir(domain->issues);
    if (dir == NULL) {
        perror(domain->issues);
        return 1;
    }
    while ((entry = readdir(dir)) != NULL) {
        struct wf_issue issue;
        size_t length = strlen(entry->d_name);
        char id[65];

        if (length <= 4 || strcmp(entry->d_name + length - 4, ".tsv") != 0 ||
            strstr(entry->d_name, ".comments.tsv") != NULL) {
            continue;
        }
        snprintf(id, sizeof(id), "%.*s", (int)(length - 4), entry->d_name);
        if (wf_issue_load(domain, id, &issue) == 0) {
            printf("%s\t%s\t%s\t%s\t%.60s\n", issue.id, issue.status, issue.creator, issue.updated_at, issue.content);
            wf_issue_free(&issue);
        }
    }
    closedir(dir);
    return 0;
}

static int wf_issue_show(const struct wf_domain *domain, const char *id)
{
    struct wf_issue issue;
    char full_id[65];

    if (wf_issue_resolve_id(domain, id, full_id) != 0) {
        return 1;
    }
    if (wf_issue_load(domain, full_id, &issue) != 0) {
        return 1;
    }
    printf("id: %s\n", issue.id);
    printf("status: %s\n", issue.status);
    printf("creator: %s\n", issue.creator);
    printf("approver: %s\n", issue.approver);
    printf("approval_comment: %s\n", issue.approval_comment);
    printf("created_at: %s\n", issue.created_at);
    printf("updated_at: %s\n", issue.updated_at);
    printf("content:\n%s\n", issue.content);
    wf_issue_free(&issue);
    return 0;
}

static int wf_issue_update(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    struct wf_issue issue;
    char *content = NULL;
    char full_id[65];

    if (argc < 4) {
        fprintf(stderr, "usage: wf issue update ISSUE_ID CONTENTS\n");
        return 1;
    }
    if (wf_issue_resolve_id(domain, argv[2], full_id) != 0) {
        return 1;
    }
    if (wf_issue_load(domain, full_id, &issue) != 0) {
        return 1;
    }
    if (user->role != WF_ROLE_ASSISTANT || strcmp(issue.creator, user->username) != 0) {
        wf_issue_free(&issue);
        fprintf(stderr, "permission denied: Assistant can update own issues only\n");
        return 1;
    }
    if (wf_join_args(argc, argv, 3, &content) != 0) {
        wf_issue_free(&issue);
        return 1;
    }
    free(issue.content);
    issue.content = content;
    wf_time_now_iso(issue.updated_at, sizeof(issue.updated_at));
    if (wf_issue_save(domain, &issue) != 0) {
        wf_issue_free(&issue);
        return 1;
    }
    wf_issue_free(&issue);
    return 0;
}

static int wf_issue_delete(const struct wf_domain *domain, const struct wf_user *user, const char *id)
{
    struct wf_issue issue;
    char path[PATH_MAX];
    char comments_path[PATH_MAX];
    char full_id[65];

    if (wf_issue_resolve_id(domain, id, full_id) != 0) {
        return 1;
    }
    if (wf_issue_load(domain, full_id, &issue) != 0) {
        return 1;
    }
    if (user->role != WF_ROLE_ASSISTANT || strcmp(issue.creator, user->username) != 0) {
        wf_issue_free(&issue);
        fprintf(stderr, "permission denied: Assistant can delete own issues only\n");
        return 1;
    }
    wf_issue_free(&issue);
    if (wf_issue_path(domain, full_id, path, sizeof(path)) != 0 ||
        wf_issue_comments_path(domain, full_id, comments_path, sizeof(comments_path)) != 0) {
        return 1;
    }
    if (unlink(path) != 0) {
        perror(path);
        return 1;
    }
    unlink(comments_path);
    return 0;
}

static int wf_issue_set_status(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv, const char *status)
{
    struct wf_issue issue;
    char *comment = NULL;
    char full_id[65];

    if (argc < 4) {
        fprintf(stderr, "usage: wf issue %s ISSUE_ID COMMENT\n", argv[1]);
        return 1;
    }
    if (user->role != WF_ROLE_USER) {
        fprintf(stderr, "permission denied: only User can change approval status\n");
        return 1;
    }
    if (wf_issue_resolve_id(domain, argv[2], full_id) != 0) {
        return 1;
    }
    if (wf_issue_load(domain, full_id, &issue) != 0) {
        return 1;
    }
    if (wf_join_args(argc, argv, 3, &comment) != 0) {
        wf_issue_free(&issue);
        return 1;
    }
    snprintf(issue.status, sizeof(issue.status), "%s", status);
    snprintf(issue.approver, sizeof(issue.approver), "%s", user->username);
    free(issue.approval_comment);
    issue.approval_comment = comment;
    wf_time_now_iso(issue.updated_at, sizeof(issue.updated_at));
    if (wf_issue_save(domain, &issue) != 0 || wf_issue_add_comment(domain, user, issue.id, comment) != 0) {
        wf_issue_free(&issue);
        return 1;
    }
    wf_issue_free(&issue);
    return 0;
}

static int wf_issue_comment_cmd(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    struct wf_issue issue;
    char *comment = NULL;
    char full_id[65];

    if (argc < 4) {
        fprintf(stderr, "usage: wf issue comment ISSUE_ID COMMENT\n");
        return 1;
    }
    if (wf_issue_resolve_id(domain, argv[2], full_id) != 0) {
        return 1;
    }
    if (wf_issue_load(domain, full_id, &issue) != 0) {
        return 1;
    }
    wf_issue_free(&issue);
    if (wf_join_args(argc, argv, 3, &comment) != 0) {
        return 1;
    }
    if (wf_issue_add_comment(domain, user, full_id, comment) != 0) {
        free(comment);
        return 1;
    }
    free(comment);
    return 0;
}

static int wf_issue_comments(const struct wf_domain *domain, const char *id)
{
    char path[PATH_MAX];
    FILE *file;
    char line[8192];
    char full_id[65];

    if (wf_issue_resolve_id(domain, id, full_id) != 0) {
        return 1;
    }
    if (wf_issue_comments_path(domain, full_id, path, sizeof(path)) != 0) {
        return 1;
    }
    file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        char *fields[4];
        char *comment = NULL;
        wf_trim_newline(line);
        if (wf_split_tsv(line, fields, 4) != 4) {
            continue;
        }
        if (wf_unescape_field(fields[3], &comment) == 0) {
            printf("%s\t%s\t%s\t%s\n", fields[0], fields[1], fields[2], comment);
            free(comment);
        }
    }
    fclose(file);
    return 0;
}

static int wf_issue_search(const struct wf_domain *domain, int argc, char **argv)
{
    DIR *dir;
    struct dirent *entry;
    char *keyword;

    if (wf_join_args(argc, argv, 2, &keyword) != 0) {
        return 1;
    }
    dir = opendir(domain->issues);
    if (dir == NULL) {
        perror(domain->issues);
        free(keyword);
        return 1;
    }
    while ((entry = readdir(dir)) != NULL) {
        struct wf_issue issue;
        size_t length = strlen(entry->d_name);
        char id[65];

        if (length <= 4 || strcmp(entry->d_name + length - 4, ".tsv") != 0 ||
            strstr(entry->d_name, ".comments.tsv") != NULL) {
            continue;
        }
        snprintf(id, sizeof(id), "%.*s", (int)(length - 4), entry->d_name);
        if (wf_issue_load(domain, id, &issue) == 0) {
            if (strstr(issue.content, keyword) != NULL || strstr(issue.id, keyword) != NULL ||
                strstr(issue.status, keyword) != NULL || strstr(issue.creator, keyword) != NULL) {
                printf("%s\t%s\t%s\t%s\t%.60s\n", issue.id, issue.status, issue.creator, issue.updated_at, issue.content);
            }
            wf_issue_free(&issue);
        }
    }
    closedir(dir);
    free(keyword);
    return 0;
}

struct wf_issue_subcommand {
    const char *name;
    int (*fn)(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv);
};

void wf_issue_usage(FILE *fp)
{
    fprintf(fp, "usage: wf issue COMMAND [ARGS...]\n");
    fprintf(fp, "  (COMMAND may be abbreviated to a unique prefix.)\n");
    fprintf(fp, "  ISSUE_ID may be abbreviated to a unique prefix of 2 or more characters.\n");
    fprintf(fp, "\n");
    fprintf(fp, "  Entity: issue (standard list + CRUD + workflow actions)\n");
    fprintf(fp, "\n");
    fprintf(fp, "  wf issue list\n");
    fprintf(fp, "  wf issue create CONTENTS\n");
    fprintf(fp, "  wf issue show ISSUE_ID\n");
    fprintf(fp, "  wf issue update ISSUE_ID CONTENTS\n");
    fprintf(fp, "  wf issue delete ISSUE_ID\n");
    fprintf(fp, "\n");
    fprintf(fp, "  wf issue approve ISSUE_ID COMMENT\n");
    fprintf(fp, "  wf issue reject ISSUE_ID COMMENT\n");
    fprintf(fp, "  wf issue hold ISSUE_ID COMMENT\n");
    fprintf(fp, "  wf issue resume ISSUE_ID COMMENT\n");
    fprintf(fp, "\n");
    fprintf(fp, "  wf issue comment ISSUE_ID COMMENT\n");
    fprintf(fp, "  wf issue comments ISSUE_ID\n");
    fprintf(fp, "  wf issue search KEYWORD\n");
    fprintf(fp, "\n");
    fprintf(fp, "See 'wf help concepts' for the meaning of roles and ISSUE_ID abbreviation.\n");
}

static int ic_create(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    return wf_issue_create(domain, user, argc, argv);
}

static int ic_list(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    (void)user;
    (void)argc;
    (void)argv;
    return wf_issue_list(domain);
}

static int ic_show(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    (void)user;
    if (argc != 3) {
        fprintf(stderr, "usage: wf issue show ISSUE_ID\n");
        return 1;
    }
    return wf_issue_show(domain, argv[2]);
}

static int ic_update(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    return wf_issue_update(domain, user, argc, argv);
}

static int ic_delete(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: wf issue delete ISSUE_ID\n");
        return 1;
    }
    return wf_issue_delete(domain, user, argv[2]);
}

static int ic_approve(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    return wf_issue_set_status(domain, user, argc, argv, "approved");
}

static int ic_reject(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    return wf_issue_set_status(domain, user, argc, argv, "rejected");
}

static int ic_hold(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    return wf_issue_set_status(domain, user, argc, argv, "hold");
}

static int ic_resume(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    return wf_issue_set_status(domain, user, argc, argv, "open");
}

static int ic_comment(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    return wf_issue_comment_cmd(domain, user, argc, argv);
}

static int ic_comments(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    (void)user;
    if (argc != 3) {
        fprintf(stderr, "usage: wf issue comments ISSUE_ID\n");
        return 1;
    }
    return wf_issue_comments(domain, argv[2]);
}

static int ic_search(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    (void)user;
    return wf_issue_search(domain, argc, argv);
}

static const struct wf_issue_subcommand issue_commands[] = {
    {"create", ic_create},
    {"list", ic_list},
    {"show", ic_show},
    {"update", ic_update},
    {"delete", ic_delete},
    {"approve", ic_approve},
    {"reject", ic_reject},
    {"hold", ic_hold},
    {"resume", ic_resume},
    {"comment", ic_comment},
    {"comments", ic_comments},
    {"search", ic_search},
    {NULL, NULL}
};

int wf_issue_command(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    if (argc < 2 || wf_streq(argv[1], "help") || wf_streq(argv[1], "--help") || wf_streq(argv[1], "-h")) {
        wf_issue_usage(argc < 2 ? stderr : stdout);
        return argc < 2 ? 1 : 0;
    }

    const char *names[32];
    int ni = 0;
    const struct wf_issue_subcommand *entry;
    for (entry = issue_commands; entry->name != NULL && ni < 31; entry += 1) {
        names[ni++] = entry->name;
    }
    names[ni] = NULL;

    const char *matched = NULL;
    enum wf_match_result res = wf_match_prefix(names, argv[1], &matched);
    if (res == WF_MATCH_EXACT || res == WF_MATCH_PREFIX) {
        for (entry = issue_commands; entry->name != NULL; entry += 1) {
            if (entry->name == matched) {
                return entry->fn(domain, user, argc, argv);
            }
        }
        for (entry = issue_commands; entry->name != NULL; entry += 1) {
            if (wf_streq(entry->name, matched)) {
                return entry->fn(domain, user, argc, argv);
            }
        }
    }

    if (res == WF_MATCH_AMBIGUOUS) {
        fprintf(stderr, "ambiguous issue command: %s. See 'wf help semantics'.\n", argv[1]);
        wf_issue_usage(stderr);
        return 1;
    }

    fprintf(stderr, "unknown issue command: %s. See 'wf help semantics' or 'wf help concepts'.\n", argv[1]);
    wf_issue_usage(stderr);
    return 1;
}
