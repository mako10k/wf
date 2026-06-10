#define _POSIX_C_SOURCE 200809L

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "auth.h"
#include "cmd.h"
#include "domain.h"
#include "issue.h"
#include "storage.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void wf_print_overview(FILE *fp, const char *program_name)
{
    fprintf(fp, "usage: %s COMMAND [ARGS...]\n", program_name);
    fprintf(fp, "\n");
    fprintf(fp, "Entities:\n");
    fprintf(fp, "  issue     the primary work item (CRUD + workflow actions)\n");
    fprintf(fp, "  domain    isolation boundary and data root (list/create/show/delete)\n");
    fprintf(fp, "  user      human or assistant identity\n");
    fprintf(fp, "\n");
    fprintf(fp, "Commands:\n");
    fprintf(fp, "  wf login [USERNAME]\n");
    fprintf(fp, "  wf logout\n");
    fprintf(fp, "  wf version\n");
    fprintf(fp, "  wf user COMMAND ...\n");
    fprintf(fp, "  wf issue COMMAND ...\n");
    fprintf(fp, "  wf domain COMMAND ...\n");
    fprintf(fp, "\n");
    fprintf(fp, "Help:\n");
    fprintf(fp, "  wf help [COMMAND|TOPIC]\n");
    fprintf(fp, "  wf COMMAND --help\n");
    fprintf(fp, "  wf --version\n");
    fprintf(fp, "\n");
    fprintf(fp, "Topics: concepts, semantics, usecases\n");
}

static void wf_print_version(FILE *fp)
{
    fprintf(fp, "%s\n", PACKAGE_STRING);
}

static void wf_print_concepts(FILE *fp);
static void wf_print_semantics(FILE *fp);
static void wf_print_usecases(FILE *fp);

static void wf_help(FILE *fp, const char *topic)
{
    if (topic == NULL || wf_streq(topic, "help") || wf_streq(topic, "--help") ||
        wf_streq(topic, "-h") || wf_streq(topic, "overview")) {
        wf_print_overview(fp, "wf");
        return;
    }

    if (wf_streq(topic, "concepts")) {
        wf_print_concepts(fp);
        return;
    }
    if (wf_streq(topic, "semantics")) {
        wf_print_semantics(fp);
        return;
    }
    if (wf_streq(topic, "usecases") || wf_streq(topic, "examples")) {
        wf_print_usecases(fp);
        return;
    }

    /* command-specific help is handled by the individual cmd_* functions */
    fprintf(fp, "No detailed help for topic '%s'. Try 'wf help concepts' or 'wf help semantics'.\n", topic);
}

static void wf_print_concepts(FILE *fp)
{
    fprintf(fp, "Concepts:\n");
    fprintf(fp, "\n");
    fprintf(fp, "  domain:\n");
    fprintf(fp, "      An isolation boundary. The domain is derived from the current\n");
    fprintf(fp, "      working directory (SHA-256 of \"wf:<realpath>\"). All data\n");
    fprintf(fp, "      (users, sessions, issues) for that directory lives under\n");
    fprintf(fp, "      $XDG_DATA_HOME/wf/domains/<64hex-id>/. Different directories\n");
    fprintf(fp, "      normally get different domains unless they share the same path\n");
    fprintf(fp, "      hash. Use 'wf domain ls' to see registered domains and\n");
    fprintf(fp, "      'WF_DOMAIN=shortid ...' to operate on a non-current domain.\n");
    fprintf(fp, "\n");
    fprintf(fp, "  actor / role:\n");
    fprintf(fp, "      User     - human approver. Requires TTY password on login.\n");
    fprintf(fp, "                 Can read issues, comment, and change status\n");
    fprintf(fp, "                 (approve/reject/hold/resume).\n");
    fprintf(fp, "      Assistant- AI / automation. No password. Can create issues and\n");
    fprintf(fp, "                 update/delete its own issues. Can also comment.\n");
    fprintf(fp, "\n");
    fprintf(fp, "  issue:\n");
    fprintf(fp, "      The central work item. Has a 16-hex ID, content, creator,\n");
    fprintf(fp, "      status, approver and comments. Status lifecycle is managed\n");
    fprintf(fp, "      by Users; creation and self-updates are done by Assistants.\n");
    fprintf(fp, "\n");
    fprintf(fp, "  abbreviation:\n");
    fprintf(fp, "      Both top-level COMMANDs and 'issue' subcommands support\n");
    fprintf(fp, "      unique prefix matching (exact match wins). ISSUE_IDs support\n");
    fprintf(fp, "      unique prefixes of 2 or more characters.\n");
    fprintf(fp, "      Ambiguous input produces a clear error and the relevant usage.\n");
}

