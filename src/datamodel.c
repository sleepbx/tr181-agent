/*
 * datamodel.c - TR-181 parameters backed by /proc and /sys
 *
 * Implemented objects:
 *   Device.DeviceInfo.
 *   Device.Ethernet.Interface.{i}.      (one instance per /sys/class/net entry)
 *   Device.Ethernet.Interface.{i}.Stats.
 *   Device.ManagementServer.            (writable, kept in memory)
 */
#include "datamodel.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

#define SYS_NET "/sys/class/net"
#define MAX_IFACES 16

/* ---------- helpers ---------- */

static int read_line(const char *file, char *buf, size_t len)
{
    FILE *f = fopen(file, "r");
    if (!f)
        return -1;
    if (!fgets(buf, (int)len, f)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    buf[strcspn(buf, "\n")] = '\0';
    return 0;
}

static void read_or(const char *file, char *buf, size_t len, const char *fallback)
{
    if (read_line(file, buf, len) != 0)
        snprintf(buf, len, "%s", fallback);
}

static int path_matches(const char *prefix, const char *path)
{
    size_t n = strlen(prefix);
    if (n == 0)
        return 1;
    if (prefix[n - 1] == '.')
        return strncmp(prefix, path, n) == 0;
    return strcmp(prefix, path) == 0;
}

/* ---------- Device.DeviceInfo. ---------- */

typedef void (*getter_fn)(char *buf, size_t len);

static void get_manufacturer(char *b, size_t l) { snprintf(b, l, "OpenSource CPE"); }
static void get_model(char *b, size_t l)        { read_or("/proc/device-tree/model", b, l, "Generic-Linux-CPE"); }

static void get_sw_version(char *b, size_t l)
{
    struct utsname u;
    if (uname(&u) == 0)
        snprintf(b, l, "%s", u.release);
    else
        snprintf(b, l, "unknown");
}

static void get_hostname(char *b, size_t l) { read_or("/proc/sys/kernel/hostname", b, l, "cpe"); }

static void get_uptime(char *b, size_t l)
{
    char tmp[64];
    read_or("/proc/uptime", tmp, sizeof(tmp), "0");
    snprintf(b, l, "%lu", strtoul(tmp, NULL, 10));
}

static void get_mem_total(char *b, size_t l)
{
    FILE *f = fopen("/proc/meminfo", "r");
    unsigned long kb = 0;
    char line[128];
    if (f) {
        while (fgets(line, sizeof(line), f))
            if (sscanf(line, "MemTotal: %lu kB", &kb) == 1)
                break;
        fclose(f);
    }
    snprintf(b, l, "%lu", kb);
}

static const struct {
    const char *name;
    dm_type_t type;
    getter_fn get;
} device_info[] = {
    { "Manufacturer",          DM_STRING, get_manufacturer },
    { "ModelName",             DM_STRING, get_model },
    { "SoftwareVersion",       DM_STRING, get_sw_version },
    { "X_HostName",            DM_STRING, get_hostname },
    { "UpTime",                DM_UINT,   get_uptime },
    { "MemoryStatus.Total",    DM_UINT,   get_mem_total },
};

/* ---------- Device.ManagementServer. (writable) ---------- */

static struct {
    const char *name;
    dm_type_t type;
    char value[DM_MAX_VALUE];
} mgmt[] = {
    { "URL",                    DM_STRING, "" },
    { "PeriodicInformEnable",   DM_BOOL,   "false" },
    { "PeriodicInformInterval", DM_UINT,   "3600" },
};

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/* ---------- Device.Ethernet.Interface.{i}. ---------- */

static int list_ifaces(char names[][32], int max)
{
    DIR *d = opendir(SYS_NET);
    struct dirent *e;
    int n = 0;

    if (!d)
        return 0;
    while ((e = readdir(d)) && n < max) {
        if (e->d_name[0] == '.' || strlen(e->d_name) >= 32)
            continue;
        memcpy(names[n++], e->d_name, strlen(e->d_name) + 1);
    }
    closedir(d);

    /* stable order so instance numbers don't jump around */
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++)
            if (strcmp(names[i], names[j]) > 0) {
                char t[32];
                memcpy(t, names[i], 32);
                memcpy(names[i], names[j], 32);
                memcpy(names[j], t, 32);
            }
    return n;
}

static void iface_attr(const char *ifc, const char *attr, char *b, size_t l, const char *def)
{
    char file[256];
    snprintf(file, sizeof(file), SYS_NET "/%s/%s", ifc, attr);
    read_or(file, b, l, def);
}

/* operstate -> TR-181 Status enum */
static const char *map_status(const char *op)
{
    if (!strcmp(op, "up"))      return "Up";
    if (!strcmp(op, "down"))    return "Down";
    if (!strcmp(op, "dormant")) return "Dormant";
    if (!strcmp(op, "lowerlayerdown")) return "LowerLayerDown";
    return "Unknown";
}

/* ---------- public API ---------- */

