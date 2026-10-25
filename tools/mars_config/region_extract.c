/**
 * @file region_extract.c
 * @brief Offline extraction of reviewed cultural flag coverage from attributed GeoJSON.
 *
 * Developer-only preparation tool: takes Natural Earth admin-1, geoBoundaries German
 * districts, the Pays Basque authority outline, French department 66 communes, French departments, PRG gminas,
 * the Finnish Sámi homeland, Frisian municipalities, Brandenburg/Saxon Sorbian settlement outlines,
 * and Mongolian districts, followed by an output path. Keeps source polygon geometry and selected identifiers;
 * it does not infer sovereignty, download data or modify installed databases.
 * See the existing globe guide and third-party notices for sources and scope.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file-t.h"
#include "json-t.h"

typedef struct { const char *source, *code; } region_mapping_t;

static const json_t *field(const json_t *object, const char *name)
{
    string_t *key = string_new_with(name);
    const json_t *value = json_object_get(object, key);
    string_free(key);
    return value;
}

static const char *text_field(const json_t *object, const char *name)
{
    const string_t *value = json_string_value(field(object, name));
    return value ? string_c_str(value) : "";
}

static bool set(json_t *object, const char *name, const json_t *value)
{
    string_t *key = string_new_with(name);
    bool ok = key && json_object_set(object, key, value);
    string_free(key);
    return ok;
}

static bool set_text(json_t *object, const char *name, const char *text)
{
    string_t *string = string_new_with(text);
    json_t *value = json_new_string(string);
    bool ok = value && set(object, name, value);
    json_free(value); string_free(string);
    return ok;
}

static int compare_mapping(const void *key, const void *row)
{
    return strcmp(key, ((const region_mapping_t *)row)->source);
}

static const char *mapped(const region_mapping_t *rows, size_t count, const char *key)
{
    const region_mapping_t *row = bsearch(key, rows, count, sizeof *rows, compare_mapping);
    return row ? row->code : NULL;
}

static const char *natural_code(const json_t *properties)
{
    static const region_mapping_t regions[] = {
        {"ES.CT", "ES-CT"}, {"ES.NA", "ES-NA"}, {"ES.PM", "ES-IB"}, {"ES.PV", "ES-PV"},
        {"ES.VC", "ES-VC"}, {"IT-88", "IT-88"}
    };
    static const region_mapping_t provinces[] = {
        {"GB-CON", "GB-CON"}, {"MN-071", "MN-KAZ"}, {"NL-FR", "NL-FR"}
    };
    const char *code = mapped(regions, sizeof regions / sizeof *regions, text_field(properties, "region_cod"));
    return code ? code : mapped(provinces, sizeof provinces / sizeof *provinces, text_field(properties, "iso_3166_2"));
}

static const char *german_code(const json_t *properties)
{
    static const region_mapping_t districts[] = {
        {"Aurich", "DE-OFR"}, {"Emden, Kreisfreie Stadt", "DE-OFR"}, {"Leer", "DE-OFR"},
        {"Nordfriesland", "DE-NFR"}, {"Wittmund", "DE-OFR"}
    };
    return mapped(districts, sizeof districts / sizeof *districts, text_field(properties, "shapeName"));
}

static const char *catalan_code(const json_t *properties)
{
    /* The 28 Occitan Fenouilledes communes, keyed by stable INSEE code, not translated names. */
    static const region_mapping_t excluded[] = {
        {"66006", ""}, {"66019", ""}, {"66035", ""}, {"66039", ""}, {"66042", ""}, {"66046", ""},
        {"66076", ""}, {"66077", ""}, {"66083", ""}, {"66092", ""}, {"66096", ""}, {"66097", ""},
        {"66107", ""}, {"66111", ""}, {"66139", ""}, {"66143", ""}, {"66151", ""}, {"66152", ""},
        {"66156", ""}, {"66158", ""}, {"66169", ""}, {"66184", ""}, {"66187", ""}, {"66198", ""},
        {"66215", ""}, {"66216", ""}, {"66232", ""}, {"66234", ""}
    };
    const char *code = text_field(properties, "code");
    return !strncmp(code, "66", 2) && !mapped(excluded, sizeof excluded / sizeof *excluded, code) ? "FR-CAT" : NULL;
}

static const char *french_code(const json_t *properties)
{
    static const region_mapping_t departments[] = {
        {"22", "FR-BRE"}, {"29", "FR-BRE"}, {"2A", "FR-COR"}, {"2B", "FR-COR"},
        {"35", "FR-BRE"}, {"44", "FR-BRE"}, {"56", "FR-BRE"}
    };
    return mapped(departments, sizeof departments / sizeof *departments, text_field(properties, "code"));
}

static const char *basque_code(const json_t *properties)
{
    (void)properties;
    return "FR-BAS";
}

