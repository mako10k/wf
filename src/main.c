#define _POSIX_C_SOURCE 200809L

#include "auth.h"
#include "cmd.h"
#include "domain.h"
#include "issue.h"
#include "util.h"

#include <stdio.h>

static void wf_usage(FILE *fp, const char *program_name)
{
    fprintf(fp, "usage: %s COMMAND [ARGS...]\n", program_name);
    fprintf(fp, "  wf passwd [USERNAME]\n");
    fprintf(fp, "  wf passwd USERNAME Assistant\n");
    fprintf(fp, "  wf login [USERNAME]\n");
    fprintf(fp, "  wf logout\n");
    fprintf(fp, "  wf issue create CONTENTS\n");
    fprintf(fp, "  wf issue list\n");
    fprintf(fp, "  wf issue show ISSUE_ID\n");
    fprintf(fp, "  wf issue update ISSUE_ID CONTENTS\n");
    fprintf(fp, "  wf issue delete ISSUE_ID\n");
    fprintf(fp, "  wf issue approve ISSUE_ID COMMENT\n");
    fprintf(fp, "  wf issue reject ISSUE_ID COMMENT\n");
    fprintf(fp, "  wf issue hold ISSUE_ID COMMENT\n");
    fprintf(fp, "  wf issue resume ISSUE_ID COMMENT\n");
    fprintf(fp, "  wf issue comment ISSUE_ID COMMENT\n");
    fprintf(fp, "  wf issue comments ISSUE_ID\n");
    fprintf(fp, "  wf issue search KEYWORD\n");
}

static int cmd_passwd(const struct wf_domain *domain, int argc, char **argv)
{
    const char *username = argc >= 1 ? argv[0] : "user";
    const char *role = argc >= 2 ? argv[1] : "User";
    if (argc > 2) {
        fprintf(stderr, "usage: wf passwd [USERNAME] [User|Assistant]\n");
        return 1;
    }
    return wf_auth_passwd(domain, username, role);
}

static int cmd_login(const struct wf_domain *domain, int argc, char **argv)
{
    const char *username = argc >= 1 ? argv[0] : "user";
    if (argc > 1) {
        fprintf(stderr, "usage: wf login [USERNAME]\n");
        return 1;
    }
    return wf_auth_login(domain, username);
}

static int cmd_logout(const struct wf_domain *domain, int argc, char **argv)
{
    (void)domain;
    (void)argc;
    (void)argv;
    if (argc != 0) {
        fprintf(stderr, "usage: wf logout\n");
        return 1;
    }
    return wf_auth_logout(domain);
}

static int cmd_issue(const struct wf_domain *domain, int argc, char **argv)
{
    struct wf_user user;

    if (wf_auth_current_user(domain, &user) != 0) {
        return 1;
    }
    /* Reconstruct argv view expected by wf_issue_command: argv[0]="issue", argv[1..]=subcommand+args.
     * Caller passes tail after top-level "issue", so argv-1 points back to "issue" in original argv. */
    return wf_issue_command(domain, &user, argc + 1, argv - 1);
}

static const struct wf_subcommand commands[] = {
    {"passwd", cmd_passwd},
    {"login", cmd_login},
    {"logout", cmd_logout},
    {"issue", cmd_issue},
    {NULL, NULL}
};

int main(int argc, char **argv)
{
    struct wf_domain domain;
    const struct wf_subcommand *match = NULL;
    enum wf_cmd_match_result res;

    if (wf_domain_resolve(&domain) != 0) {
        return 1;
    }

    if (argc < 2 || wf_streq(argv[1], "help") || wf_streq(argv[1], "--help") || wf_streq(argv[1], "-h")) {
        wf_usage(argc < 2 ? stderr : stdout, argv[0]);
        return argc < 2 ? 1 : 0;
    }

    res = wf_cmd_match(commands, argv[1], &match);
    if (res == WF_CMD_MATCH_EXACT || res == WF_CMD_MATCH_PREFIX) {
        /* match->fn receives tail after the matched subcommand name */
        return match->fn(&domain, argc - 2, argv + 2);
    }

    if (res == WF_CMD_MATCH_AMBIGUOUS) {
        fprintf(stderr, "ambiguous command: %s\n", argv[1]);
        wf_usage(stderr, argv[0]);
        return 1;
    }

    fprintf(stderr, "unknown command: %s\n", argv[1]);
    wf_usage(stderr, argv[0]);
    return 1;
}