static int emit(const char *prefix, const char *path, const char *val,
                dm_type_t t, int w, dm_visit_fn fn, void *ctx)
{
    if (!path_matches(prefix, path))
        return 0;
    fn(path, val, t, w, ctx);
    return 1;
}

int dm_get(const char *prefix, dm_visit_fn fn, void *ctx)
{
    char path[DM_MAX_PATH], val[DM_MAX_VALUE];
    char ifaces[MAX_IFACES][32];
    int count = 0, n;

    for (size_t i = 0; i < ARRAY_LEN(device_info); i++) {
        snprintf(path, sizeof(path), "Device.DeviceInfo.%s", device_info[i].name);
        if (!path_matches(prefix, path))
            continue;
        device_info[i].get(val, sizeof(val));
        count += emit(prefix, path, val, device_info[i].type, 0, fn, ctx);
    }

    n = list_ifaces(ifaces, MAX_IFACES);
    snprintf(val, sizeof(val), "%d", n);
    count += emit(prefix, "Device.Ethernet.InterfaceNumberOfEntries", val, DM_UINT, 0, fn, ctx);

    for (int i = 0; i < n; i++) {
        static const struct { const char *param, *attr; } stats[] = {
            { "BytesSent",       "statistics/tx_bytes" },
            { "BytesReceived",   "statistics/rx_bytes" },
            { "PacketsSent",     "statistics/tx_packets" },
            { "PacketsReceived", "statistics/rx_packets" },
            { "ErrorsSent",      "statistics/tx_errors" },
            { "ErrorsReceived",  "statistics/rx_errors" },
        };
        char base[64], op[32];

        snprintf(base, sizeof(base), "Device.Ethernet.Interface.%d.", i + 1);
        if (strncmp(prefix, base, strlen(prefix) < strlen(base) ? strlen(prefix) : strlen(base)) != 0)
            continue;

        snprintf(path, sizeof(path), "%sName", base);
        count += emit(prefix, path, ifaces[i], DM_STRING, 0, fn, ctx);

        iface_attr(ifaces[i], "operstate", op, sizeof(op), "unknown");
        snprintf(path, sizeof(path), "%sStatus", base);
        count += emit(prefix, path, map_status(op), DM_STRING, 0, fn, ctx);

        iface_attr(ifaces[i], "address", val, sizeof(val), "");
        snprintf(path, sizeof(path), "%sMACAddress", base);
        count += emit(prefix, path, val, DM_STRING, 0, fn, ctx);

        iface_attr(ifaces[i], "mtu", val, sizeof(val), "0");
        snprintf(path, sizeof(path), "%sMaxMTUSize", base);
        count += emit(prefix, path, val, DM_UINT, 0, fn, ctx);

        for (size_t s = 0; s < ARRAY_LEN(stats); s++) {
            iface_attr(ifaces[i], stats[s].attr, val, sizeof(val), "0");
            snprintf(path, sizeof(path), "%sStats.%s", base, stats[s].param);
            count += emit(prefix, path, val, DM_UINT, 0, fn, ctx);
        }
    }

    for (size_t i = 0; i < ARRAY_LEN(mgmt); i++) {
        snprintf(path, sizeof(path), "Device.ManagementServer.%s", mgmt[i].name);
        count += emit(prefix, path, mgmt[i].value, mgmt[i].type, 1, fn, ctx);
    }

    return count;
}

static void noop_visit(const char *p, const char *v, dm_type_t t, int w, void *c)
{
    (void)p; (void)v; (void)t; (void)w; (void)c;
}

static int valid_for_type(dm_type_t t, const char *v)
{
    char *end;
    switch (t) {
    case DM_BOOL:
        return !strcmp(v, "true") || !strcmp(v, "false") ||
               !strcmp(v, "1") || !strcmp(v, "0");
    case DM_UINT:
        if (*v == '\0' || *v == '-')
            return 0;
        strtoul(v, &end, 10);
        return *end == '\0';
    default:
        return strlen(v) < DM_MAX_VALUE;
    }
}

int dm_set(const char *path, const char *value)
{
    char full[DM_MAX_PATH];

    for (size_t i = 0; i < ARRAY_LEN(mgmt); i++) {
        snprintf(full, sizeof(full), "Device.ManagementServer.%s", mgmt[i].name);
        if (strcmp(full, path) != 0)
            continue;
        if (!valid_for_type(mgmt[i].type, value))
            return -3;
        if (mgmt[i].type == DM_BOOL)
            value = (!strcmp(value, "1") || !strcmp(value, "true")) ? "true" : "false";
        snprintf(mgmt[i].value, sizeof(mgmt[i].value), "%s", value);
        return 0;
    }

    /* known but read-only? */
    if (dm_get(path, noop_visit, NULL) > 0)
        return -2;
    return -1;
}

const char *dm_type_name(dm_type_t t)
{
    switch (t) {
    case DM_UINT: return "unsignedInt";
    case DM_BOOL: return "boolean";
    default:      return "string";
    }
}
