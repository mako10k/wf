#define _POSIX_C_SOURCE 200809L

#include "auth.h"

#include "i18n.h"
#include "storage.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

struct wf_user_record {
    char username[128];
    char role[32];
    char salt[33];
    char hash[65];
};

const char *wf_role_name(enum wf_role role)
{
    return role == WF_ROLE_ASSISTANT ? "Assistant" : "User";
}

int wf_role_parse(const char *value, enum wf_role *role)
{
    if (value == NULL || value[0] == '\0' || wf_streq(value, "User") || wf_streq(value, "user")) {
        *role = WF_ROLE_USER;
        return 0;
    }
    if (wf_streq(value, "Assistant") || wf_streq(value, "assistant")) {
        *role = WF_ROLE_ASSISTANT;
        return 0;
    }
    fprintf(stderr, _("invalid role: %s\n"), value);
    return 1;
}

static int wf_valid_username(const char *username)
{
    const char *cursor;

    if (username == NULL || username[0] == '\0' || strlen(username) >= 128) {
        return 0;
    }
    for (cursor = username; *cursor != '\0'; cursor += 1) {
        if (*cursor == '\t' || *cursor == '\n' || *cursor == '\r') {
            return 0;
        }
    }
    return 1;
}

static void wf_password_hash(const char *salt, const char *password, char out[65])
{
    char input[1024];

    snprintf(input, sizeof(input), "%s:%s", salt, password);
    wf_sha256_hex(input, out);
}

static int wf_write_user_record(
    const struct wf_domain *domain,
    const char *username,
    enum wf_role role,
    const char *salt,
    const char *hash
)
{
    FILE *old_file;
    FILE *new_file;
    char line[1024];
    int replaced = 0;

    if (wf_replace_file(domain->users, &new_file) != 0) {
        return 1;
    }
    old_file = fopen(domain->users, "r");
    if (old_file != NULL) {
        while (fgets(line, sizeof(line), old_file) != NULL) {
            char copy[1024];
            char *fields[4];
            snprintf(copy, sizeof(copy), "%s", line);
            wf_trim_newline(copy);
            if (wf_split_tsv(copy, fields, 4) >= 2 && strcmp(fields[0], username) == 0) {
                fprintf(new_file, "%s\t%s\t%s\t%s\n", username, wf_role_name(role), salt, hash);
                replaced = 1;
            } else {
                fputs(line, new_file);
            }
        }
        fclose(old_file);
    }
    if (!replaced) {
        fprintf(new_file, "%s\t%s\t%s\t%s\n", username, wf_role_name(role), salt, hash);
    }
    return wf_commit_replace(domain->users, new_file);
}

static int wf_read_user_record(const struct wf_domain *domain, const char *username, struct wf_user_record *record)
{
    FILE *file;
    char line[1024];

    file = fopen(domain->users, "r");
    if (file == NULL) {
        return 1;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        char *fields[4];
        wf_trim_newline(line);
        if (wf_split_tsv(line, fields, 4) != 4) {
            continue;
        }
        if (strcmp(fields[0], username) == 0) {
            snprintf(record->username, sizeof(record->username), "%s", fields[0]);
            snprintf(record->role, sizeof(record->role), "%s", fields[1]);
            snprintf(record->salt, sizeof(record->salt), "%s", fields[2]);
            snprintf(record->hash, sizeof(record->hash), "%s", fields[3]);
            fclose(file);
            return 0;
        }
    }
    fclose(file);
    return 1;
}

static int wf_auth_user_exists(const struct wf_domain *domain, const char *username)
{
    struct wf_user_record record;

    return wf_read_user_record(domain, username, &record) == 0;
}

int wf_auth_create(const struct wf_domain *domain, const char *username, const char *role_name)
{
    if (!wf_valid_username(username)) {
        fprintf(stderr, _("invalid username\n"));
        return 1;
    }
    if (wf_auth_user_exists(domain, username)) {
        fprintf(stderr, _("user already exists: %s\n"), username);
        return 1;
    }
    return wf_auth_passwd(domain, username, role_name);
}

