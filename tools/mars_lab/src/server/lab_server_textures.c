/**
 * @file lab_server_textures.c
 * @brief Database-backed image routes with conditional browser-cache validation.
 *
 * Retains the existing image URLs and progressive loading order. Routes contain fixed numeric addresses, never
 * filesystem paths supplied by clients. Each server worker lazily owns a configured globe reader; opening the
 * listener does not open SQLite or preload imagery. Content digests identify images across database migrations.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "array-t.h"
#include "globe-t.h"
#include "string-t.h"
#define MARS_LAB_SERVER_INTERNAL_ACCESS
#include "lab_server_internal.h"

typedef struct {
    lab_texture_routes_t *owner;
    unsigned level, column, row;
} lab_texture_route_t;

struct _lab_texture_routes_t {
    globe_reader_t *reader;
    lab_texture_route_t routes[74 + 1152 + GLOBE_TEXTURE_REGION_LIMIT];
};

/* GET revalidation uses weak comparison and accepts an HTTP entity-tag list. Header length is server-bounded. */
static bool texture_cached(const string_t *header, const char *etag)
{
    const char *p = header ? string_c_str(header) : "";
    size_t length = strlen(etag);
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == ',') ++p;
        if (p[0] == 'W' && p[1] == '/') p += 2;
        const char *end = strchr(p, ',');
        if (!end) end = p + strlen(p);
        const char *trimmed = end;
        while (trimmed > p && (trimmed[-1] == ' ' || trimmed[-1] == '\t')) --trimmed;
        size_t size = (size_t)(trimmed - p);
        if ((size == 1 && *p == '*') || (size == length && !memcmp(p, etag, length))) return true;
        p = *end ? end + 1 : end;
    }
    return false;
}

static bool texture_request(const websrv_request_t *request, websrv_response_t *response, void *context)
{
    lab_texture_route_t *route = context;
    if (!lab_svr_header(response, "Cache-Control", "no-store") ||
        !lab_svr_header(response, "X-Content-Type-Options", "nosniff")) return false;
    if (!lab_svr_request_permitted(request))
        return lab_svr_message(request, response, 403, "MARS Lab is private; use this machine or a private network.");
    if (!route->owner->reader) route->owner->reader = globe_reader_new();
    globe_t *globe = globe_reader_get(route->owner->reader);
    globe_texture_info_t info;
    if (!globe_texture_metadata(globe, route->level, route->column, route->row, &info))
        return lab_svr_message(request, response, 404, "Globe texture is not installed");
    string_t *etag = string_sprintf("\"sha256-%s\"", info.sha256);
    bool cached = etag && texture_cached(lab_svr_request_header(request, "If-None-Match"), string_c_str(etag));
    array_t *bytes = !cached && etag ? globe_texture_png(globe, route->level, route->column, route->row, &info) : NULL;
    bool ok;
    if (!etag || (!cached && !bytes)) {
        ok = lab_svr_message(request, response, 500, "Globe texture failed validation");
    } else {
        ok = lab_svr_header(response, "Content-Type", "image/png") &&
             lab_svr_header(response, "Cache-Control", "private, no-cache") &&
             lab_svr_header(response, "ETag", string_c_str(etag)) &&
             (cached ? websrv_response_status(response, 304)
                     : websrv_response_body(response, array_get(bytes, 0), array_size(bytes)));
    }
    array_destroy(bytes);
    string_free(etag);
    return ok;
}

static bool texture_block_request(const websrv_request_t *request, websrv_response_t *response, void *context)
{
    lab_texture_routes_t *owner = context;
    if (!lab_svr_header(response, "Cache-Control", "no-store") ||
        !lab_svr_header(response, "X-Content-Type-Options", "nosniff")) return false;
    if (!lab_svr_request_permitted(request))
        return lab_svr_message(request, response, 403, "MARS Lab is private; use this machine or a private network.");
    const string_t *target = websrv_request_target(request);
    char source[65] = {0}, tier[11] = {0}, row[11] = {0}, column[11] = {0};
    int end = 0;
    const char *text = target ? string_c_str(target) : "";
    if (!target || string_byte_length(target) > 180 ||
        sscanf(text, "/earth-block?v=1&sha=%64[a-f0-9]&tier=%10[0-9]&row=%10[0-9]&column=%10[0-9]%n",
            source, tier, row, column, &end) != 4 || !end || text[end] || strlen(source) != 64)
        return lab_svr_message(request, response, 400, "Invalid photographic block address");
    unsigned long size = strtoul(tier, NULL, 10), y = strtoul(row, NULL, 10), x = strtoul(column, NULL, 10);
    if (size < 64 || size > 8192 || x >= (size + 511) / 512 || y >= (size + 511) / 512)
        return lab_svr_message(request, response, 400, "Invalid photographic block coordinates");
    if (!owner->reader) owner->reader = globe_reader_new();
    globe_t *globe = globe_reader_get(owner->reader);
    globe_texture_info_t info;
    array_t *bytes = globe_texture_block(globe, source, (unsigned)size, (unsigned)y, (unsigned)x, &info);
    if (!bytes) return lab_svr_message(request, response, 404, "Photographic block is not installed");
    string_t *etag = string_sprintf("\"sha256-%s\"", info.sha256);
    bool cached = etag && texture_cached(lab_svr_request_header(request, "If-None-Match"), string_c_str(etag));
    bool ok = etag && lab_svr_header(response, "Content-Type", "image/png") &&
        lab_svr_header(response, "X-Content-Type-Options", "nosniff") &&
        lab_svr_header(response, "Cache-Control", "private, max-age=31536000, immutable") &&
        lab_svr_header(response, "ETag", string_c_str(etag)) &&
        (cached ? websrv_response_status(response, 304) :
            websrv_response_body(response, array_get(bytes, 0), array_size(bytes)));
    string_free(etag); array_destroy(bytes);
    return ok;
}

lab_texture_routes_t *lab_svr_textures_new(websrv_t *listener)
{
    lab_texture_routes_t *owner = calloc(1, sizeof *owner);
    if (!owner) return NULL;
    unsigned index = 0;
    bool ok = true;
    for (unsigned level = 0; ok && level < 5; ++level) {
        unsigned rows = level == 4 ? 24 : level == 2 ? 6 : 1;
        unsigned columns = level == 4 ? 48 : level == 3 ? GLOBE_TEXTURE_REGION_LIMIT : level == 2 ? 12 : 1;
        for (unsigned row = 0; ok && row < rows; ++row) {
            for (unsigned column = 0; ok && column < columns; ++column) {
                lab_texture_route_t *route = &owner->routes[index++];
                *route = (lab_texture_route_t){owner, level, column, row};
                string_t *path = level == 4 ? string_sprintf("/earth_tiles_detail/%02u-%02u.png", column, row) :
                                level == 3 ? string_sprintf("/earth_regions/%02u.png", column) :
                                level == 2 ? string_sprintf("/earth_tiles/%02u-%02u.png", column, row)
                                           : string_new_with(level ? "/earth_surface_detail.png" : "/earth_surface.png");
                ok = path && websrv_route(listener, HTTP_GET, path, texture_request, route);
                string_free(path);
            }
        }
    }
    string_t *block = ok ? string_new_with("/earth-block") : NULL;
    ok = block && websrv_route(listener, HTTP_GET, block, texture_block_request, owner);
    string_free(block);
    if (!ok) { lab_svr_textures_free(owner); owner = NULL; }
    return owner;
}

void lab_svr_textures_free(lab_texture_routes_t *routes)
{
    if (!routes) return;
    globe_reader_free(routes->reader);
    free(routes);
}
