//
// Created by notezway on 20.10.2019.
//
#include <stdbool.h>
#include <stdint.h>

#ifndef JROARING_HASH_MAP_H
#define JROARING_HASH_MAP_H

typedef struct hash_map_s hash_map_t;

hash_map_t* hash_map_create();

hash_map_t* hash_map_create_pre_sized(uint32_t binsCount);

hash_map_t* hash_map_reload(hash_map_t* hashMap);

hash_map_t* hash_map_reload_with_count(hash_map_t* hashMap, uint32_t newBinCount);

void hash_map_free(hash_map_t* hashMap);

bool hash_map_contains(hash_map_t* hashMap, uint32_t keyLength, const void *key);

void* hash_map_get(hash_map_t* hashMap, uint32_t keyLength, const void *key);

void* hash_map_put(hash_map_t* hashMap, uint32_t keyLength, const void *key, void *value);

void* hash_map_remove(hash_map_t* hashMap, uint32_t keyLength, const void *key);

#endif //JROARING_HASH_MAP_H


