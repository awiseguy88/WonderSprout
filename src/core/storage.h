#ifndef WONDERSPROUT_STORAGE_H
#define WONDERSPROUT_STORAGE_H

#include "progress.h"

/* Load progress from path. Returns 0 on success (fills *out), -1 on any
   failure (missing/corrupt/wrong version) in which case *out is set to
   defaults and the caller may still play in-memory. */
int storage_load(const char *path, Progress *out);

/* Save progress to path (atomic: write temp then rename). Returns 0 on
   success, -1 on failure (swallowed by the caller so play continues). */
int storage_save(const char *path, const Progress *p);

/* Reset progress to defaults at path. Returns 0 on success, -1 on failure. */
int storage_reset(const char *path);

#endif
