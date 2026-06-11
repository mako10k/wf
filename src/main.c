#define _POSIX_C_SOURCE 200809L

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "auth.h"
#include "cmd.h"
#include "domain.h"
#include "i18n.h"
#include "issue.h"
#include "storage.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void wf_print_overview(FILE *fp, const char *program_name)
{
    fprintf(fp, _("usage: %s COMMAND [ARGS...]\n"), program_name);
    fprintf(fp, "\n");
    fprintf(fp, _("Entities:\n"));
    fprintf(fp, _("  issue     the primary work item (CRUD + workflow actions)\n"));
    fprintf(fp, _("  domain    isolation boundary and data root (list/create/show/delete)\n"));
    fprintf(fp, _("  user      human or assistant identity\n"));
    fprintf(fp, "\n");
    fprintf(fp, _("Commands:\n"));
    fprintf(fp, _("  wf exec [USERNAME] [-- COMMAND ...]\n"));
    fprintf(fp, _("  wf env COMMAND\n"));
    fprintf(fp, _("  wf whoami          # current wf session user\n"));
    fprintf(fp, _("  wf version\n"));
    fprintf(fp, _("  wf user COMMAND ...\n"));
    fprintf(fp, _("  wf issue COMMAND ...\n"));
    fprintf(fp, _("  wf domain COMMAND ...\n"));
    fprintf(fp, "\n");
    fprintf(fp, _("Help:\n"));
    fprintf(fp, _("  wf help [COMMAND|TOPIC]\n"));
    fprintf(fp, _("  wf COMMAND --help\n"));
    fprintf(fp, _("  wf --version\n"));
    fprintf(fp, "\n");
    fprintf(fp, _("Topics: concepts, semantics, usecases\n"));
}

static void wf_print_version(FILE *fp)
{
    fprintf(fp, "%s\n", PACKAGE_STRING);
}

static void wf_print_concepts(FILE *fp);
static void wf_print_semantics(FILE *fp);
static void wf_print_usecases(FILE *fp);
static void wf_print_exec_usage(FILE *fp);
static void wf_print_env_usage(FILE *fp);
static void wf_print_whoami_usage(FILE *fp);
static void wf_print_version_usage(FILE *fp);
static void wf_user_usage(FILE *fp);
static void wf_domain_usage(FILE *fp);

static void wf_print_exec_usage(FILE *fp)
{
    fprintf(fp, _("usage: wf exec [USERNAME] [-- COMMAND [ARGS...]]\n"));
    fprintf(fp, _("       wf exec -- COMMAND [ARGS...]\n"));
    fprintf(fp, "\n");
    fprintf(fp, _("Interactive shell mode (no COMMAND):\n"));
    fprintf(fp, _("  - prints a start banner to stderr when the shell begins\n"));
    fprintf(fp, _("  - prints a return banner to stderr after the shell exits\n"));
    fprintf(fp, _("  - exports WF_EXEC_USER and WF_EXEC_DOMAIN (domain id)\n"));
}

static void wf_print_env_usage(FILE *fp)
{
    fprintf(fp, _("usage: wf env COMMAND\n"));
    fprintf(fp, _("  wf env export [USERNAME]\n"));
    fprintf(fp, _("  wf env clear\n"));
}

static void wf_print_whoami_usage(FILE *fp)
{
    fprintf(fp, _("usage: wf whoami\n"));
    fprintf(fp, _("  Print the current authenticated wf session username.\n"));
    fprintf(fp, _("  Requires an active session from 'wf exec' or 'wf env export'.\n"));
}

static void wf_print_version_usage(FILE *fp)
{
    fprintf(fp, _("usage: wf version\n"));
    fprintf(fp, _("       wf --version\n"));
}

static void wf_user_usage(FILE *fp)
{
    fprintf(fp, _("usage: wf user COMMAND\n"));
    fprintf(fp, _("  (COMMAND may be abbreviated to a unique prefix.)\n"));
    fprintf(fp, _("  Entity: user (list + create + password management)\n"));
    fprintf(fp, _("  wf user list\n"));
    fprintf(fp, _("  wf user create USERNAME [User|Assistant]\n"));
    fprintf(fp, _("  wf user passwd USERNAME [User|Assistant]\n"));
    fprintf(fp, "\n");
    fprintf(fp, _("  create: new user only; fails if USERNAME already exists.\n"));
    fprintf(fp, _("  passwd: create or update credentials/role for USERNAME.\n"));
    fprintf(fp, _("\nSee 'wf help concepts' (actor / role section).\n"));
}

