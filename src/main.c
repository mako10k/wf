#define _POSIX_C_SOURCE 200809L

#include "auth.h"
#include "cmd.h"
#include "domain.h"
#include "issue.h"
#include "util.h"

#include <stdio.h>

static void wf_print_overview(FILE *fp, const char *program_name)
{
    fprintf(fp, "usage: %s COMMAND [ARGS...]\n", program_name);
    fprintf(fp, "\n");
    fprintf(fp, "Groups:\n");
    fprintf(fp, "  auth                  user/assistant registration and session management\n");
    fprintf(fp, "  issue                 create, inspect and advance issues\n");
    fprintf(fp, "  domain                inspect and manage domain roots\n");
    fprintf(fp, "\n");
    fprintf(fp, "Commands:\n");
    fprintf(fp, "  wf passwd [USERNAME] [User|Assistant]\n");
    fprintf(fp, "  wf login [USERNAME]\n");
    fprintf(fp, "  wf logout\n");
    fprintf(fp, "  wf issue COMMAND ...\n");
    fprintf(fp, "  wf domain COMMAND ...\n");
    fprintf(fp, "\n");
    fprintf(fp, "Help:\n");
    fprintf(fp, "  wf help [COMMAND|TOPIC]\n");
    fprintf(fp, "  wf COMMAND --help\n");
    fprintf(fp, "\n");
    fprintf(fp, "Topics: concepts, semantics, usecases\n");
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
            "  wf domain create\n"
            "  wf domain delete <id|short>\n"
            "  wf domain ls\n"
            "  wf domain current\n"
            "  wf domain status\n");
        fprintf(stdout, "\nSee 'wf help concepts' (domain section) and 'wf help semantics'.\n");
        return (sub == NULL ? 1 : 0);
    }

    /* use the shared prefix matcher for domain subcommands */
    {
        const char *domsubs[] = {"create", "delete", "ls", "current", "status", NULL};
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

        if (strcmp(matched, "create") == 0) {
            struct wf_domain tmp;
            return wf_domain_create(&tmp);
        } else if (strcmp(matched, "delete") == 0) {
            const char *target = (argc >= 2 ? argv[1] : domain->id);
            return wf_domain_delete(target);
        } else if (strcmp(matched, "ls") == 0) {
            return wf_domain_list(stdout);
        } else if (strcmp(matched, "current") == 0 || strcmp(matched, "status") == 0) {
            return wf_domain_current(domain, stdout);
        }
    }
    return 1;
}

static const struct wf_subcommand commands[] = {
    {"passwd", cmd_passwd},
    {"login", cmd_login},
    {"logout", cmd_logout},
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

    if (forced_domain && forced_domain[0]) {
        if (wf_domain_resolve_by_id(&domain, forced_domain) != 0) {
            return 1;
        }
    } else {
        if (wf_domain_resolve(&domain) != 0) {
            return 1;
        }
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
