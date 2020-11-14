#include "channel_map.h"

struct channel_map* chanmap_init(int msize) 
{
    int initsz = CHANNELMAP_INITSIZE;
    void** entries;

    struct channel_map* chanMap = malloc(sizeof(struct channel_map));
    if (chanMap == NULL) goto failed;

    entries = cmalloc(msize * initsz);
    if (entries == NULL) goto failed;

    chanMap->nentry = initsz;
    chanMap->entries = entries;

    return chanMap;

failed:
    if (entries != NULL) free(entries);
    if (chanMap != NULL) free(chanMap);
    return NULL;
}

int chanmap_expand(struct channel_map* chanmap, int slot, int msize)
{
    if (slot < chanmap->nentry) return 0;

    int nsize = chanmap->nentry;
    void** nentries = NULL;

    if (nsize >= CHANNELMAP_MAXSIZE / 2) nsize = CHANNELMAP_MAXSIZE;
    else while (nsize <= slot) nsize <<= 1; // 向上取2的倍数

    nentries = realloc(chanmap->entries, nsize * msize);
    if (nentries == NULL) return -1;
    memset(&nentries[chanmap->nentry], 0, (nsize - chanmap->nentry) * msize);

    chanmap->entries = nentries;
    chanmap->nentry = nsize;
    return 0;
}


void chanmap_clear(struct channel_map* chanmap)
{
    if (chanmap != NULL) {
        if (chanmap->entries != NULL) {
            for (int i = 0; i < chanmap->nentry; i++) {
                if (chanmap[i] != NULL) free(chanmap[i]);
            }
            free(chanmap->entries);
        }
        free(chanmap);
    }
} 
