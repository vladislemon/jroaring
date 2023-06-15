//
// Created by notezway on 20.10.2019.
//
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "MurmurHash3.h"
#include "hash_map.h"

#define DEFAULT_BINS_COUNT 20
#define DEFAULT_HASH_SEED 0x5EC327D1
#define LOAD_FACTOR 0.8F

typedef struct hash_map_bin_entry_s {
    uint32_t hash;
    void* value;
} hash_map_bin_entry_t;

typedef struct hash_map_bin_s {
    uint16_t size;
    hash_map_bin_entry_t elements[];
} hash_map_bin_t;

typedef struct hash_map_s {
    uint32_t hashSeed;
    uint32_t binCount;
    uint32_t valueCount;
    hash_map_bin_t* bins[];
} hash_map_t;

static hash_map_t* createMap(uint32_t hashSeed, uint32_t binsCount) {
    uint32_t lengthInBytes = sizeof(hash_map_t) + sizeof(hash_map_bin_t*) * binsCount;
    hash_map_t* hashMap = malloc(lengthInBytes);
    if(!hashMap)
        return 0;
    memset(hashMap, 0, lengthInBytes);
    hashMap->hashSeed = hashSeed;
    hashMap->binCount = binsCount;
    return hashMap;
}

static hash_map_t* reallocMap(hash_map_t* hashMap, uint32_t newBinCount) {
    if(!hashMap || newBinCount <= 0)
        return 0;
    hash_map_t* newPointerToMap = realloc(hashMap, sizeof(hash_map_t) + sizeof(hash_map_bin_t*) * newBinCount);
    if(!newPointerToMap)
        return 0;
    return newPointerToMap;
}

static void* putValue(hash_map_t* hashMap, uint32_t hash, void* value) {
    if(!hashMap)
        return 0;
    uint32_t index = hash % hashMap->binCount;
    hash_map_bin_t* bin = hashMap->bins[index];
    if(!bin) {
        bin = malloc(sizeof(hash_map_bin_t) + sizeof(hash_map_bin_entry_t));
        hashMap->bins[index] = bin;
        if(bin) {
            bin->size = 1;
            bin->elements[0].hash = hash;
            bin->elements[0].value = value;
            hashMap->valueCount++;
            return value;
        }
        return 0;
    }
    for(uint32_t i = 0; i < bin->size; i++) {
        if(bin->elements[i].hash == hash) {
            void* oldValue = bin->elements[i].value;
            bin->elements[i].hash = hash;
            bin->elements[i].value =value;
            return oldValue;
        }
    }
    bin = realloc(bin, sizeof(hash_map_bin_t) + sizeof(hash_map_bin_entry_t) * (bin->size + 1));
    if(!bin)
        return 0;
    bin->elements[bin->size].hash = hash;
    bin->elements[bin->size].value = value;
    bin->size++;
    hashMap->bins[index] = bin;
    hashMap->valueCount++;
    return value;
}

hash_map_t* hash_map_create() {
    return createMap(DEFAULT_HASH_SEED, DEFAULT_BINS_COUNT);
}

hash_map_t* hash_map_create_pre_sized(uint32_t binsCount) {
    return createMap(DEFAULT_HASH_SEED, binsCount);
}

hash_map_t* hash_map_reload(hash_map_t* hashMap) {
    if(!hashMap)
        return 0;
    return hash_map_reload_with_count(hashMap, ceilf(hashMap->valueCount / LOAD_FACTOR));
}

hash_map_t* hash_map_reload_with_count(hash_map_t* hashMap, uint32_t newBinCount) {
    if(!hashMap || newBinCount <= 0)
        return 0;
    hash_map_t* newMap = createMap(hashMap->hashSeed, newBinCount);
    if(!newMap)
        return 0;
    for(uint32_t i = 0; i < hashMap->binCount; i++) {
        if(hashMap->bins[i] != 0) {
            for (uint16_t j = 0; j < hashMap->bins[i]->size; j++) {
                hash_map_bin_entry_t entry = hashMap->bins[i]->elements[j];
                putValue(newMap, entry.hash, entry.value);
            }
        }
    }
    hash_map_free(hashMap);
    return newMap;
}

void hash_map_free(hash_map_t* hashMap) {
    if(!hashMap)
        return;
    for(uint32_t i = 0; i < hashMap->binCount; i++) {
        free(hashMap->bins[i]);
    }
    free(hashMap);
}

bool hash_map_contains(hash_map_t* hashMap, uint32_t keyLength, const void *key) {
    if(!hashMap || !key || keyLength <= 0)
        return false;
    uint32_t hash;
    MurmurHash3_x86_32(key, keyLength, hashMap->hashSeed, &hash);
    uint32_t index = hash % hashMap->binCount;
    return hashMap->bins[index] != 0;
}

void* hash_map_get(hash_map_t* hashMap, uint32_t keyLength, const void *key) {
    if(!hashMap || !key || keyLength <= 0)
        return 0;
    uint32_t hash;
    MurmurHash3_x86_32(key, keyLength, hashMap->hashSeed, &hash);
    uint32_t index = hash % hashMap->binCount;
    hash_map_bin_t* bin = hashMap->bins[index];
    if(!bin)
        return 0;
    for(uint16_t i = 0; i < bin->size; i++) {
        if(bin->elements[i].hash == hash)
            return bin->elements[i].value;
    }
    return 0;
}

void* hash_map_put(hash_map_t* hashMap, uint32_t keyLength, const void *key, void *value) {
    if(!hashMap || !key || keyLength <= 0)
        return 0;
    uint32_t hash;
    MurmurHash3_x86_32(key, keyLength, hashMap->hashSeed, &hash);
    return putValue(hashMap, hash, value);
}

void* hash_map_remove(hash_map_t* hashMap, uint32_t keyLength, const void *key) {
    if(!hashMap || !key || keyLength <= 0)
        return 0;
    uint32_t hash;
    MurmurHash3_x86_32(key, keyLength, hashMap->hashSeed, &hash);
    uint32_t index = hash % hashMap->binCount;
    hash_map_bin_t* bin = hashMap->bins[index];
    if(!bin)
        return 0;
    void* value = 0;
    if(bin->size == 1) {
        value = bin->elements[0].value;
        hashMap->bins[index] = 0;
        hashMap->valueCount--;
    } else {
        hash_map_bin_t* newBin = malloc(sizeof(hash_map_bin_t) + sizeof(hash_map_bin_entry_t) * (bin->size - 1));
        if(!newBin)
            return 0;
        newBin->size = bin->size - 1;
        uint16_t j = 0;
        for(uint16_t i = 0; i < bin->size; i++) {
            if(bin->elements[i].hash == hash) {
                value = bin->elements[i].value;
            } else {
                newBin->elements[j] = bin->elements[i];
                j++;
            }
        }
        hashMap->bins[index] = newBin;
        hashMap->valueCount--;
    }
    free(bin);
    return value;
}