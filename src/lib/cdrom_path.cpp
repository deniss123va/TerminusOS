#include "cdrom_path.h"
#include "../drivers/iso9660.h"
#include "string.h"

bool path_is_cdrom_prefixed(const char* p) {
    const char* pfx = "/cdrom";
    int pfxlen = 6, plen = strlen(p);
    if (plen < pfxlen) return false;
    for (int i = 0; i < pfxlen; i++) if (p[i] != pfx[i]) return false;
    return (plen == pfxlen || p[pfxlen] == '/');
}

void build_iso_path(const char* name, char* out) {
    if (path_is_cdrom_prefixed(name)) {
        const char* rest = name + 6; // после "/cdrom"
        if (*rest == 0) { out[0] = '/'; out[1] = 0; return; }
        int p = 0; for (int i = 0; rest[i] && p < ISO_MAX_PATH-1; i++) out[p++] = rest[i]; out[p] = 0;
        return;
    }
    int p = 0;
    for (int i = 0; iso_current_path[i] && p < ISO_MAX_PATH-2; i++) out[p++] = iso_current_path[i];
    if (p == 0 || out[p-1] != '/') out[p++] = '/';
    for (int i = 0; name[i] && p < ISO_MAX_PATH-1; i++) out[p++] = name[i];
    out[p] = 0;
}
