#ifndef _PYGMY_SYSLOGGING_H_
#define _PYGMY_SYSLOGGING_H_

#include <nuttx/config.h>
#include <syslog.h>

#define __HLOGSTR(fstring) "%s::" fstring

/* Debug output */

#ifdef CONFIG_PYGMY_SYSLOG_DEBUG
#define pydebug(fstring, ...)                                                \
  syslog(LOG_DEBUG | LOG_USER, __HLOGSTR(fstring),                           \
         __FUNCTION__ __VA_OPT__(, ) __VA_ARGS__)
#else
#define pydebug(fstring, ...)
#endif

/* Info output */

#ifdef CONFIG_PYGMY_SYSLOG_INFO
#define pyinfo(fstring, ...)                                                 \
  syslog(LOG_INFO | LOG_USER, __HLOGSTR(fstring),                            \
         __FUNCTION__ __VA_OPT__(, ) __VA_ARGS__)
#else
#define pyinfo(fstring, ...)
#endif

/* Warning output */

#ifdef CONFIG_PYGMY_SYSLOG_WARN
#define pywarn(fstring, ...)                                                 \
  syslog(LOG_WARNING | LOG_USER, __HLOGSTR(fstring),                         \
         __FUNCTION__ __VA_OPT__(, ) __VA_ARGS__)
#else
#define pywarn(fstring, ...)
#endif

/* Error output */

#ifdef CONFIG_PYGMY_SYSLOG_ERR
#define pyerr(fstring, ...)                                                  \
  syslog(LOG_ERR | LOG_USER, __HLOGSTR(fstring),                             \
         __FUNCTION__ __VA_OPT__(, ) __VA_ARGS__)
#else
#define pyerr(fstring, ...)
#endif

#endif // _PYGMY_SYSLOGGING_H_
