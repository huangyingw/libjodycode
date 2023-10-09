/* libjodycode: path manipulation
 *
 * Copyright (C) 2014-2023 by Jody Bruchon <jody@jodybruchon.com>
 * Released under The MIT License
 */

#include <stdio.h>
#include <string.h>
#include "likely_unlikely.h"
#include "libjodycode.h"

/* Collapse dot-dot and single dot path components
 * This code MUST be passed a full file pathname (starting with '/') */
extern int jc_collapse_dotdot(char * const path)
{
  char *p;   /* string copy input */
  char *out; /* string copy output */
  unsigned int i = 0;

  /* Fail if not passed an absolute path */
  if (unlikely(*path != '/')) return JC_ENULL;

  p = path; out = path;

  while (*p != '\0') {
    /* Abort if we're too close to the end of the buffer */
    if (unlikely(i >= (JC_PATHBUF_SIZE - 3))) return JC_ECDOTDOT;

    /* Skip repeated slashes */
    while (*p == '/' && *(p + 1) == '/') {
      p++; i++;
    }

    /* Scan for '/./', '/..', '/.\0' combinations */
    if (*p == '/' && *(p + 1) == '.'
        && (*(p + 2) == '.' || *(p + 2) == '/' || *(p + 2) == '\0')) {
      /* Check for '../' or terminal '..' */
      if (*(p + 2) == '.' && (*(p + 3) == '/' || *(p + 3) == '\0')) {
        /* Found a dot-dot; pull everything back to the previous directory */
        p += 3; i += 3;
        /* If already at root, skip over the dot-dot */
        if (i == 0) continue;
        /* Don't seek back past the first character */
	if ((uintptr_t)out == (uintptr_t)path) continue;
	out--;
        while (*out != '/') out--;
	if (*p == '\0') break;
        continue;
      } else if (*(p + 2) == '/' || *(p + 2) == '\0') {
        /* Found a single dot; seek input ptr past it */
        p += 2; i += 2;
	if (*p == '\0') break;
        continue;
      }
      /* Fall through: not a dot or dot-dot, just a slash */
    }

    /* Copy all remaining text */
    *out = *p;
    p++; out++; i++;
  }

  /* If only a root slash remains, be sure to keep it */
  if ((uintptr_t)out == (uintptr_t)path) {
    *out = '/';
    out++;
  }

  /* Output must always be terminated properly */
  *out = '\0';

  return 0;
}


/* Create a relative symbolic link path for a destination file */
extern int jc_make_relative_link_name(const char * const src,
                const char * const dest, char * rel_path)
{
  static char p1[JC_PATHBUF_SIZE * 2], p2[JC_PATHBUF_SIZE * 2];
  char *sp, *dp, *ss;

  if (unlikely(!src || !dest)) return JC_ENULL;

  /* Get working directory path and prefix to pathnames if needed */
  if (*src != '/' || *dest != '/') {
    if (!jc_getcwd(p1, JC_PATHBUF_SIZE * 2)) return JC_EGETCWD;
    *(p1 + (JC_PATHBUF_SIZE * 2) - 1) = '\0';
#ifdef ON_WINDOWS
    strncat_s(p1, JC_PATHBUF_SIZE * 2, "/", JC_PATHBUF_SIZE * 2 - 1);
    strncpy_s(p2, JC_PATHBUF_SIZE * 2, p1, JC_PATHBUF_SIZE * 2);
#else
    strncat(p1, "/", JC_PATHBUF_SIZE * 2 - 1);
    strncpy(p2, p1, JC_PATHBUF_SIZE * 2);
#endif
  }

  /* If an absolute path is provided, use it as-is */
  if (*src == '/') *p1 = '\0';
  if (*dest == '/') *p2 = '\0';

  /* Concatenate working directory to relative paths */
#ifdef ON_WINDOWS
  strncat_s(p1, JC_PATHBUF_SIZE * 2, src, JC_PATHBUF_SIZE);
  strncat_s(p2, JC_PATHBUF_SIZE * 2, dest, JC_PATHBUF_SIZE);
#else
  strncat(p1, src, JC_PATHBUF_SIZE);
  strncat(p2, dest, JC_PATHBUF_SIZE);
#endif

  /* Collapse . and .. path components */
  if (unlikely(jc_collapse_dotdot(p1) != 0)) return JC_ECDOTDOT;
  if (unlikely(jc_collapse_dotdot(p2) != 0)) return JC_ECDOTDOT;

  /* Find where paths differ, remembering each slash along the way */
  sp = p1; dp = p2; ss = p1;
  while (*sp == *dp && *sp != '\0' && *dp != '\0') {
    if (*sp == '/') ss = sp;
    sp++; dp++;
  }
  /* If paths are 100% identical then the files are the same file */
  if (*sp == '\0' && *dp == '\0') return 1;

  /* Replace dirs in destination path with dot-dot */
  while (*dp != '\0') {
    if (*dp == '/') {
      *rel_path++ = '.'; *rel_path++ = '.'; *rel_path++ = '/';
    }
    dp++;
  }

  /* Copy the file name into rel_path and return */
  ss++;
  while (unlikely(*ss != '\0')) *rel_path++ = *ss++;

  /* . and .. dirs at end are invalid */
  if (*(rel_path - 1) == '.')
    if (*(rel_path - 2) == '/' ||
        (*(rel_path - 2) == '.' && *(rel_path - 3) == '/'))
      return JC_EGRNEND;
  if (unlikely(*(rel_path - 1) == '/')) return JC_EGRNEND;

  *rel_path = '\0';
  return 0;
}
