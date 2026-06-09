#define _POSIX_C_SOURCE 200809L

#include "auth.h"
#include "domain.h"
#include "issue.h"
#include "util.h"

#include <stdio.h>

static void wf_usage(void)
{
    puts("usage:");
    puts("  wf passwd [USERNAME]");
    puts("  wf passwd USERNAME Assistant");
    puts("  wf login [USERNAME]");
    puts("  wf logout");
    puts("  wf issue create CONTENTS");
    puts("  wf issue list");
    puts("  wf issue show ISSUE_ID");
    puts("  wf issue update ISSUE_ID CONTENTS");
    puts("  wf issue delete ISSUE_ID");
    puts("  wf issue approve ISSUE_ID COMMENT");
    puts("  wf issue reject ISSUE_ID COMMENT");
    puts("  wf issue hold ISSUE_ID COMMENT");
    puts("  wf issue resume ISSUE_ID COMMENT");
    puts("  wf issue comment ISSUE_ID COMMENT");
    puts("  wf issue comments ISSUE_ID");
    puts("  wf issue search KEYWORD");
}

int main(int argc, char **argv)
{
    struct wf_domain domain;
    struct wf_user user;

    if (wf_domain_resolve(&domain) != 0) {
        return 1;
    }

    if (argc < 2 || wf_streq(argv[1], "help") || wf_streq(argv[1], "--help") || wf_streq(argv[1], "-h")) {
        wf_usage();
        return argc < 2 ? 1 : 0;
    }

    if (wf_streq(argv[1], "passwd")) {
        const char *username = argc >= 3 ? argv[2] : "user";
        const char *role = argc >= 4 ? argv[3] : "User";
        if (argc > 4) {
            fprintf(stderr, "usage: wf passwd [USERNAME] [User|Assistant]\n");
            return 1;
        }
        return wf_auth_passwd(&domain, username, role);
    }

    if (wf_streq(argv[1], "login")) {
        const char *username = argc >= 3 ? argv[2] : "user";
        if (argc > 3) {
            fprintf(stderr, "usage: wf login [USERNAME]\n");
            return 1;
        }
        return wf_auth_login(&domain, username);
    }

    if (wf_streq(argv[1], "logout")) {
        if (argc != 2) {
            fprintf(stderr, "usage: wf logout\n");
            return 1;
        }
        return wf_auth_logout(&domain);
    }

    if (wf_streq(argv[1], "issue")) {
        if (wf_auth_current_user(&domain, &user) != 0) {
            return 1;
        }
        return wf_issue_command(&domain, &user, argc - 1, argv + 1);
    }

    fprintf(stderr, "unknown command: %s\n", argv[1]);
    wf_usage();
    return 1;
}