static void wf_domain_usage(FILE *fp)
{
    fprintf(fp, _("usage: wf domain COMMAND\n"));
    fprintf(fp, _("  (COMMAND may be abbreviated to a unique prefix; aliases: ls, current.)\n"));
    fprintf(fp, _("  wf domain list\n"));
    fprintf(fp, _("  wf domain create\n"));
    fprintf(fp, _("  wf domain show [id|short]\n"));
    fprintf(fp, _("  wf domain delete <id|short>\n"));
    fprintf(fp, _("  wf domain status\n"));
    fprintf(fp, _("\nSee 'wf help concepts' (domain section) and 'wf help semantics'.\n"));
}

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
    if (wf_streq(topic, "exec")) {
        wf_print_exec_usage(fp);
        return;
    }
    if (wf_streq(topic, "env")) {
        wf_print_env_usage(fp);
        return;
    }
    if (wf_streq(topic, "whoami")) {
        wf_print_whoami_usage(fp);
        return;
    }
    if (wf_streq(topic, "version")) {
        wf_print_version_usage(fp);
        return;
    }
    if (wf_streq(topic, "issue")) {
        wf_issue_usage(fp);
        return;
    }
    if (wf_streq(topic, "user")) {
        wf_user_usage(fp);
        return;
    }
    if (wf_streq(topic, "domain")) {
        wf_domain_usage(fp);
        return;
    }

    /* command-specific help is handled by the individual cmd_* functions */
    fprintf(fp, _("No detailed help for topic '%s'. Try 'wf help concepts' or 'wf help semantics'.\n"), topic);
}

static void wf_print_concepts(FILE *fp)
{
    fprintf(fp, _("Concepts:\n"));
    fprintf(fp, "\n");
    fprintf(fp, _("  domain:\n"));
    fprintf(fp, _("      An isolation boundary. The domain is derived from the current\n"));
    fprintf(fp, _("      working directory (SHA-256 of \"wf:<realpath>\"). All data\n"));
    fprintf(fp, _("      (users, sessions, issues) for that directory lives under\n"));
    fprintf(fp, _("      $XDG_DATA_HOME/wf/domains/<64hex-id>/. Different directories\n"));
    fprintf(fp, _("      normally get different domains unless they share the same path\n"));
    fprintf(fp, _("      hash. Use 'wf domain ls' to see registered domains and\n"));
    fprintf(fp, _("      'WF_DOMAIN=shortid ...' to operate on a non-current domain.\n"));
    fprintf(fp, "\n");
    fprintf(fp, _("  actor / role:\n"));
    fprintf(fp, _("      User     - human approver. Requires TTY password when\n"));
    fprintf(fp, _("                 creating an execution environment with\n"));
    fprintf(fp, _("                 'wf exec' or 'wf env export'.\n"));
    fprintf(fp, _("                 Can read issues, comment, review issues\n"));
    fprintf(fp, _("                 (approve/reject/hold/resume), invalidate a\n"));
    fprintf(fp, _("                 requested condition, or invalidate an issue.\n"));
    fprintf(fp, _("      Assistant- AI / automation. No password. Can create issues and\n"));
    fprintf(fp, _("                 update/delete its own issues, request a\n"));
    fprintf(fp, _("                 condition, clear that condition, and\n"));
    fprintf(fp, _("                 comment.\n"));
    fprintf(fp, "\n");
    fprintf(fp, _("  issue:\n"));
    fprintf(fp, _("      The central work item. Has a 16-hex ID, content, creator,\n"));
    fprintf(fp, _("      issue state, optional requested condition, recorded review\n"));
    fprintf(fp, _("      decision details, approver and comments. Assistants create\n"));
    fprintf(fp, _("      issues and manage requested conditions; Users review them.\n"));
    fprintf(fp, "\n");
    fprintf(fp, _("  abbreviation:\n"));
    fprintf(fp, _("      Both top-level COMMANDs and 'issue' subcommands support\n"));
    fprintf(fp, _("      unique prefix matching (exact match wins). ISSUE_IDs support\n"));
    fprintf(fp, _("      unique prefixes of 2 or more characters.\n"));
    fprintf(fp, _("      Ambiguous input produces a clear error and the relevant usage.\n"));
}

