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
    char issue_state[16];
    char *requested_condition;
    char requested_condition_state[16];
    char approver[128];
    char decision_type[16];
    char decision_scope[16];
    char decision_basis[24];
    char *decision_condition;
    char *decision_comment;
    char created_at[32];
    char updated_at[32];
};

static void wf_issue_free(struct wf_issue *issue)
{
    free(issue->content);
    free(issue->requested_condition);
    free(issue->decision_condition);
    free(issue->decision_comment);
    memset(issue, 0, sizeof(*issue));
}

static int wf_valid_issue_state(const char *issue_state)
{
    return wf_streq(issue_state, "open") || wf_streq(issue_state, "approved") || wf_streq(issue_state, "rejected") ||
           wf_streq(issue_state, "closed") || wf_streq(issue_state, "hold") || wf_streq(issue_state, "invalid");
}

static int wf_valid_requested_condition_state(const char *state)
{
    return wf_streq(state, "none") || wf_streq(state, "active") || wf_streq(state, "invalid");
}

static int wf_valid_decision_type(const char *decision_type)
{
    return wf_streq(decision_type, "none") || wf_streq(decision_type, "approve") ||
           wf_streq(decision_type, "reject") || wf_streq(decision_type, "hold");
}

static int wf_valid_decision_scope(const char *decision_scope)
{
    return wf_streq(decision_scope, "none") || wf_streq(decision_scope, "full") ||
           wf_streq(decision_scope, "partial");
}

static int wf_valid_decision_basis(const char *decision_basis)
{
    return wf_streq(decision_basis, "none") || wf_streq(decision_basis, "as_requested") ||
           wf_streq(decision_basis, "reviewer_override");
}

static int wf_issue_set_text(char **slot, const char *value)
{
    char *copy = wf_strdup(value);

    if (copy == NULL) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    free(*slot);
    *slot = copy;
    return 0;
}