int wf_auth_passwd(const struct wf_domain *domain, const char *username, const char *role_name)
{
    enum wf_role role;
    char *password = NULL;
    char salt[33] = "";
    char hash[65] = "";

    if (!wf_valid_username(username)) {
        fprintf(stderr, _("invalid username\n"));
        return 1;
    }
    if (wf_role_parse(role_name, &role) != 0) {
        return 1;
    }
    if (role == WF_ROLE_USER) {
        if (wf_read_password_twice(_("New password: "), _("Retype password: "), &password) != 0) {
            return 1;
        }
        if (wf_random_hex(salt, 32) != 0) {
            free(password);
            return 1;
        }
        wf_password_hash(salt, password, hash);
        free(password);
    }

    if (wf_write_user_record(domain, username, role, salt, hash) != 0) {
        return 1;
    }
    if (role == WF_ROLE_ASSISTANT) {
        printf(_("assistant registered for %s\n"), username);
    } else {
        printf(_("password updated for %s (%s)\n"), username, wf_role_name(role));
    }
    return 0;
}

static int wf_auth_open_session(const struct wf_domain *domain, const char *username, char token[65])
{
    struct wf_user_record record;
    enum wf_role role;
    char *password = NULL;
    char hash[65];
    FILE *file;

    if (wf_read_user_record(domain, username, &record) != 0) {
        if (strcmp(username, "assistant") != 0) {
            fprintf(stderr,
                _("unknown user: %s. Create a User with 'wf user passwd %s User' or an Assistant with 'wf user create %s Assistant'.\n"),
                username,
                username,
                username);
            return 1;
        }
        if (wf_write_user_record(domain, username, WF_ROLE_ASSISTANT, "", "") != 0 ||
            wf_read_user_record(domain, username, &record) != 0) {
            return 1;
        }
    }

    if (wf_role_parse(record.role, &role) != 0) {
        return 1;
    }
    if (role == WF_ROLE_USER) {
        if (wf_read_password_once("Password: ", &password) != 0) {
            return 1;
        }
        wf_password_hash(record.salt, password, hash);
        free(password);
        if (strcmp(hash, record.hash) != 0) {
            fprintf(stderr, _("authentication failed\n"));
            return 1;
        }
    }
    if (wf_random_hex(token, 64) != 0) {
        return 1;
    }
    file = fopen(domain->sessions, "a");
    if (file == NULL) {
        perror(domain->sessions);
        return 1;
    }
    fprintf(file, "%s\t%s\t%s\n", token, record.username, record.role);
    if (fclose(file) != 0) {
        perror(domain->sessions);
        return 1;
    }
    return 0;
}

static int wf_auth_remove_session_token(const struct wf_domain *domain, const char *token)
{
    FILE *old_file;
    FILE *new_file;
    char line[1024];

    if (token == NULL || token[0] == '\0') {
        return 0;
    }
    if (wf_replace_file(domain->sessions, &new_file) != 0) {
        return 1;
    }
    old_file = fopen(domain->sessions, "r");
    if (old_file != NULL) {
        while (fgets(line, sizeof(line), old_file) != NULL) {
            char copy[1024];
            char *fields[3];
            snprintf(copy, sizeof(copy), "%s", line);
            wf_trim_newline(copy);
            if (wf_split_tsv(copy, fields, 3) == 3 && strcmp(fields[0], token) == 0) {
                continue;
            }
            fputs(line, new_file);
        }
        fclose(old_file);
    }
    if (wf_commit_replace(domain->sessions, new_file) != 0) {
        return 1;
    }
    return 0;
}

