#ifndef WF_I18N_H
#define WF_I18N_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef PACKAGE
#define PACKAGE "wf"
#endif

#ifndef LOCALEDIR
#define LOCALEDIR "/usr/local/share/locale"
#endif

#include <locale.h>

#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#else
#define gettext(Msgid) (Msgid)
#define textdomain(Domainname) (Domainname)
#define bindtextdomain(Domainname, Dirname) (Dirname)
#endif

#ifndef _
#define _(Msgid) gettext(Msgid)
#endif

#ifndef N_
#define N_(Msgid) (Msgid)
#endif

void wf_i18n_init(void);

#endif