static void wf_print_semantics(FILE *fp)
{
    fprintf(fp, _("Semantics:\n"));
    fprintf(fp, "\n");
    fprintf(fp, _("  COMMAND\n"));
    fprintf(fp, _("      Top-level command. May be abbreviated to a unique prefix\n"));
    fprintf(fp, _("      (e.g. 'ex' for 'exec', 'i' for 'issue').\n"));
    fprintf(fp, _("      Entity subcommands under 'env', 'user', 'issue', and 'domain'\n"));
    fprintf(fp, _("      also use unique-prefix matching (e.g. 'en ex', 'us pas', 'i sh', 'dom cur').\n"));
    fprintf(fp, "\n");
    fprintf(fp, _("  ISSUE_ID\n"));
    fprintf(fp, _("      16-character hex identifier of an issue. May be given as a\n"));
    fprintf(fp, _("      unique prefix of 2 or more characters. The resolver reports\n"));
    fprintf(fp, _("      'too short', 'ambiguous issue id' or 'issue not found'.\n"));
    fprintf(fp, "\n");
    fprintf(fp, _("  DOMAIN selection\n"));
    fprintf(fp, _("      By default the domain is computed from the current working\n"));
    fprintf(fp, _("      directory. You can force another domain with the environment\n"));
    fprintf(fp, _("      variable WF_DOMAIN (full id or unique short prefix).\n"));
    fprintf(fp, _("      See also 'wf domain ls' and 'wf domain current'.\n"));
    fprintf(fp, "\n");
    fprintf(fp, _("  Review flow\n"));
    fprintf(fp, _("      Assistants may attach a requested condition before review.\n"));
    fprintf(fp, _("      Users may approve/reject/hold using that condition as-is,\n"));
    fprintf(fp, _("      supply an override condition, partially approve, invalidate\n"));
    fprintf(fp, _("      only the requested condition, or invalidate the whole issue.\n"));
}

static void wf_print_usecases(FILE *fp)
{
    fprintf(fp, _("Common usecases:\n"));
    fprintf(fp, "\n");
    fprintf(fp, _("  The reserved 'assistant' account auto-registers on first use:\n"));
    fprintf(fp, "      wf exec assistant -- wf issue create \"fix the thing\"\n");
    fprintf(fp, "\n");
    fprintf(fp, _("  Create a named Assistant explicitly:\n"));
    fprintf(fp, "      wf user create bot Assistant\n");
    fprintf(fp, "      wf exec bot -- wf issue create \"fix the thing\"\n");
    fprintf(fp, "\n");
    fprintf(fp, _("  Start an interactive User shell:\n"));
    fprintf(fp, "      wf user passwd user User\n");
    fprintf(fp, "      wf exec\n");
    fprintf(fp, "      wf issue list\n");
    fprintf(fp, "      wf issue show <id>\n");
    fprintf(fp, "      wf issue approve <id> \"looks good\"\n");
    fprintf(fp, _("      # wf exec also exports WF_EXEC_USER and WF_EXEC_DOMAIN\n"));
    fprintf(fp, "\n");
    fprintf(fp, _("  Export environment into the current shell:\n"));
    fprintf(fp, "      eval \"$(wf env export assistant)\"\n");
    fprintf(fp, "      eval \"$(wf env clear)\"\n");
    fprintf(fp, "\n");
    fprintf(fp, _("  Show the current authenticated wf session user:\n"));
    fprintf(fp, "      wf whoami\n");
    fprintf(fp, "\n");
    fprintf(fp, _("  As an Assistant, request a condition before review:\n"));
    fprintf(fp, "      wf issue request-condition <id> \"only after canary passes\"\n");
    fprintf(fp, "\n");
    fprintf(fp, _("  As a User, override the requested condition during review:\n"));
    fprintf(fp, "      wf issue approve-with <id> \"only after on-call signs off\" \"acceptable with override\"\n");
    fprintf(fp, "\n");
    fprintf(fp, _("  Work with a different directory's domain without cd:\n"));
    fprintf(fp, "      WF_DOMAIN=8bae3411 wf issue list\n");
    fprintf(fp, "\n");
    fprintf(fp, _("  See all known domains:\n"));
    fprintf(fp, "      wf domain ls\n");
}

static int cmd_exec(const struct wf_domain *domain, int argc, char **argv)
{
    if (argc == 1 && (wf_streq(argv[0], "help") || wf_streq(argv[0], "--help") || wf_streq(argv[0], "-h"))) {
        wf_print_exec_usage(stdout);
        return 0;
    }
    {
        const char *username = "user";
        int command_argc = 0;
        char **command_argv = NULL;

        if (argc >= 1 && !wf_streq(argv[0], "--")) {
            username = argv[0];
            command_argc = argc - 1;
            command_argv = argv + 1;
        } else {
            command_argc = argc;
            command_argv = argv;
        }
        if (command_argc >= 1 && !wf_streq(command_argv[0], "--")) {
            fprintf(stderr, _("child commands require '--' before COMMAND\n"));
            wf_print_exec_usage(stderr);
            return 1;
        }
        if (command_argc >= 1 && wf_streq(command_argv[0], "--")) {
            command_argc -= 1;
            command_argv += 1;
        }
        return wf_auth_exec(domain, username, command_argc, command_argv);
    }
}

