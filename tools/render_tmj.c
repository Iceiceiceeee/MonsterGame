/**
 * Render a TMJ tilemap to a flat PNG image using software compositing.
 * Usage: render_tmj <input.tmj> <output.png>
 *
 * Requires: raylib, cJSON
 * Build: gcc -O2 tools/render_tmj.c src/cJSON.c -Iinclude $(pkg-config --cflags --libs raylib) -o tools/render_tmj
 */
#include "raylib.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TILESETS 8

typedef struct {
    Image  image;
    int    firstGid;
    int    cols;
    int    tileW;
    int    tileH;
} TsInfo;

/* directory containing the TMJ */
static char g_dir[512];

/* resolve a path relative to the TMJ directory (preserves subdirectories) */
static void resolve(char *dst, size_t n, const char *rel)
{
    snprintf(dst, n, "%s/%s", g_dir, rel);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.tmj> <output.png>\n", argv[0]);
        return 1;
    }

    const char *tmjPath = argv[1];
    const char *pngPath = argv[2];

    /* extract directory from tmj path */
    strncpy(g_dir, tmjPath, sizeof(g_dir) - 1);
    char *sep = strrchr(g_dir, '/');
    char *sep2 = strrchr(g_dir, '\\');
    if (sep2 > sep) sep = sep2;
    if (sep) *sep = '\0';

    /* load TMJ */
    char *jsonStr = LoadFileText(tmjPath);
    if (!jsonStr) {
        fprintf(stderr, "Failed to load: %s\n", tmjPath);
        return 1;
    }

    cJSON *root = cJSON_Parse(jsonStr);
    if (!root) {
        fprintf(stderr, "Failed to parse JSON\n");
        UnloadFileText(jsonStr);
        return 1;
    }

    int mapW = cJSON_GetObjectItem(root, "width")->valueint;
    int mapH = cJSON_GetObjectItem(root, "height")->valueint;
    int tw   = cJSON_GetObjectItem(root, "tilewidth")->valueint;
    int th   = cJSON_GetObjectItem(root, "tileheight")->valueint;

    printf("Map: %dx%d tiles, %dx%d px per tile\n", mapW, mapH, tw, th);

    /* --- collect floor data --- */
    int  dataSize = 0;
    int *floorData = NULL;
    cJSON *layers = cJSON_GetObjectItem(root, "layers");
    cJSON *layer = NULL;
    cJSON_ArrayForEach(layer, layers) {
        const char *type = cJSON_GetObjectItem(layer, "type")->valuestring;
        const char *name = cJSON_GetObjectItem(layer, "name")->valuestring;
        if (strcmp(type, "tilelayer") == 0 && strcmp(name, "floor") == 0) {
            cJSON *dataArr = cJSON_GetObjectItem(layer, "data");
            if (dataArr && cJSON_IsArray(dataArr)) {
                dataSize = cJSON_GetArraySize(dataArr);
                floorData = malloc(sizeof(int) * dataSize);
                for (int i = 0; i < dataSize; i++) {
                    cJSON *e = cJSON_GetArrayItem(dataArr, i);
                    floorData[i] = e ? e->valueint : 0;
                }
            }
        }
    }

    if (!floorData) {
        fprintf(stderr, "No floor layer found\n");
        cJSON_Delete(root);
        UnloadFileText(jsonStr);
        return 1;
    }

    /* --- load tilesets --- */
    TsInfo tilesets[MAX_TILESETS];
    int tsCount = 0;
    cJSON *tilesetsJson = cJSON_GetObjectItem(root, "tilesets");
    if (tilesetsJson && cJSON_IsArray(tilesetsJson)) {
        cJSON *ts = NULL;
        cJSON_ArrayForEach(ts, tilesetsJson) {
            if (tsCount >= MAX_TILESETS) break;
            TsInfo *t = &tilesets[tsCount];

            t->firstGid = cJSON_GetObjectItem(ts, "firstgid")->valueint;
            const char *imgRelPath = NULL;

            /* embedded tileset */
            cJSON *imgNode = cJSON_GetObjectItem(ts, "image");
            if (imgNode) {
                imgRelPath = imgNode->valuestring;
                t->tileW = cJSON_GetObjectItem(ts, "tilewidth")->valueint;
                t->tileH = cJSON_GetObjectItem(ts, "tileheight")->valueint;
            }
            /* external .tsj reference */
            else {
                cJSON *srcNode = cJSON_GetObjectItem(ts, "source");
                if (!srcNode) continue;
                char tsjPath[1024];
                resolve(tsjPath, sizeof(tsjPath), srcNode->valuestring);
                char *tsjStr = LoadFileText(tsjPath);
                if (!tsjStr) {
                    fprintf(stderr, "Failed to load: %s\n", tsjPath);
                    continue;
                }
                cJSON *tsj = cJSON_Parse(tsjStr);
                if (tsj) {
                    imgRelPath = cJSON_GetObjectItem(tsj, "image")->valuestring;
                    t->tileW = cJSON_GetObjectItem(tsj, "tilewidth")->valueint;
                    t->tileH = cJSON_GetObjectItem(tsj, "tileheight")->valueint;
                    cJSON_Delete(tsj);
                }
                UnloadFileText(tsjStr);
            }

            if (!imgRelPath) continue;

            char imgPath[1024];
            resolve(imgPath, sizeof(imgPath), imgRelPath);
            t->image = LoadImage(imgPath);
            if (!t->image.data) {
                fprintf(stderr, "Failed to load image: %s\n", imgPath);
                continue;
            }
            t->cols = t->image.width / t->tileW;

            printf("Tileset %d: %s (firstGid=%d, %dx%d, cols=%d)\n",
                   tsCount, imgRelPath, t->firstGid, t->tileW, t->tileH, t->cols);
            tsCount++;
        }
    }

    if (tsCount == 0) {
        fprintf(stderr, "No tilesets loaded\n");
        free(floorData);
        cJSON_Delete(root);
        UnloadFileText(jsonStr);
        return 1;
    }

    /* --- composite tiles to output image --- */
    int outW = mapW * tw;
    int outH = mapH * th;
    Image out = GenImageColor(outW, outH, BLANK);
    printf("Compositing %d tiles...\n", dataSize);

    for (int y = 0; y < mapH; y++) {
        for (int x = 0; x < mapW; x++) {
            int gid = floorData[y * mapW + x];
            if (gid == 0) continue;

            /* find tileset for this gid */
            TsInfo *best = NULL;
            for (int i = 0; i < tsCount; i++) {
                if (tilesets[i].firstGid <= gid &&
                    (!best || tilesets[i].firstGid > best->firstGid)) {
                    best = &tilesets[i];
                }
            }
            if (!best) continue;

            int localId = gid - best->firstGid;
            int col = localId % best->cols;
            int row = localId / best->cols;
            Rectangle src = {
                (float)(col * best->tileW),
                (float)(row * best->tileH),
                (float)best->tileW,
                (float)best->tileH
            };
            Rectangle dst = {
                (float)(x * tw),
                (float)(y * th),
                (float)tw,
                (float)th
            };
            ImageDraw(&out, best->image, src, dst, WHITE);
        }
    }

    /* --- also draw the back image layer if present --- */

    /* save */
    if (ExportImage(out, pngPath)) {
        printf("Saved: %s (%dx%d)\n", pngPath, outW, outH);
    } else {
        fprintf(stderr, "Failed to export PNG\n");
    }

    /* cleanup */
    UnloadImage(out);
    for (int i = 0; i < tsCount; i++) UnloadImage(tilesets[i].image);
    free(floorData);
    cJSON_Delete(root);
    UnloadFileText(jsonStr);
    return 0;
}
