/*
 * Unit tests for the abstracted prefix matcher (wf_match_prefix).
 * This exercises the core used by both top-level subcommands and
 * issue ID / subcommand resolution.
 */

#include "util.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_basic(void)
{
    const char *names[] = {
        "passwd",
        "exec",
        "env",
        "issue",
        NULL
    };
    const char *m = NULL;
    enum wf_match_result r;

    /* exact */
    r = wf_match_prefix(names, "exec", &m);
    assert(r == WF_MATCH_EXACT);
    assert(m && strcmp(m, "exec") == 0);

    /* unique prefix */
    r = wf_match_prefix(names, "ex", &m);
    assert(r == WF_MATCH_PREFIX);
    assert(m && strcmp(m, "exec") == 0);

    r = wf_match_prefix(names, "e", &m);  /* exec + env -> ambiguous */
    assert(r == WF_MATCH_AMBIGUOUS);
    assert(m == NULL);

    /* no match */
    r = wf_match_prefix(names, "xyz", &m);
    assert(r == WF_MATCH_NONE);
    assert(m == NULL);

    /* empty input */
    r = wf_match_prefix(names, "", &m);
    assert(r == WF_MATCH_NONE);

    /* null safety */
    r = wf_match_prefix(NULL, "foo", &m);
    assert(r == WF_MATCH_NONE);
    r = wf_match_prefix(names, NULL, &m);
    assert(r == WF_MATCH_NONE);

    printf("test_basic: OK\n");
}

static void test_issue_like_ids(void)
{
    const char *ids[] = {
        "0123456789abcde0",
        "0fedcba987654321",
        "abc1111111111111",
        NULL
    };
    const char *m = NULL;
    enum wf_match_result r;

    /* exact full id */
    r = wf_match_prefix(ids, "0123456789abcde0", &m);
    assert(r == WF_MATCH_EXACT);
    assert(strcmp(m, "0123456789abcde0") == 0);

    /* unique 4-char prefix (only first id starts with 0123) */
    r = wf_match_prefix(ids, "0123", &m);
    assert(r == WF_MATCH_PREFIX);
    assert(strcmp(m, "0123456789abcde0") == 0);

    /* 2-char unique */
    r = wf_match_prefix(ids, "01", &m);
    assert(r == WF_MATCH_PREFIX);
    assert(strcmp(m, "0123456789abcde0") == 0);

    /* ambiguous on short common prefix (0... for first two) */
    r = wf_match_prefix(ids, "0", &m);
    assert(r == WF_MATCH_AMBIGUOUS);
    assert(m == NULL);

    /* 15-char unique prefix of the first */
    r = wf_match_prefix(ids, "0123456789abcde", &m);
    assert(r == WF_MATCH_PREFIX);
    assert(strcmp(m, "0123456789abcde0") == 0);

    /* different branch */
    r = wf_match_prefix(ids, "0fe", &m);
    assert(r == WF_MATCH_PREFIX);
    assert(strcmp(m, "0fedcba987654321") == 0);

    r = wf_match_prefix(ids, "abc1", &m);
    assert(r == WF_MATCH_PREFIX);
    assert(strcmp(m, "abc1111111111111") == 0);

    printf("test_issue_like_ids: OK\n");
}

static void test_edge_cases(void)
{
    const char *single[] = { "onlyone", NULL };
    const char *m = NULL;

    enum wf_match_result r = wf_match_prefix(single, "on", &m);
    assert(r == WF_MATCH_PREFIX);
    assert(strcmp(m, "onlyone") == 0);

    /* prefix that is longer than name should not match unless exact */
    r = wf_match_prefix(single, "onlyoneextra", &m);
    assert(r == WF_MATCH_NONE);

    printf("test_edge_cases: OK\n");
}

int main(void)
{
    test_basic();
    test_issue_like_ids();
    test_edge_cases();
    printf("All matcher unit tests passed.\n");
    return 0;
}