static const char *polish_code(const json_t *properties)
{
    /* Reviewed municipal proxy for the ethnic and historical coverage; see the globe guide. */
    static const region_mapping_t districts[] = {
        {"2201", "PL-KAS"}, {"2202", "PL-KAS"}, {"2205", "PL-KAS"}, {"2206", "PL-KAS"},
        {"2208", "PL-KAS"}, {"2211", "PL-KAS"}, {"2212", "PL-KAS"}, {"2215", "PL-KAS"},
        {"2261", "PL-KAS"}, {"2262", "PL-KAS"}, {"2263", "PL-KAS"}, {"2264", "PL-KAS"}
    };
    /* Names retain the spelling in the GIOS PRG service, including its missing Polish diacritics. */
    static const region_mapping_t municipalities[] = {
        {"2203:Czluchów", "PL-KAS"}, {"2203:Koczala", "PL-KAS"}, {"2203:Przechlewo", "PL-KAS"},
        {"2203:Rzeczenica", "PL-KAS"}, {"2204:Kolbudy", "PL-KAS"}, {"2204:Przywidz", "PL-KAS"}
    };
    number_t number;
    if (!json_number_value(field(properties, "DISTRICT_I"), &number)) return NULL;
    double district = num_to_double(number);
    if (!(district >= 2200 && district < 2300) || district != (unsigned)district) return NULL;
    char key[128];
    snprintf(key, sizeof key, "%04u", (unsigned)district);
    const char *code = mapped(districts, sizeof districts / sizeof *districts, key);
    if (code) return code;
    int length = snprintf(key, sizeof key, "%04u:%s", (unsigned)district, text_field(properties, "NAME"));
    return length > 0 && (size_t)length < sizeof key ?
           mapped(municipalities, sizeof municipalities / sizeof *municipalities, key) : NULL;
}

static const char *sami_code(const json_t *properties)
{
    return !strcmp(text_field(properties, "Nimi"), "Saamelaisten kotiseutualue") ? "FI-SAP" : NULL;
}

static const char *frisian_code(const json_t *properties)
{
    static const region_mapping_t municipalities[] = {
        {"01056025", "DE-NFR"}, {"03453013", "DE-SAT"}
    };
    return mapped(municipalities, sizeof municipalities / sizeof *municipalities,
                  text_field(properties, "Gemeindeschlüssel_AGS"));
}

static const char *brandenburg_code(const json_t *properties)
{
    return !strcmp(text_field(properties, "sorb_info"), "Angestammtes Siedlungsgebiet der Sorben/Wenden") ?
           "DE-SOR" : NULL;
}

static const char *saxon_code(const json_t *properties)
{
    /* The dedicated L356 layer contains the entire Saxon settlement outline as one feature. */
    number_t number;
    return json_number_value(field(properties, "objectid"), &number) && num_to_double(number) == 1 ? "DE-SOR" : NULL;
}

static const char *mongolian_code(const json_t *properties)
{
    /* Pinned geoBoundaries identifiers distinguish namesakes in other aimags. */
    static const region_mapping_t districts[] = {
        {"93677404B42040735626844", "MN-KAZ"}, /* Jargalant: Khovd city. */
        {"93677404B72325521473587", "MN-KAZ"}, /* Nalaikh. */
        {"93677404B7788562092433", "MN-KAZ"},  /* Buyant, Khovd aimag. */
        {"93677404B84217279169077", "MN-KAZ"}  /* Khovd soum, Khovd aimag. */
    };
    return !strcmp(text_field(properties, "shapeGroup"), "MNG") ?
           mapped(districts, sizeof districts / sizeof *districts, text_field(properties, "shapeID")) : NULL;
}

static bool extract(json_t *output, const char *path, unsigned source)
{
    static const char *(*const selectors[])(const json_t *) = {
        natural_code, german_code, basque_code, catalan_code, french_code, polish_code, sami_code,
        frisian_code, brandenburg_code, saxon_code, mongolian_code
    };
    string_t *name = string_new_with(path);
    json_t *document = json_from_file(name);
    string_free(name);
    const json_t *features = field(document, "features");
    size_t count = json_array_size(features), selected = 0;
    bool ok = count && count <= 10000 && source < sizeof selectors / sizeof *selectors;
    for (size_t i = 0; ok && i < count; ++i) {
        const json_t *feature = json_array_get(features, i), *properties = field(feature, "properties");
        const char *code = selectors[source](properties);
        if (!code) continue;
        json_t *copy = json_new_object(), *metadata = json_new_object();
        ok = copy && metadata && set_text(copy, "type", "Feature") && set_text(metadata, "code", code) &&
             set(copy, "properties", metadata) && set(copy, "geometry", field(feature, "geometry")) &&
             json_array_append(output, copy);
        json_free(copy); json_free(metadata);
        ++selected;
    }
    fprintf(stderr, "%s: %zu selected features\n", path, selected);
    json_free(document);
    return ok && selected;
}

/* Prepare a reproducible offline subset; installation consumes only the resulting package. */
int main(int argc, char **argv)
{
    if (argc != 13) {
        fprintf(stderr, "usage: region_extract admin1 districts basque communes departments gminas sami "
                        "frisian brandenburg saxony mongolia output\n"); return 2;
    }
    json_t *document = json_new_object(), *features = json_new_array();
    bool ok = document && features;
    for (unsigned i = 0; ok && i < 11; ++i) ok = extract(features, argv[i + 1], i);
    ok = ok && set_text(document, "type", "FeatureCollection") && set(document, "features", features);
    string_t *path = string_new_with(argv[12]);
    if (ok) ok = json_to_file(document, path) == 0;
    string_free(path); json_free(features); json_free(document);
    return ok ? 0 : 1;
}
