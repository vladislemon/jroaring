//
// Created by notezway on 22.10.2019.
//

#include <stdio.h>
#include <stdlib.h>
#include "hash_map.h"

int main() {
    hash_map_t* hashMap = hash_map_create();
    uint32_t values[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    int count = 10;
    for(int i = 0; i < count; i++) {
        printf("\n%d", i);
        printf("\n%p", (int*)hash_map_put(hashMap, sizeof(int), &i, &values[i]));
    }
    for(int i = 0; i < count; i++) {
        //printf("%d ", i);
        printf("\n%u", *(int*)hash_map_get(hashMap, sizeof(int), &i));
    }
    for(int i = 0; i < count*2; i++) {
        printf("\n%d", hash_map_contains(hashMap, sizeof(int), &i));
    }
    /*for(int i = 0; i < count*2; i++) {
        if(hash_map_contains(hashMap, sizeof(int), &i)) {
            printf("\n%p", hash_map_remove(hashMap, sizeof(int), &i));
        }
    }*/
    hashMap = hash_map_reload(hashMap);

    for(int i = 0; i < count; i++) {
        //printf("%d ", i);
        printf("\n%u", *(int*)hash_map_get(hashMap, sizeof(int), &i));
    }
    for(int i = 0; i < count*2; i++) {
        if(hash_map_contains(hashMap, sizeof(int), &i)) {
            printf("\n%p", hash_map_remove(hashMap, sizeof(int), &i));
        }
    }
    hash_map_free(hashMap);
    char *lastChar;
    printf("\n%li\n", strtol(" a 123, 50", &lastChar, 10));
    printf("%s\n", lastChar);
    return 0;
}
