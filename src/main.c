/*
 * tr181ctl - ubus-cli style front-end for the mini TR-181 data model
 *
 *   tr181ctl get  <path>            Device.DeviceInfo. / Device.Ethernet.Interface.1.Status
 *   tr181ctl set  <path> <value>    writable params only (Device.ManagementServer.)
 *   tr181ctl json <path>            USP (TR-369) style GetResp as JSON
 *   tr181ctl schema                 list every parameter with type and access
 *   tr181ctl shell                  interactive prompt
 */
#include "datamodel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION "1.0.0"
#define DEFAULT_STATE "/tmp/tr181-agent.state"
#define WRITABLE_ROOT "Device.ManagementServer."

static const char *state_file(void)
{
    const char *p = getenv("TR181_STATE");
    return p && *p ? p : DEFAULT_STATE;
}

/* ---------- persistence of writable params ---------- */

static void load_state(void)
{
    FILE *f = fopen(state_file(), "r");
    char line[256];

    if (!f)
        return;
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq = '\0';
        eq[1 + strcspn(eq + 1, "\n")] = '\0';
        dm_set(line, eq + 1);
    }
    fclose(f);
}

static void save_visit(const char *path, const char *value, dm_type_t t, int w, void *ctx)
{
    (void)t;
    if (w)
        fprintf((FILE *)ctx, "%s=%s\n", path, value);
}

static int save_state(void)
{
    FILE *f = fopen(state_file(), "w");
    if (!f)
        return -1;
    dm_get(WRITABLE_ROOT, save_visit, f);
    fclose(f);
    return 0;
}

/* ---------- output formatters ---------- */

static void print_plain(const char *path, const char *value, dm_type_t t, int w, void *ctx)
{
    (void)t; (void)w; (void)ctx;
    printf("%s=\"%s\"\n", path, value);
}

static void print_schema(const char *path, const char *value, dm_type_t t, int w, void *ctx)
{
    (void)value; (void)ctx;
    printf("%-50s %-12s %s\n", path, dm_type_name(t), w ? "readWrite" : "readOnly");
}

static void json_str(const char *s)
{
    putchar('"');
    for (; *s; s++) {
        if (*s == '"' || *s == '\\')
            putchar('\\');
        if ((unsigned char)*s < 0x20)
            printf("\\u%04x", *s);
        else
            putchar(*s);
    }
    putchar('"');
}

struct json_ctx { int first; };

static void print_json(const char *path, const char *value, dm_type_t t, int w, void *vctx)
{
    struct json_ctx *c = vctx;
    (void)w;
    printf("%s\n        ", c->first ? "" : ",");
    c->first = 0;
    json_str(path);
    printf(": ");
    if (t == DM_STRING)
        json_str(value);
    else
        printf("%s", value);
}

static void count_visit(const char *p, const char *v, dm_type_t t, int w, void *c)
{
    (void)p; (void)v; (void)t; (void)w; (void)c;
}

/* Shape loosely follows a USP GetResp: requested_path_results[].resolved_path_results */
static int cmd_json(const char *path)
{
    struct json_ctx c = { 1 };
    int n = dm_get(path, count_visit, NULL);

    /* 7026 = "Invalid Path" in the USP error code table */
    printf("{\n  \"get_resp\": {\n    \"requested_path\": ");
    json_str(path);
    printf(",\n    \"err_code\": %d,\n    \"result_params\": {", n > 0 ? 0 : 7026);
    dm_get(path, print_json, &c);
    printf("\n    }\n  }\n}\n");
    return n > 0 ? 0 : 1;
}

/* ---------- commands ---------- */

static int cmd_get(const char *path)
{
    if (dm_get(path, print_plain, NULL) == 0) {
        fprintf(stderr, "error: %s not found\n", path);
        return 1;
    }
    return 0;
}

static int cmd_set(const char *path, const char *value)
{
    switch (dm_set(path, value)) {
    case 0:
        if (save_state() != 0)
            fprintf(stderr, "warning: could not persist to %s\n", state_file());
        printf("%s=\"%s\"\n", path, value);
        return 0;
    case -2:
        fprintf(stderr, "error: %s is read-only\n", path);
        break;
    case -3:
        fprintf(stderr, "error: invalid value '%s' for %s\n", value, path);
        break;
    default:
        fprintf(stderr, "error: %s not found\n", path);
    }
    return 1;
}

static int dispatch(int argc, char **argv);

static int cmd_shell(void)
{
    char line[256];

    printf("tr181ctl %s - type 'help' or 'quit'\n", VERSION);
    for (;;) {
        char *argv[4] = { 0 };
        int argc = 0;

        printf("tr181> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin))
            break;
        for (char *tok = strtok(line, " \t\n"); tok && argc < 4; tok = strtok(NULL, " \t\n"))
            argv[argc++] = tok;
        if (argc == 0)
            continue;
        if (!strcmp(argv[0], "quit") || !strcmp(argv[0], "exit"))
            break;
        dispatch(argc, argv);
    }
    return 0;
}

static void usage(void)
{
    puts("usage: tr181ctl get <path>\n"
         "       tr181ctl set <path> <value>\n"
         "       tr181ctl json <path>\n"
         "       tr181ctl schema\n"
         "       tr181ctl shell\n"
         "paths ending in '.' return the whole object, e.g. Device.DeviceInfo.");
}

static int dispatch(int argc, char **argv)
{
    if (argc >= 2 && !strcmp(argv[0], "get"))
        return cmd_get(argv[1]);
    if (argc >= 3 && !strcmp(argv[0], "set"))
        return cmd_set(argv[1], argv[2]);
    if (argc >= 1 && !strcmp(argv[0], "json"))
        return cmd_json(argc >= 2 ? argv[1] : "Device.");
    if (argc >= 1 && !strcmp(argv[0], "schema"))
        return dm_get("Device.", print_schema, NULL) > 0 ? 0 : 1;
    if (argc >= 1 && !strcmp(argv[0], "version")) {
        puts(VERSION);
        return 0;
    }
    usage();
    return 2;
}

int main(int argc, char **argv)
{
    load_state();
    if (argc >= 2 && !strcmp(argv[1], "shell"))
        return cmd_shell();
    return dispatch(argc - 1, argv + 1);
}
