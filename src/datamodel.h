/*
 * datamodel.h - minimal TR-181 (Device:2) data model for embedded Linux
 */
#ifndef TR181_DATAMODEL_H
#define TR181_DATAMODEL_H

#include <stddef.h>

#define DM_MAX_VALUE 128
#define DM_MAX_PATH  128

typedef enum { DM_STRING, DM_UINT, DM_BOOL } dm_type_t;

/* Callback invoked for every parameter that matches a path query. */
typedef void (*dm_visit_fn)(const char *path, const char *value,
                            dm_type_t type, int writable, void *ctx);

/* Walk every parameter whose full path starts with `prefix`.
 * A prefix ending in '.' is a partial path (object), otherwise an exact
 * parameter. Returns the number of parameters visited. */
int dm_get(const char *prefix, dm_visit_fn fn, void *ctx);

/* Set a writable parameter. Returns 0 on success, -1 if unknown,
 * -2 if read-only, -3 if the value is invalid for the type. */
int dm_set(const char *path, const char *value);

const char *dm_type_name(dm_type_t t);

#endif