static int wf_auth_prepare_exec_shell_env(const struct wf_domain *domain, const char *username)
{
    if (setenv("WF_EXEC_USER", username, 1) != 0) {
        perror("setenv");
        return 1;
    }
    if (setenv("WF_EXEC_DOMAIN", domain->id, 1) != 0) {
        perror("setenv");
        return 1;
    }
    return 0;
}

int wf_auth_env_export(const struct wf_domain *domain, const char *username)
{
    char token[65];

    if (wf_auth_open_session(domain, username, token) != 0) {
        return 1;
    }
    printf("export WF_TOKEN='%s'\n", token);
    return 0;
}

int wf_auth_env_clear(const struct wf_domain *domain)
{
    const char *token = getenv("WF_TOKEN");

    if (wf_auth_remove_session_token(domain, token) != 0) {
        return 1;
    }
    printf("unset WF_TOKEN\n");
    return 0;
}

int wf_auth_exec(const struct wf_domain *domain, const char *username, int argc, char **argv)
{
    char token[65];
    const char *shell;
    pid_t child;
    int status;
    int wait_rc;
    int clear_rc;

    if (wf_auth_open_session(domain, username, token) != 0) {
        return 1;
    }

    child = fork();
    if (child < 0) {
        perror("fork");
        wf_auth_remove_session_token(domain, token);
        return 1;
    }

    if (child == 0) {
        if (setenv("WF_TOKEN", token, 1) != 0) {
            perror("setenv");
            _exit(1);
        }
        if (argc == 0) {
            shell = getenv("SHELL");
            if (shell == NULL || shell[0] == '\0') {
                shell = "/bin/sh";
            }
            if (wf_auth_prepare_exec_shell_env(domain, username) != 0) {
                _exit(1);
            }
            fprintf(stderr, _("wf exec shell: user=%s domain=%s\n"), username, domain->id);
            execl(shell, shell, "-i", (char *)NULL);
            perror(shell);
            _exit(127);
        }
        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }

    do {
        wait_rc = waitpid(child, &status, 0);
    } while (wait_rc < 0 && errno == EINTR);

    if (wait_rc >= 0 && argc == 0) {
        if (WIFEXITED(status)) {
            fprintf(stderr, _("wf exec shell returned: user=%s domain=%s exit=%d\n"), username, domain->id, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, _("wf exec shell returned: user=%s domain=%s signal=%d\n"), username, domain->id, WTERMSIG(status));
        } else {
            fprintf(stderr, _("wf exec shell returned: user=%s domain=%s\n"), username, domain->id);
        }
    }

    clear_rc = wf_auth_remove_session_token(domain, token);
    if (wait_rc < 0) {
        perror("waitpid");
        return 1;
    }
    if (clear_rc != 0) {
        return 1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
}

int wf_auth_current_user(const struct wf_domain *domain, struct wf_user *user)
{
    const char *token = getenv("WF_TOKEN");
    FILE *file;
    char line[1024];

    if (token == NULL || token[0] == '\0') {
        fprintf(stderr, _("not logged in: run 'wf exec USERNAME -- COMMAND' or 'eval \"$(wf env export USERNAME)\"'\n"));
        return 1;
    }
    file = fopen(domain->sessions, "r");
    if (file == NULL) {
        fprintf(stderr, _("not logged in: run 'wf exec USERNAME -- COMMAND' or 'eval \"$(wf env export USERNAME)\"'\n"));
        return 1;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        char *fields[3];
        enum wf_role role;
        wf_trim_newline(line);
        if (wf_split_tsv(line, fields, 3) != 3) {
            continue;
        }
        if (strcmp(fields[0], token) == 0 && wf_role_parse(fields[2], &role) == 0) {
            snprintf(user->username, sizeof(user->username), "%s", fields[1]);
            user->role = role;
            fclose(file);
            return 0;
        }
    }
    fclose(file);
    fprintf(stderr, _("invalid WF_TOKEN: run 'wf exec USERNAME -- COMMAND' or 'eval \"$(wf env export USERNAME)\"'\n"));
    return 1;
}