static void wf_print_semantics(FILE *fp)
{
    fprintf(fp, "Semantics:\n");
    fprintf(fp, "\n");
    fprintf(fp, "  COMMAND\n");
    fprintf(fp, "      Top-level command. May be abbreviated to a unique prefix\n");
    fprintf(fp, "      (e.g. 'logi' for 'login', 'i' for 'issue').\n");
    fprintf(fp, "\n");
    fprintf(fp, "  ISSUE_ID\n");
    fprintf(fp, "      16-character hex identifier of an issue. May be given as a\n");
    fprintf(fp, "      unique prefix of 2 or more characters. The resolver reports\n");
    fprintf(fp, "      'too short', 'ambiguous issue id' or 'issue not found'.\n");
    fprintf(fp, "\n");
    fprintf(fp, "  DOMAIN selection\n");
    fprintf(fp, "      By default the domain is computed from the current working\n");
    fprintf(fp, "      directory. You can force another domain with the environment\n");
    fprintf(fp, "      variable WF_DOMAIN (full id or unique short prefix).\n");
    fprintf(fp, "      See also 'wf domain ls' and 'wf domain current'.\n");
}

static void wf_print_usecases(FILE *fp)
{
    fprintf(fp, "Common usecases:\n");
    fprintf(fp, "\n");
    fprintf(fp, "  Register an Assistant (no password):\n");
    fprintf(fp, "      wf passwd assistant Assistant\n");
    fprintf(fp, "\n");
    fprintf(fp, "  Login as Assistant and create an issue:\n");
    fprintf(fp, "      eval \"$(wf login assistant)\"\n");
    fprintf(fp, "      wf issue create \"fix the thing\"\n");
    fprintf(fp, "\n");
    fprintf(fp, "  As a User, review and approve:\n");
    fprintf(fp, "      eval \"$(wf login)\"\n");
    fprintf(fp, "      wf issue list\n");
    fprintf(fp, "      wf issue show <id>\n");
    fprintf(fp, "      wf issue approve <id> \"looks good\"\n");
    fprintf(fp, "\n");
    fprintf(fp, "  Work with a different directory's domain without cd:\n");
    fprintf(fp, "      WF_DOMAIN=8bae3411 wf issue list\n");
    fprintf(fp, "\n");
    fprintf(fp, "  See all known domains:\n");
    fprintf(fp, "      wf domain ls\n");
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

static int cmd_version(const struct wf_domain *domain, int argc, char **argv)
{
    (void)domain;
    (void)argv;

    if (argc != 0) {
        fprintf(stderr, "usage: wf version\n");
        return 1;
    }

    wf_print_version(stdout);
    return 0;
}

static int cmd_issue(const struct wf_domain *domain, int argc, char **argv)
{
    struct wf_user user;

    /* Forward declaration so the call below has a prototype even if
     * issue.h hasn't been processed for some reason (defensive). */
    void wf_issue_usage(FILE *fp);

    /* Allow showing issue subcommand help without being logged in. */
    if (argc >= 1 && (wf_streq(argv[0], "help") || wf_streq(argv[0], "--help") || wf_streq(argv[0], "-h"))) {
        if (argc >= 2) {
            const char *t = argv[1];
            if (wf_streq(t, "concepts") || wf_streq(t, "semantics") || wf_streq(t, "usecases")) {
                wf_help(stdout, t);
                return 0;
            }
        }
        wf_issue_usage(stdout);
        return 0;
    }

    if (wf_auth_current_user(domain, &user) != 0) {
        return 1;
    }
    /* Reconstruct argv view expected by wf_issue_command: argv[0]="issue", argv[1..]=subcommand+args.
     * Caller passes tail after top-level "issue", so argv-1 points back to "issue" in original argv. */
    return wf_issue_command(domain, &user, argc + 1, argv - 1);
}

static int cmd_user(const struct wf_domain *domain, int argc, char **argv)
{
    const char *sub = (argc >= 1 ? argv[0] : NULL);

    if (sub == NULL || wf_streq(sub, "help") || wf_streq(sub, "--help") || wf_streq(sub, "-h")) {
        fprintf(stdout,
            "usage: wf user COMMAND\n"
            "  Entity: user (list + create + password management)\n"
            "  wf user list\n"
            "  wf user create USERNAME [User|Assistant]\n"
            "  wf user passwd USERNAME [User|Assistant]\n");
        fprintf(stdout, "\nSee 'wf help concepts' (actor / role section).\n");
        return (sub == NULL ? 1 : 0);
    }

    {
        const char *usersubs[] = {"list", "create", "passwd", NULL};
        const char *matched = NULL;
        enum wf_match_result mres = wf_match_prefix(usersubs, sub, &matched);

        if (mres == WF_MATCH_AMBIGUOUS) {
            fprintf(stderr, "ambiguous user command: %s. See 'wf help semantics'.\n", sub);
            return 1;
        }
        if (mres != WF_MATCH_EXACT && mres != WF_MATCH_PREFIX) {
            fprintf(stderr, "unknown user command: %s. See 'wf help concepts'.\n", sub);
            return 1;
        }

        if (strcmp(matched, "list") == 0) {
            /* simple entity list: read the users file of the domain */
            char path[PATH_MAX];
            FILE *f;
            char line[1024];
            int written = snprintf(path, sizeof(path), "%s/users.tsv", domain->root);
            if (written < 0 || (size_t)written >= sizeof(path)) return 1;
            f = fopen(path, "r");
            if (f == NULL) {
                perror(path);
                return 1;
            }
            while (fgets(line, sizeof(line), f)) {
                char *fields[4];
                wf_trim_newline(line);
                if (wf_split_tsv(line, fields, 4) >= 2) {
                    printf("%s\t%s\n", fields[0], fields[1]);
                }
            }
            fclose(f);
            return 0;
        } else if (strcmp(matched, "create") == 0) {
            const char *username = (argc >= 2 ? argv[1] : NULL);
            const char *role = (argc >= 3 ? argv[2] : "User");
            if (!username) {
                fprintf(stderr, "usage: wf user create USERNAME [User|Assistant]\n");
                return 1;
            }
            return wf_auth_passwd(domain, username, role);
        } else if (strcmp(matched, "passwd") == 0) {
            const char *username = (argc >= 2 ? argv[1] : NULL);
            const char *role = (argc >= 3 ? argv[2] : NULL);
            if (!username) {
                fprintf(stderr, "usage: wf user passwd USERNAME [User|Assistant]\n");
                return 1;
            }
            return wf_auth_passwd(domain, username, role ? role : "User");
        }
    }
    return 1;
}

static int cmd_domain(const struct wf_domain *domain, int argc, char **argv)
{
    const char *sub = (argc >= 1 ? argv[0] : NULL);

    if (sub == NULL || wf_streq(sub, "help") || wf_streq(sub, "--help") || wf_streq(sub, "-h")) {
        if (argc >= 2) {
            const char *topic = argv[1];
            if (wf_streq(topic, "concepts") || wf_streq(topic, "semantics") || wf_streq(topic, "usecases")) {
                wf_help(stdout, topic);
                return 0;
            }
        }
        fprintf(stdout,
            "usage: wf domain COMMAND\n"
            "  wf domain list\n"
            "  wf domain create\n"
            "  wf domain show [id|short]\n"
            "  wf domain delete <id|short>\n"
            "  wf domain status\n");
        fprintf(stdout, "\nSee 'wf help concepts' (domain section) and 'wf help semantics'.\n");
        return (sub == NULL ? 1 : 0);
    }

    /* use the shared prefix matcher for domain subcommands (entity CRUD style) */
    {
        const char *domsubs[] = {"list", "create", "show", "delete", "status", "ls", "current", NULL};
        const char *matched = NULL;
        enum wf_match_result mres = wf_match_prefix(domsubs, sub, &matched);

        if (mres == WF_MATCH_AMBIGUOUS) {
            fprintf(stderr, "ambiguous domain command: %s. See 'wf help semantics'.\n", sub);
            return 1;
        }
        if (mres != WF_MATCH_EXACT && mres != WF_MATCH_PREFIX) {
            fprintf(stderr, "unknown domain command: %s. See 'wf help concepts'.\n", sub);
            return 1;
        }

        /* normalize aliases for CRUD consistency */
        const char *norm = matched;
        if (strcmp(matched, "ls") == 0) norm = "list";
        if (strcmp(matched, "current") == 0) norm = "show";

        if (strcmp(norm, "create") == 0) {
            struct wf_domain tmp;
            return wf_domain_create(&tmp);
        } else if (strcmp(norm, "delete") == 0) {
            const char *target = (argc >= 2 ? argv[1] : domain->id);
            return wf_domain_delete(target);
        } else if (strcmp(norm, "list") == 0) {
            return wf_domain_list(stdout);
        } else if (strcmp(norm, "show") == 0 || strcmp(norm, "status") == 0) {
            /* show without arg = current; with arg = specific domain */
            if (argc >= 2 && !wf_streq(argv[1], "show") && !wf_streq(argv[1], "status")) {
                /* user gave an id after show/status */
                struct wf_domain target;
                if (wf_domain_resolve_by_id(&target, argv[1]) != 0) return 1;
                return wf_domain_current(&target, stdout);
            }
            return wf_domain_current(domain, stdout);
        }
    }
    return 1;
}

static const struct wf_subcommand commands[] = {
    {"login", cmd_login},
    {"logout", cmd_logout},
    {"version", cmd_version},
    {"user", cmd_user},
    {"issue", cmd_issue},
    {"domain", cmd_domain},
    {NULL, NULL}
};

int main(int argc, char **argv)
{
    struct wf_domain domain;
    const struct wf_subcommand *match = NULL;
    enum wf_match_result res;
    const char *forced_domain = getenv("WF_DOMAIN");

    if (argc >= 2 && wf_streq(argv[1], "--version")) {
        if (argc != 2) {
            fprintf(stderr, "usage: wf --version\n");
            return 1;
        }
        wf_print_version(stdout);
        return 0;
    }

    if (argc < 2 || wf_streq(argv[1], "help") || wf_streq(argv[1], "--help") || wf_streq(argv[1], "-h")) {
        if (argc >= 3) {
            /* wf help <topic> */
            wf_help((argc < 2 ? stderr : stdout), argv[2]);
        } else {
            wf_help((argc < 2 ? stderr : stdout), NULL);
        }
        return argc < 2 ? 1 : 0;
    }

    if (forced_domain && forced_domain[0]) {
        if (wf_domain_resolve_by_id(&domain, forced_domain) != 0) {
            return 1;
        }
    } else {
        if (wf_domain_resolve(&domain) != 0) {
            return 1;
        }
    }

    res = wf_cmd_match(commands, argv[1], &match);
    if (res == WF_MATCH_EXACT || res == WF_MATCH_PREFIX) {
        /* match->fn receives tail after the matched subcommand name */
        return match->fn(&domain, argc - 2, argv + 2);
    }

    if (res == WF_MATCH_AMBIGUOUS) {
        fprintf(stderr, "ambiguous command: %s. See 'wf help' or 'wf help concepts'.\n", argv[1]);
        wf_help(stderr, NULL);
        return 1;
    }

    fprintf(stderr, "unknown command: %s. See 'wf help' or 'wf help concepts'.\n", argv[1]);
    wf_help(stderr, NULL);
    return 1;
}
