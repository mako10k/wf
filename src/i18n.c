#include "i18n.h"

#include <stdlib.h>

void wf_i18n_init(void)
{
    const char *locale_dir;

    setlocale(LC_ALL, "");

    locale_dir = getenv("WF_LOCALEDIR");
    if (locale_dir == NULL || locale_dir[0] == '\0') {
        locale_dir = LOCALEDIR;
    }

    (void)bindtextdomain(PACKAGE, locale_dir);
    (void)textdomain(PACKAGE);
}