static int wf_issue_init_defaults(struct wf_issue *issue)
{
    memset(issue, 0, sizeof(*issue));

    issue->content = wf_strdup("");
    issue->requested_condition = wf_strdup("");
    issue->decision_condition = wf_strdup("");
    issue->decision_comment = wf_strdup("");
    if (issue->content == NULL || issue->requested_condition == NULL ||
        issue->decision_condition == NULL || issue->decision_comment == NULL) {
        wf_issue_free(issue);
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    snprintf(issue->issue_state, sizeof(issue->issue_state), "open");
    snprintf(issue->requested_condition_state, sizeof(issue->requested_condition_state), "none");
    snprintf(issue->decision_type, sizeof(issue->decision_type), "none");
    snprintf(issue->decision_scope, sizeof(issue->decision_scope), "none");
    snprintf(issue->decision_basis, sizeof(issue->decision_basis), "none");
    return 0;
}

static int wf_issue_validate(const struct wf_issue *issue)
{
    if (!wf_valid_issue_state(issue->issue_state)) {
        fprintf(stderr, "corrupt issue state: %s\n", issue->issue_state);
        return 1;
    }
    if (!wf_valid_requested_condition_state(issue->requested_condition_state)) {
        fprintf(stderr, "corrupt requested condition state: %s\n", issue->requested_condition_state);
        return 1;
    }
    if (!wf_valid_decision_type(issue->decision_type)) {
        fprintf(stderr, "corrupt decision type: %s\n", issue->decision_type);
        return 1;
    }
    if (!wf_valid_decision_scope(issue->decision_scope)) {
        fprintf(stderr, "corrupt decision scope: %s\n", issue->decision_scope);
        return 1;
    }
    if (!wf_valid_decision_basis(issue->decision_basis)) {
        fprintf(stderr, "corrupt decision basis: %s\n", issue->decision_basis);
        return 1;
    }
    return 0;
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
    char *fields[14];
    char *content = NULL;
    char *requested_condition = NULL;
    char *decision_condition = NULL;
    char *decision_comment = NULL;
    int field_count;

    if (wf_issue_init_defaults(issue) != 0) {
        return 1;
    }
    if (wf_issue_path(domain, id, path, sizeof(path)) != 0) {
        wf_issue_free(issue);
        return 1;
    }
    file = fopen(path, "r");
    if (file == NULL) {
        wf_issue_free(issue);
        fprintf(stderr, "issue not found: %s\n", id);
        return 1;
    }
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        wf_issue_free(issue);
        fprintf(stderr, "corrupt issue: %s\n", id);
        return 1;
    }
    fclose(file);
    wf_trim_newline(line);
    field_count = wf_split_tsv(line, fields, 14);
    if (field_count != 8 && field_count != 14) {
        wf_issue_free(issue);
        fprintf(stderr, "corrupt issue: %s\n", id);
        return 1;
    }
    if (wf_unescape_field(fields[1], &content) != 0) {
        wf_issue_free(issue);
        return 1;
    }
    snprintf(issue->id, sizeof(issue->id), "%s", fields[0]);
    if (wf_issue_set_text(&issue->content, content) != 0) {
        free(content);
        wf_issue_free(issue);
        return 1;
    }
    free(content);
    snprintf(issue->creator, sizeof(issue->creator), "%s", fields[2]);

    if (field_count == 14) {
        if (wf_unescape_field(fields[4], &requested_condition) != 0 ||
            wf_unescape_field(fields[10], &decision_condition) != 0 ||
            wf_unescape_field(fields[11], &decision_comment) != 0) {
            free(requested_condition);
            free(decision_condition);
            free(decision_comment);
            wf_issue_free(issue);
            return 1;
        }
        snprintf(issue->issue_state, sizeof(issue->issue_state), "%s", fields[3]);
        if (wf_issue_set_text(&issue->requested_condition, requested_condition) != 0 ||
            wf_issue_set_text(&issue->decision_condition, decision_condition) != 0 ||
            wf_issue_set_text(&issue->decision_comment, decision_comment) != 0) {
            free(requested_condition);
            free(decision_condition);
            free(decision_comment);
            wf_issue_free(issue);
            return 1;
        }
        free(requested_condition);
        free(decision_condition);
        free(decision_comment);
        snprintf(issue->requested_condition_state, sizeof(issue->requested_condition_state), "%s", fields[5]);
        snprintf(issue->approver, sizeof(issue->approver), "%s", fields[6]);
        snprintf(issue->decision_type, sizeof(issue->decision_type), "%s", fields[7]);
        snprintf(issue->decision_scope, sizeof(issue->decision_scope), "%s", fields[8]);
        snprintf(issue->decision_basis, sizeof(issue->decision_basis), "%s", fields[9]);
        snprintf(issue->created_at, sizeof(issue->created_at), "%s", fields[12]);
        snprintf(issue->updated_at, sizeof(issue->updated_at), "%s", fields[13]);
    } else {
        if (wf_unescape_field(fields[5], &decision_comment) != 0) {
            wf_issue_free(issue);
            return 1;
        }
        snprintf(issue->issue_state, sizeof(issue->issue_state), "%s", fields[3]);
        snprintf(issue->approver, sizeof(issue->approver), "%s", fields[4]);
        if (wf_issue_set_text(&issue->decision_comment, decision_comment) != 0) {
            free(decision_comment);
            wf_issue_free(issue);
            return 1;
        }
        free(decision_comment);
        snprintf(issue->created_at, sizeof(issue->created_at), "%s", fields[6]);
        snprintf(issue->updated_at, sizeof(issue->updated_at), "%s", fields[7]);

        if (wf_streq(issue->issue_state, "approved")) {
            snprintf(issue->decision_type, sizeof(issue->decision_type), "approve");
            snprintf(issue->decision_scope, sizeof(issue->decision_scope), "full");
        } else if (wf_streq(issue->issue_state, "rejected")) {
            snprintf(issue->decision_type, sizeof(issue->decision_type), "reject");
            snprintf(issue->decision_scope, sizeof(issue->decision_scope), "full");
        } else if (wf_streq(issue->issue_state, "hold")) {
            snprintf(issue->decision_type, sizeof(issue->decision_type), "hold");
            snprintf(issue->decision_scope, sizeof(issue->decision_scope), "full");
        }
    }

    if (wf_issue_validate(issue) != 0) {
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
    char *requested_condition = NULL;
    char *decision_condition = NULL;
    char *decision_comment = NULL;
    int rc = 0;

    if (wf_issue_path(domain, issue->id, path, sizeof(path)) != 0) {
        return 1;
    }
    if (wf_issue_validate(issue) != 0) {
        return 1;
    }
    if (wf_escape_field(issue->content, &content) != 0 ||
        wf_escape_field(issue->requested_condition, &requested_condition) != 0 ||
        wf_escape_field(issue->decision_condition, &decision_condition) != 0 ||
        wf_escape_field(issue->decision_comment, &decision_comment) != 0) {
        free(content);
        free(requested_condition);
        free(decision_condition);
        free(decision_comment);
        return 1;
    }
    if (wf_replace_file(path, &file) != 0) {
        free(content);
        free(requested_condition);
        free(decision_condition);
        free(decision_comment);
        return 1;
    }
    fprintf(file, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
            issue->id,
            content,
            issue->creator,
            issue->issue_state,
            requested_condition,
            issue->requested_condition_state,
            issue->approver,
            issue->decision_type,
            issue->decision_scope,
            issue->decision_basis,
            decision_condition,
            decision_comment,
            issue->created_at,
            issue->updated_at);
    if (wf_commit_replace(path, file) != 0) {
        rc = 1;
    }
    free(content);
    free(requested_condition);
    free(decision_condition);
    free(decision_comment);
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

static int wf_issue_add_action_comment(
    const struct wf_domain *domain,
    const struct wf_user *user,
    const char *id,
    const char *prefix,
    const char *text
)
{
    char *message;
    size_t length;
    int rc;

    if (text == NULL || text[0] == '\0') {
        return 0;
    }
    length = strlen(prefix) + 2 + strlen(text) + 1;
    message = malloc(length);
    if (message == NULL) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    snprintf(message, length, "%s: %s", prefix, text);
    rc = wf_issue_add_comment(domain, user, id, message);
    free(message);
    return rc;
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
    snprintf(issue.issue_state, sizeof(issue.issue_state), "open");
    issue.requested_condition = wf_strdup("");
    snprintf(issue.requested_condition_state, sizeof(issue.requested_condition_state), "none");
    issue.approver[0] = '\0';
    snprintf(issue.decision_type, sizeof(issue.decision_type), "none");
    snprintf(issue.decision_scope, sizeof(issue.decision_scope), "none");
    snprintf(issue.decision_basis, sizeof(issue.decision_basis), "none");
    issue.decision_condition = wf_strdup("");
    issue.decision_comment = wf_strdup("");
    if (issue.requested_condition == NULL || issue.decision_condition == NULL || issue.decision_comment == NULL) {
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
    printf("id\tissue_state\trequested_condition_state\tdecision_type\tdecision_scope\tdecision_basis\tcreator\tupdated_at\tcontent\n");
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
             printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%.60s\n",
                   issue.id,
                   issue.issue_state,
                   issue.requested_condition_state,
                   issue.decision_type,
                 issue.decision_scope,
                 issue.decision_basis,
                   issue.creator,
                   issue.updated_at,
                   issue.content);
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
    printf("issue_state: %s\n", issue.issue_state);
    printf("creator: %s\n", issue.creator);
    printf("requested_condition_state: %s\n", issue.requested_condition_state);
    printf("requested_condition: %s\n", issue.requested_condition);
    printf("approver: %s\n", issue.approver);
    printf("decision_type: %s\n", issue.decision_type);
    printf("decision_scope: %s\n", issue.decision_scope);
    printf("decision_basis: %s\n", issue.decision_basis);
    printf("decision_condition: %s\n", issue.decision_condition);
    printf("decision_comment: %s\n", issue.decision_comment);
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
    if (wf_streq(issue.issue_state, "invalid") || wf_streq(issue.issue_state, "closed")) {
        wf_issue_free(&issue);
        fprintf(stderr, "cannot update issue after it has reached a terminal state\n");
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
    if (wf_streq(issue.issue_state, "invalid") || wf_streq(issue.issue_state, "closed")) {
        wf_issue_free(&issue);
        fprintf(stderr, "cannot delete issue after it has reached a terminal state\n");
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

static int wf_issue_request_condition(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    struct wf_issue issue;
    char *condition = NULL;
    char full_id[65];

    if (argc < 4) {
        fprintf(stderr, "usage: wf issue request-condition ISSUE_ID CONDITION\n");
        return 1;
    }
    if (user->role != WF_ROLE_ASSISTANT) {
        fprintf(stderr, "permission denied: only Assistant can update requested conditions\n");
        return 1;
    }
    if (wf_issue_resolve_id(domain, argv[2], full_id) != 0) {
        return 1;
    }
    if (wf_issue_load(domain, full_id, &issue) != 0) {
        return 1;
    }
    if (wf_streq(issue.issue_state, "invalid") || wf_streq(issue.issue_state, "closed")) {
        wf_issue_free(&issue);
        fprintf(stderr, "cannot update requested condition after the issue reached a terminal state\n");
        return 1;
    }
    if (!wf_streq(issue.issue_state, "open")) {
        wf_issue_free(&issue);
        fprintf(stderr, "cannot update requested condition unless issue is open\n");
        return 1;
    }
    if (strcmp(issue.creator, user->username) != 0) {
        wf_issue_free(&issue);
        fprintf(stderr, "permission denied: Assistant can update own issues only\n");
        return 1;
    }
    if (wf_join_args(argc, argv, 3, &condition) != 0) {
        wf_issue_free(&issue);
        return 1;
    }
    free(issue.requested_condition);
    issue.requested_condition = condition;
    snprintf(issue.requested_condition_state, sizeof(issue.requested_condition_state), "active");
    wf_time_now_iso(issue.updated_at, sizeof(issue.updated_at));
    if (wf_issue_save(domain, &issue) != 0 ||
        wf_issue_add_action_comment(domain, user, issue.id, "requested_condition", issue.requested_condition) != 0) {
        wf_issue_free(&issue);
        return 1;
    }
    wf_issue_free(&issue);
    return 0;
}

static int wf_issue_clear_condition(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    struct wf_issue issue;
    char *reason = NULL;
    char full_id[65];

    if (argc < 4) {
        fprintf(stderr, "usage: wf issue clear-condition ISSUE_ID REASON\n");
        return 1;
    }
    if (user->role != WF_ROLE_ASSISTANT) {
        fprintf(stderr, "permission denied: only Assistant can update requested conditions\n");
        return 1;
    }
    if (wf_issue_resolve_id(domain, argv[2], full_id) != 0) {
        return 1;
    }
    if (wf_issue_load(domain, full_id, &issue) != 0) {
        return 1;
    }
    if (wf_streq(issue.issue_state, "invalid") || wf_streq(issue.issue_state, "closed")) {
        wf_issue_free(&issue);
        fprintf(stderr, "cannot clear requested condition after the issue reached a terminal state\n");
        return 1;
    }
    if (!wf_streq(issue.issue_state, "open")) {
        wf_issue_free(&issue);
        fprintf(stderr, "cannot clear requested condition unless issue is open\n");
        return 1;
    }
    if (strcmp(issue.creator, user->username) != 0) {
        wf_issue_free(&issue);
        fprintf(stderr, "permission denied: Assistant can update own issues only\n");
        return 1;
    }
    if (wf_join_args(argc, argv, 3, &reason) != 0) {
        wf_issue_free(&issue);
        return 1;
    }
    if (wf_issue_set_text(&issue.requested_condition, "") != 0) {
        free(reason);
        wf_issue_free(&issue);
        return 1;
    }
    snprintf(issue.requested_condition_state, sizeof(issue.requested_condition_state), "none");
    wf_time_now_iso(issue.updated_at, sizeof(issue.updated_at));
    if (wf_issue_save(domain, &issue) != 0 ||
        wf_issue_add_action_comment(domain, user, issue.id, "requested_condition cleared", reason) != 0) {
        free(reason);
        wf_issue_free(&issue);
        return 1;
    }
    free(reason);
    wf_issue_free(&issue);
    return 0;
}

static int wf_issue_apply_decision(
    const struct wf_domain *domain,
    const struct wf_user *user,
    const char *id,
    const char *issue_state,
    const char *decision_type,
    const char *decision_scope,
    int use_requested_condition,
    const char *explicit_condition,
    const char *decision_comment
)
{
    struct wf_issue issue;
    char full_id[65];
    const char *basis = "none";
    const char *condition = "";

    if (user->role != WF_ROLE_USER) {
        fprintf(stderr, "permission denied: only User can change approval status\n");
        return 1;
    }
    if (wf_issue_resolve_id(domain, id, full_id) != 0) {
        return 1;
    }
    if (wf_issue_load(domain, full_id, &issue) != 0) {
        return 1;
    }
    if (wf_streq(issue.issue_state, "invalid") || wf_streq(issue.issue_state, "closed")) {
        wf_issue_free(&issue);
        fprintf(stderr, "cannot review issue after it has reached a terminal state\n");
        return 1;
    }
    if (!wf_streq(issue.issue_state, "open")) {
        wf_issue_free(&issue);
        fprintf(stderr, "cannot change review decision unless issue is open\n");
        return 1;
    }

    if (explicit_condition != NULL) {
        basis = "reviewer_override";
        condition = explicit_condition;
    } else if (use_requested_condition && wf_streq(issue.requested_condition_state, "active") &&
               issue.requested_condition[0] != '\0') {
        basis = "as_requested";
        condition = issue.requested_condition;
    }

    snprintf(issue.issue_state, sizeof(issue.issue_state), "%s", issue_state);
    snprintf(issue.approver, sizeof(issue.approver), "%s", user->username);
    snprintf(issue.decision_type, sizeof(issue.decision_type), "%s", decision_type);
    snprintf(issue.decision_scope, sizeof(issue.decision_scope), "%s", decision_scope);
    snprintf(issue.decision_basis, sizeof(issue.decision_basis), "%s", basis);
    if (wf_issue_set_text(&issue.decision_condition, condition) != 0 ||
        wf_issue_set_text(&issue.decision_comment, decision_comment) != 0) {
        wf_issue_free(&issue);
        return 1;
    }
    wf_time_now_iso(issue.updated_at, sizeof(issue.updated_at));
    if (wf_issue_save(domain, &issue) != 0 || wf_issue_add_comment(domain, user, issue.id, decision_comment) != 0) {
        wf_issue_free(&issue);
        return 1;
    }
    wf_issue_free(&issue);
    return 0;
}

static int wf_issue_resume(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    struct wf_issue issue;
    char *comment = NULL;
    char full_id[65];

    if (argc < 4) {
        fprintf(stderr, "usage: wf issue resume ISSUE_ID COMMENT\n");
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
    if (!wf_streq(issue.issue_state, "hold")) {
        wf_issue_free(&issue);
        fprintf(stderr, "cannot resume issue unless it is on hold\n");
        return 1;
    }
    if (wf_join_args(argc, argv, 3, &comment) != 0) {
        wf_issue_free(&issue);
        return 1;
    }
    snprintf(issue.issue_state, sizeof(issue.issue_state), "open");
    snprintf(issue.approver, sizeof(issue.approver), "%s", user->username);
    snprintf(issue.decision_type, sizeof(issue.decision_type), "none");
    snprintf(issue.decision_scope, sizeof(issue.decision_scope), "none");
    snprintf(issue.decision_basis, sizeof(issue.decision_basis), "none");
    if (wf_issue_set_text(&issue.decision_condition, "") != 0 ||
        wf_issue_set_text(&issue.decision_comment, "") != 0) {
        free(comment);
        wf_issue_free(&issue);
        return 1;
    }
    wf_time_now_iso(issue.updated_at, sizeof(issue.updated_at));
    if (wf_issue_save(domain, &issue) != 0 || wf_issue_add_comment(domain, user, issue.id, comment) != 0) {
        free(comment);
        wf_issue_free(&issue);
        return 1;
    }
    free(comment);
    wf_issue_free(&issue);
    return 0;
}

static int wf_issue_invalidate_condition(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    struct wf_issue issue;
    char *reason = NULL;
    char full_id[65];

    if (argc < 4) {
        fprintf(stderr, "usage: wf issue invalidate-condition ISSUE_ID REASON\n");
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
    if (wf_streq(issue.issue_state, "invalid") || wf_streq(issue.issue_state, "closed")) {
        wf_issue_free(&issue);
        fprintf(stderr, "cannot invalidate requested condition after the issue reached a terminal state\n");
        return 1;
    }
    if (issue.requested_condition[0] == '\0' && wf_streq(issue.requested_condition_state, "none")) {
        wf_issue_free(&issue);
        fprintf(stderr, "no requested condition to invalidate\n");
        return 1;
    }
    if (wf_streq(issue.requested_condition_state, "invalid")) {
        wf_issue_free(&issue);
        fprintf(stderr, "requested condition is already invalid\n");
        return 1;
    }
    if (!wf_streq(issue.decision_type, "none") || wf_streq(issue.decision_basis, "as_requested")) {
        wf_issue_free(&issue);
        fprintf(stderr, "cannot invalidate requested condition after a review decision\n");
        return 1;
    }
    if (wf_join_args(argc, argv, 3, &reason) != 0) {
        wf_issue_free(&issue);
        return 1;
    }
    snprintf(issue.requested_condition_state, sizeof(issue.requested_condition_state), "invalid");
    snprintf(issue.approver, sizeof(issue.approver), "%s", user->username);
    wf_time_now_iso(issue.updated_at, sizeof(issue.updated_at));
    if (wf_issue_save(domain, &issue) != 0 ||
        wf_issue_add_action_comment(domain, user, issue.id, "requested_condition invalidated", reason) != 0) {
        free(reason);
        wf_issue_free(&issue);
        return 1;
    }
    free(reason);
    wf_issue_free(&issue);
    return 0;
}

static int wf_issue_invalidate(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    struct wf_issue issue;
    char *reason = NULL;
    char full_id[65];

    if (argc < 4) {
        fprintf(stderr, "usage: wf issue invalidate ISSUE_ID REASON\n");
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
    if (wf_streq(issue.issue_state, "invalid") || wf_streq(issue.issue_state, "closed")) {
        wf_issue_free(&issue);
        fprintf(stderr, "cannot invalidate issue after it has reached a terminal state\n");
        return 1;
    }
    if (wf_join_args(argc, argv, 3, &reason) != 0) {
        wf_issue_free(&issue);
        return 1;
    }
    snprintf(issue.issue_state, sizeof(issue.issue_state), "invalid");
    if (wf_streq(issue.requested_condition_state, "active")) {
        snprintf(issue.requested_condition_state, sizeof(issue.requested_condition_state), "invalid");
    }
    snprintf(issue.approver, sizeof(issue.approver), "%s", user->username);
    snprintf(issue.decision_type, sizeof(issue.decision_type), "none");
    snprintf(issue.decision_scope, sizeof(issue.decision_scope), "none");
    snprintf(issue.decision_basis, sizeof(issue.decision_basis), "none");
    if (wf_issue_set_text(&issue.decision_condition, "") != 0 ||
        wf_issue_set_text(&issue.decision_comment, reason) != 0) {
        free(reason);
        wf_issue_free(&issue);
        return 1;
    }
    wf_time_now_iso(issue.updated_at, sizeof(issue.updated_at));
    if (wf_issue_save(domain, &issue) != 0 || wf_issue_add_comment(domain, user, issue.id, reason) != 0) {
        free(reason);
        wf_issue_free(&issue);
        return 1;
    }
    free(reason);
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
    printf("id\tissue_state\trequested_condition_state\tdecision_type\tdecision_scope\tdecision_basis\tcreator\tupdated_at\tcontent\n");
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
                strstr(issue.issue_state, keyword) != NULL || strstr(issue.creator, keyword) != NULL ||
                strstr(issue.requested_condition, keyword) != NULL || strstr(issue.requested_condition_state, keyword) != NULL ||
                strstr(issue.decision_type, keyword) != NULL || strstr(issue.decision_scope, keyword) != NULL ||
                strstr(issue.decision_basis, keyword) != NULL || strstr(issue.decision_condition, keyword) != NULL ||
                strstr(issue.decision_comment, keyword) != NULL) {
                printf("%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%.60s\n",
                       issue.id,
                       issue.issue_state,
                       issue.requested_condition_state,
                       issue.decision_type,
                       issue.decision_scope,
                       issue.decision_basis,
                       issue.creator,
                       issue.updated_at,
                       issue.content);
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
    fprintf(fp, "  Entity: issue (standard list + CRUD + review actions)\n");
    fprintf(fp, "\n");
    fprintf(fp, "  wf issue list\n");
    fprintf(fp, "  wf issue create CONTENTS\n");
    fprintf(fp, "  wf issue show ISSUE_ID\n");
    fprintf(fp, "  wf issue update ISSUE_ID CONTENTS\n");
    fprintf(fp, "  wf issue delete ISSUE_ID\n");
    fprintf(fp, "\n");
    fprintf(fp, "  wf issue request-condition ISSUE_ID CONDITION\n");
    fprintf(fp, "      Set or replace the requested condition while the issue is still open.\n");
    fprintf(fp, "  wf issue clear-condition ISSUE_ID REASON\n");
    fprintf(fp, "      Remove the requested condition while the issue is still open.\n");
    fprintf(fp, "\n");
    fprintf(fp, "  wf issue approve ISSUE_ID COMMENT\n");
    fprintf(fp, "      Approve using the requested condition if one is active.\n");
    fprintf(fp, "  wf issue approve-with ISSUE_ID CONDITION COMMENT\n");
    fprintf(fp, "      Approve using a reviewer-supplied condition instead.\n");
    fprintf(fp, "  wf issue approve-partial ISSUE_ID CONDITION COMMENT\n");
    fprintf(fp, "      Approve only part of the request under a reviewer condition.\n");
    fprintf(fp, "  wf issue reject ISSUE_ID COMMENT\n");
    fprintf(fp, "      Reject using the requested condition if one is active.\n");
    fprintf(fp, "  wf issue reject-with ISSUE_ID CONDITION COMMENT\n");
    fprintf(fp, "      Reject using a reviewer-supplied condition instead.\n");
    fprintf(fp, "  wf issue hold ISSUE_ID COMMENT\n");
    fprintf(fp, "      Put the issue on hold using the requested condition if one is active.\n");
    fprintf(fp, "  wf issue hold-with ISSUE_ID CONDITION COMMENT\n");
    fprintf(fp, "      Put the issue on hold using a reviewer-supplied condition instead.\n");
    fprintf(fp, "  wf issue resume ISSUE_ID COMMENT\n");
    fprintf(fp, "      Return a held issue to open state.\n");
    fprintf(fp, "  wf issue invalidate-condition ISSUE_ID REASON\n");
    fprintf(fp, "      Invalidate only the requested condition and keep the issue itself active.\n");
    fprintf(fp, "  wf issue invalidate ISSUE_ID REASON\n");
    fprintf(fp, "      Mark the whole issue invalid.\n");
    fprintf(fp, "\n");
    fprintf(fp, "  wf issue comment ISSUE_ID COMMENT\n");
    fprintf(fp, "  wf issue comments ISSUE_ID\n");
    fprintf(fp, "  wf issue search KEYWORD\n");
    fprintf(fp, "\n");
    fprintf(fp, "  Quote CONDITION, COMMENT, and REASON when they contain spaces.\n");
    fprintf(fp, "  Example: wf issue approve-with 0123abcd \"only after canary passes\" \"looks acceptable\"\n");
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

static int ic_request_condition(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    return wf_issue_request_condition(domain, user, argc, argv);
}

static int ic_clear_condition(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    return wf_issue_clear_condition(domain, user, argc, argv);
}

static int ic_approve(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    char *comment = NULL;
    int rc;

    if (argc < 4) {
        fprintf(stderr, "usage: wf issue approve ISSUE_ID COMMENT\n");
        return 1;
    }
    if (wf_join_args(argc, argv, 3, &comment) != 0) {
        return 1;
    }
    rc = wf_issue_apply_decision(domain, user, argv[2], "approved", "approve", "full", 1, NULL, comment);
    free(comment);
    return rc;
}

static int ic_approve_with(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    char *comment = NULL;
    int rc;

    if (argc < 5) {
        fprintf(stderr, "usage: wf issue approve-with ISSUE_ID CONDITION COMMENT\n");
        return 1;
    }
    if (wf_join_args(argc, argv, 4, &comment) != 0) {
        return 1;
    }
    rc = wf_issue_apply_decision(domain, user, argv[2], "approved", "approve", "full", 0, argv[3], comment);
    free(comment);
    return rc;
}

static int ic_approve_partial(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    char *comment = NULL;
    int rc;

    if (argc < 5) {
        fprintf(stderr, "usage: wf issue approve-partial ISSUE_ID CONDITION COMMENT\n");
        return 1;
    }
    if (wf_join_args(argc, argv, 4, &comment) != 0) {
        return 1;
    }
    rc = wf_issue_apply_decision(domain, user, argv[2], "approved", "approve", "partial", 0, argv[3], comment);
    free(comment);
    return rc;
}

static int ic_reject(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    char *comment = NULL;
    int rc;

    if (argc < 4) {
        fprintf(stderr, "usage: wf issue reject ISSUE_ID COMMENT\n");
        return 1;
    }
    if (wf_join_args(argc, argv, 3, &comment) != 0) {
        return 1;
    }
    rc = wf_issue_apply_decision(domain, user, argv[2], "rejected", "reject", "full", 1, NULL, comment);
    free(comment);
    return rc;
}

static int ic_reject_with(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    char *comment = NULL;
    int rc;

    if (argc < 5) {
        fprintf(stderr, "usage: wf issue reject-with ISSUE_ID CONDITION COMMENT\n");
        return 1;
    }
    if (wf_join_args(argc, argv, 4, &comment) != 0) {
        return 1;
    }
    rc = wf_issue_apply_decision(domain, user, argv[2], "rejected", "reject", "full", 0, argv[3], comment);
    free(comment);
    return rc;
}

static int ic_hold(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    char *comment = NULL;
    int rc;

    if (argc < 4) {
        fprintf(stderr, "usage: wf issue hold ISSUE_ID COMMENT\n");
        return 1;
    }
    if (wf_join_args(argc, argv, 3, &comment) != 0) {
        return 1;
    }
    rc = wf_issue_apply_decision(domain, user, argv[2], "hold", "hold", "full", 1, NULL, comment);
    free(comment);
    return rc;
}

static int ic_hold_with(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    char *comment = NULL;
    int rc;

    if (argc < 5) {
        fprintf(stderr, "usage: wf issue hold-with ISSUE_ID CONDITION COMMENT\n");
        return 1;
    }
    if (wf_join_args(argc, argv, 4, &comment) != 0) {
        return 1;
    }
    rc = wf_issue_apply_decision(domain, user, argv[2], "hold", "hold", "full", 0, argv[3], comment);
    free(comment);
    return rc;
}

static int ic_resume(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    return wf_issue_resume(domain, user, argc, argv);
}

static int ic_invalidate_condition(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    return wf_issue_invalidate_condition(domain, user, argc, argv);
}

static int ic_invalidate(const struct wf_domain *domain, const struct wf_user *user, int argc, char **argv)
{
    return wf_issue_invalidate(domain, user, argc, argv);
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
    {"request-condition", ic_request_condition},
    {"clear-condition", ic_clear_condition},
    {"approve", ic_approve},
    {"approve-with", ic_approve_with},
    {"approve-partial", ic_approve_partial},
    {"reject", ic_reject},
    {"reject-with", ic_reject_with},
    {"hold", ic_hold},
    {"hold-with", ic_hold_with},
    {"resume", ic_resume},
    {"invalidate-condition", ic_invalidate_condition},
    {"invalidate", ic_invalidate},
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