static int cmd_env(const struct wf_domain *domain, int argc, char **argv)
{
    const char *sub = (argc >= 1 ? argv[0] : NULL);

    if (sub == NULL || wf_streq(sub, "help") || wf_streq(sub, "--help") || wf_streq(sub, "-h")) {
        wf_print_env_usage(stdout);
        return (sub == NULL ? 1 : 0);
    }

    {
        const char *envsubs[] = {"export", "clear", NULL};
        const char *matched = NULL;
        enum wf_match_result mres = wf_match_prefix(envsubs, sub, &matched);

        if (mres == WF_MATCH_AMBIGUOUS) {
            fprintf(stderr, _("ambiguous env command: %s. See 'wf help semantics'.\n"), sub);
            wf_print_env_usage(stderr);
            return 1;
        }
        if (mres != WF_MATCH_EXACT && mres != WF_MATCH_PREFIX) {
            fprintf(stderr, _("unknown env command: %s. See 'wf help semantics'.\n"), sub);
            return 1;
        }
        if (strcmp(matched, "export") == 0) {
            const char *username = (argc >= 2 ? argv[1] : "user");

            if (argc > 2) {
                fprintf(stderr, _("usage: wf env export [USERNAME]\n"));
                return 1;
            }
            return wf_auth_env_export(domain, username);
        }
        if (argc != 1) {
            fprintf(stderr, _("usage: wf env clear\n"));
            return 1;
        }
        return wf_auth_env_clear(domain);
    }
}

static int cmd_version(const struct wf_domain *domain, int argc, char **argv)
{
    (void)domain;
    (void)argv;

    if (argc == 1 && (wf_streq(argv[0], "help") || wf_streq(argv[0], "--help") || wf_streq(argv[0], "-h"))) {
        wf_print_version_usage(stdout);
        return 0;
    }

    if (argc != 0) {
        wf_print_version_usage(stderr);
        return 1;
    }

    wf_print_version(stdout);
    return 0;
}

static int cmd_whoami(const struct wf_domain *domain, int argc, char **argv)
{
    struct wf_user user;

    if (argc == 1 && (wf_streq(argv[0], "help") || wf_streq(argv[0], "--help") || wf_streq(argv[0], "-h"))) {
        wf_print_whoami_usage(stdout);
        return 0;
    }
    if (argc != 0) {
        wf_print_whoami_usage(stderr);
        return 1;
    }
    if (wf_auth_current_user(domain, &user) != 0) {
        return 1;
    }
    printf("%s\n", user.username);
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
        wf_user_usage(stdout);
        return (sub == NULL ? 1 : 0);
    }

    {
        const char *usersubs[] = {"list", "create", "passwd", NULL};
        const char *matched = NULL;
        enum wf_match_result mres = wf_match_prefix(usersubs, sub, &matched);

        if (mres == WF_MATCH_AMBIGUOUS) {
            fprintf(stderr, _("ambiguous user command: %s. See 'wf help semantics'.\n"), sub);
            wf_user_usage(stderr);
            return 1;
        }
        if (mres != WF_MATCH_EXACT && mres != WF_MATCH_PREFIX) {
            fprintf(stderr, _("unknown user command: %s. See 'wf help concepts'.\n"), sub);
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
                fprintf(stderr, _("usage: wf user create USERNAME [User|Assistant]\n"));
                return 1;
            }
            return wf_auth_create(domain, username, role);
        } else if (strcmp(matched, "passwd") == 0) {
            const char *username = (argc >= 2 ? argv[1] : NULL);
            const char *role = (argc >= 3 ? argv[2] : NULL);
            if (!username) {
                fprintf(stderr, _("usage: wf user passwd USERNAME [User|Assistant]\n"));
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
        wf_domain_usage(stdout);
        return (sub == NULL ? 1 : 0);
    }

    /* use the shared prefix matcher for domain subcommands (entity CRUD style) */
    {
        const char *domsubs[] = {"list", "create", "show", "delete", "status", "ls", "current", NULL};
        const char *matched = NULL;
        enum wf_match_result mres = wf_match_prefix(domsubs, sub, &matched);

        if (mres == WF_MATCH_AMBIGUOUS) {
            fprintf(stderr, _("ambiguous domain command: %s. See 'wf help semantics'.\n"), sub);
            wf_domain_usage(stderr);
            return 1;
        }
        if (mres != WF_MATCH_EXACT && mres != WF_MATCH_PREFIX) {
            fprintf(stderr, _("unknown domain command: %s. See 'wf help concepts'.\n"), sub);
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
    {"exec", cmd_exec},
    {"env", cmd_env},
    {"whoami", cmd_whoami},
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

    wf_i18n_init();

    if (argc >= 2 && wf_streq(argv[1], "--version")) {
        if (argc != 2) {
            fprintf(stderr, _("usage: wf --version\n"));
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
        fprintf(stderr, _("ambiguous command: %s. See 'wf help' or 'wf help concepts'.\n"), argv[1]);
        wf_help(stderr, NULL);
        return 1;
    }

    fprintf(stderr, _("unknown command: %s. See 'wf help' or 'wf help concepts'.\n"), argv[1]);
    wf_help(stderr, NULL);
    return 1;
}
