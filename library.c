#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <roaring/roaring.h>
#include "hash_map.h"
#include "ua_com_ubuntuzone_features_ProductFeaturesNativeRoaring.h"

typedef struct sorting_index_s {
    uint32_t *products;
    uint32_t *indices;
} sorting_index_t;

typedef struct product_attribute_s {
    float value;
    uint32_t productId;
} product_attribute_t;

typedef struct similar_product_s {
    uint32_t productId;
    uint8_t hitPercent;
} similar_product_t;

typedef struct jroaring_s {

    roaring_bitmap_t **productFeatures;
    roaring_bitmap_t **productFeaturesExt;
    roaring_bitmap_t **featureProducts;
    roaring_bitmap_t **featureProductsExt;
    roaring_bitmap_t **featureGroups;
    roaring_bitmap_t **groupProducts;
    roaring_bitmap_t **groupFeatures;

    uint32_t productCount;
    uint32_t featureCount;
    uint32_t minProduct;
    uint32_t maxProduct;
    uint32_t minFeature;
    uint32_t maxFeature;
    uint32_t minFeatureExt;
    uint32_t maxFeatureExt;
    uint32_t minGroup;
    uint32_t maxGroup;
    uint32_t similarHit;

    uint32_t *indexToProduct;
    uint32_t *productToIndex;

    uint32_t *indexToGroup;

    uint32_t *indexToGroupOrder;

    hash_map_t *productAttributes;
    uint32_t attributeNameCount;
    char **attributeNames;

    hash_map_t *sortingIndexes;
    uint32_t sortingIndexNameCount;
    char **sortingIndexNames;

} jroaring_t;

static jroaring_t *createStorage() {
    jroaring_t *storage = malloc(sizeof(jroaring_t));
    memset(storage, 0, sizeof(jroaring_t));
    return storage;
}

static void clearBitmaps(uint32_t length, roaring_bitmap_t **bitmaps) {
    if (bitmaps) {
        for (uint32_t i = 0; i < length; i++) {
            if (bitmaps[i]) {
                roaring_bitmap_free(bitmaps[i]);
                bitmaps[i] = 0;
            }
        }
    }
}

static inline void freeProductAttributes(jroaring_t *storage) {
    for (uint32_t i = 0; i < storage->attributeNameCount; i++) {
        char *attributeName = storage->attributeNames[i];
        product_attribute_t *attributes = hash_map_get(storage->productAttributes, strlen(attributeName),
                                                       attributeName);
        if (attributes) {
            free(attributes);
        }
    }
    hash_map_free(storage->productAttributes);
}

static inline void freeSortingIndex(sorting_index_t *sortingIndex) {
    free(sortingIndex->indices);
    free(sortingIndex->products);
    free(sortingIndex);
}

static inline void freeSortingIndexes(jroaring_t *storage) {
    for (uint32_t i = 0; i < storage->sortingIndexNameCount; i++) {
        char *indexName = storage->sortingIndexNames[i];
        sorting_index_t *sortingIndex = hash_map_get(storage->sortingIndexes, strlen(indexName), indexName);
        if (sortingIndex) {
            freeSortingIndex(sortingIndex);
        }
    }
    hash_map_free(storage->sortingIndexes);
}

static void clearStorage(jroaring_t *storage) {
    if (storage && storage->productCount > 0 && storage->featureCount > 0) {
        if (storage->productFeatures) {
            clearBitmaps(storage->productCount, storage->productFeatures);
            free(storage->productFeatures);
        }
        if (storage->productFeaturesExt) {
            clearBitmaps(storage->productCount, storage->productFeaturesExt);
            free(storage->productFeaturesExt);
        }
        if (storage->featureProducts) {
            clearBitmaps(storage->featureCount, storage->featureProducts);
            free(storage->featureProducts);
        }
        if (storage->featureProductsExt) {
            clearBitmaps(storage->featureCount, storage->featureProductsExt);
            free(storage->featureProductsExt);
        }
        if (storage->featureGroups) {
            clearBitmaps(storage->featureCount, storage->featureGroups);
            free(storage->featureGroups);
        }
        if (storage->groupProducts) {
            clearBitmaps(storage->maxGroup + 1, storage->groupProducts);
            free(storage->groupProducts);
        }
        if (storage->groupFeatures) {
            clearBitmaps(storage->maxGroup + 1, storage->groupFeatures);
            free(storage->groupFeatures);
        }
        if (storage->indexToProduct)
            free(storage->indexToProduct);
        if (storage->productToIndex)
            free(storage->productToIndex);
        if (storage->indexToGroup)
            free(storage->indexToGroup);
        if (storage->indexToGroupOrder)
            free(storage->indexToGroupOrder);
        if (storage->productAttributes)
            freeProductAttributes(storage);
        if (storage->attributeNames) {
            for (uint32_t i = 0; i < storage->attributeNameCount; i++) {
                free(storage->attributeNames[i]);
            }
            free(storage->attributeNames);
        }
        if (storage->sortingIndexes)
            freeSortingIndexes(storage);
        if (storage->sortingIndexNames) {
            for (uint32_t i = 0; i < storage->sortingIndexNameCount; i++) {
                free(storage->sortingIndexNames[i]);
            }
            free(storage->sortingIndexNames);
        }

        memset(storage, 0, sizeof(jroaring_t));
    }
}

static roaring_bitmap_t *roaring_bitmap_from_jint_array(JNIEnv *env, jintArray array) {
    jboolean isCopy;
    jint length = (*env)->GetArrayLength(env, array);
    jint *elements = (*env)->GetIntArrayElements(env, array, &isCopy);
    roaring_bitmap_t *bitmap = roaring_bitmap_of_ptr(length, (const uint32_t *) elements);
    (*env)->ReleaseIntArrayElements(env, array, elements, JNI_ABORT);
    return bitmap;
}

static void addAttribute(JNIEnv *env, jroaring_t *storage, jint index, jint productId, jstring name, jfloat value) {
    jint nameLength = (*env)->GetStringUTFLength(env, name);
    jboolean isCopy;
    const char *nameChars = (*env)->GetStringUTFChars(env, name, &isCopy);
    product_attribute_t *attributes = hash_map_get(storage->productAttributes, nameLength, nameChars);
    if (!attributes) {
        attributes = malloc(sizeof(product_attribute_t *) * storage->productCount);
        if (!attributes)
            return;
        hash_map_put(storage->productAttributes, nameLength, nameChars, attributes);

        storage->attributeNameCount++;
        if (!storage->attributeNames) {
            storage->attributeNames = malloc(sizeof(char *) * storage->attributeNameCount);
        } else {
            storage->attributeNames = realloc(storage->attributeNames, sizeof(char *) * storage->attributeNameCount);
        }
        storage->attributeNames[storage->attributeNameCount - 1] = malloc(nameLength + 1);
        memcpy(storage->attributeNames[storage->attributeNameCount - 1], nameChars, nameLength + 1);
    }
    attributes[index].value = value;
    attributes[index].productId = productId;
    (*env)->ReleaseStringUTFChars(env, name, nameChars);
}

static int compareAttributes(const void *attribute1, const void *attribute2) {
    if (((product_attribute_t *) attribute1)->value < ((product_attribute_t *) attribute2)->value)
        return -1;
    if (((product_attribute_t *) attribute1)->value > ((product_attribute_t *) attribute2)->value)
        return 1;
    return 0;
}

static int compareSimilar(const void *similar1, const void *similar2) {
    if (((similar_product_t *) similar1)->hitPercent < ((similar_product_t *) similar2)->hitPercent)
        return 1;
    if (((similar_product_t *) similar1)->hitPercent > ((similar_product_t *) similar2)->hitPercent)
        return -1;
    return 0;
}

static void getMatches(JNIEnv *env, jroaring_t *storage, jstring expressionString, roaring_bitmap_t *matches) {
    roaring_bitmap_t *subMatches = roaring_bitmap_create();
    jint expressionLength = (*env)->GetStringUTFLength(env, expressionString);
    jboolean isCopy;
    const char *expression = (*env)->GetStringUTFChars(env, expressionString, &isCopy);
    char groupOperator = '|';
    char subOperator = '|';
    char *currentOperatorPtr = &groupOperator;
    //      (0&1)|(0&2)
    for (jint i = 0; i < expressionLength; i++) {
        switch (expression[i]) {
            case '(':
                currentOperatorPtr = &subOperator;
                break;
            case ')':
                currentOperatorPtr = &groupOperator;
                if (groupOperator == '&') {
                    roaring_bitmap_and_inplace(matches, subMatches);
                } else if (groupOperator == '|') {
                    roaring_bitmap_or_inplace(matches, subMatches);
                }
                roaring_bitmap_clear(subMatches);
                subOperator = '|';
                break;
            case '&':
            case '|':
                currentOperatorPtr[0] = expression[i];
                break;
            default:
                roaring_bitmap_t *bitmap = currentOperatorPtr == &groupOperator ? matches : subMatches;
                char *lastChar;
                uint32_t index = strtol(expression + i, &lastChar, 10);
                i = lastChar - expression - 1;
                if (index >= storage->featureCount)
                    continue;
                if (currentOperatorPtr[0] == '&') {
                    if (storage->featureProducts[index])
                        roaring_bitmap_and_inplace(bitmap, storage->featureProducts[index]);
                    else
                        roaring_bitmap_clear(bitmap);
                } else if (currentOperatorPtr[0] == '|') {
                    if (storage->featureProducts[index])
                        roaring_bitmap_or_inplace(bitmap, storage->featureProducts[index]);
                }
                break;
        }
    }
    roaring_bitmap_free(subMatches);
    (*env)->ReleaseStringUTFChars(env, expressionString, expression);
}

static inline void applyFilter(roaring_bitmap_t *bitmap, uint32_t attributeCount, product_attribute_t *attributes,
                               float fromValue, float toValue) {
    if (toValue < fromValue && toValue != -1)
        return;
    uint32_t fromIndex;
    uint32_t toIndex;
    if (fromValue < 0) {
        fromIndex = 0;
    } else {
        fromIndex = attributeCount / 2;
        uint32_t distance = attributeCount / 2 / 2 + 1;
        while (true) {
            if (attributes[fromIndex].value >= fromValue) {
                if (fromIndex <= 0 || attributes[fromIndex - 1].value < fromValue) {
                    break;
                }
                fromIndex -= distance;
            } else {
                if (fromIndex >= attributeCount - 1) {
                    fromIndex = -1;
                    break;
                }
                fromIndex += distance;
            }
            distance = (distance / 2) + (distance % 2);
        }
    }
    if (fromIndex == -1) {
        roaring_bitmap_clear(bitmap);
        return;
    }
    if (toValue < 0) {
        toIndex = attributeCount - 1;
    } else {
        toIndex = attributeCount / 2;
        uint32_t distance = attributeCount / 2 / 2 + 1;
        while (true) {
            if (attributes[toIndex].value <= toValue) {
                if (toIndex >= attributeCount - 1 || attributes[toIndex + 1].value > toValue) {
                    break;
                }
                toIndex += distance;
            } else {
                if (toIndex <= 0) {
                    toIndex = -1;
                    break;
                }
                toIndex -= distance;
            }
            distance = (distance / 2) + (distance % 2);
        }
    }
    if (toIndex == -1) {
        roaring_bitmap_clear(bitmap);
        return;
    }
    if (fromIndex == 0 && toIndex == attributeCount - 1)
        return;
    roaring_bitmap_t *filterBitmap = roaring_bitmap_create();
    for (uint32_t i = fromIndex; i <= toIndex; i++) {
        roaring_bitmap_add(filterBitmap, attributes[i].productId);
    }
    roaring_bitmap_and_inplace(bitmap, filterBitmap);
    roaring_bitmap_free(filterBitmap);
}

static inline void
applyFilters(JNIEnv *env, jroaring_t *storage, roaring_bitmap_t *bitmap, jobjectArray filterNamesArray,
             jfloatArray filterFromValuesArray, jfloatArray filterToValuesArray) {
    jsize filterCount = (*env)->GetArrayLength(env, filterNamesArray);
    jboolean isCopy;
    jfloat *fromValues = (*env)->GetFloatArrayElements(env, filterFromValuesArray, &isCopy);
    jfloat *toValues = (*env)->GetFloatArrayElements(env, filterToValuesArray, &isCopy);
    for (jsize i = 0; i < filterCount; i++) {
        jstring filterNameString = (*env)->GetObjectArrayElement(env, filterNamesArray, i);
        jsize filterNameLength = (*env)->GetStringUTFLength(env, filterNameString);
        const char *filterName = (*env)->GetStringUTFChars(env, filterNameString, &isCopy);
        product_attribute_t *attributes = hash_map_get(storage->productAttributes, filterNameLength, filterName);
        (*env)->ReleaseStringUTFChars(env, filterNameString, filterName);
        if (attributes) {
            applyFilter(bitmap, storage->productCount, attributes, fromValues[i], toValues[i]);
        }
    }
    (*env)->ReleaseFloatArrayElements(env, filterFromValuesArray, fromValues, JNI_ABORT);
    (*env)->ReleaseFloatArrayElements(env, filterToValuesArray, toValues, JNI_ABORT);
}

static inline uint32_t getGroupSize(jroaring_t *storage, roaring_bitmap_t *products, roaring_bitmap_t *groups) {
    roaring_bitmap_clear(groups);
    roaring_uint32_iterator_t *iterator = roaring_create_iterator(products);
    while (iterator->has_value) {
        roaring_bitmap_add(groups, storage->indexToGroup[storage->productToIndex[iterator->current_value]]);
        roaring_advance_uint32_iterator(iterator);
    }
    roaring_free_uint32_iterator(iterator);
    return roaring_bitmap_get_cardinality(groups);
}

static void setSortingIndex(jroaring_t *storage, uint32_t sortingIdLength, const char *sortingId,
                            uint32_t sortedProductCount, const uint32_t *sortedProducts) {
    sorting_index_t *sortingIndex = malloc(sizeof(sorting_index_t));
    sortingIndex->products = malloc(sizeof(uint32_t) * sortedProductCount);
    memcpy(sortingIndex->products, sortedProducts, sortedProductCount);
    uint32_t maxProductId = sortedProducts[0];
    for (uint32_t i = 1; i < sortedProductCount; i++) {
        if (sortedProducts[i] > maxProductId)
            maxProductId = sortedProducts[i];
    }
    sortingIndex->indices = malloc(sizeof(uint32_t) * (maxProductId + 1));
    for (uint32_t i = 0; i <= maxProductId; i++) {
        sortingIndex->indices[i] = -1;
    }
    for (uint32_t i = 1; i < sortedProductCount; i++) {
        sortingIndex->indices[sortedProducts[i]] = i;
    }
    sorting_index_t *previousValue = hash_map_put(storage->sortingIndexes, sortingIdLength, sortingId, sortingIndex);
    if (previousValue && previousValue != sortingIndex)
        freeSortingIndex(previousValue);
}

JNIEXPORT jlong JNICALL Java_ua_com_ubuntuzone_features_ProductFeaturesNativeRoaring_init
        (JNIEnv *env, jclass class) {
    return (jlong) createStorage();
}

JNIEXPORT void JNICALL Java_ua_com_ubuntuzone_features_ProductFeaturesNativeRoaring_initStorage
        (JNIEnv *env, jclass class, jlong pointer, jint rowCount, jint columnCount) {

    jroaring_t *storage = (jroaring_t *) pointer;
    clearStorage(storage);

    storage->productCount = rowCount;
    storage->featureCount = columnCount;

    storage->similarHit = 50;

    storage->productFeatures = malloc(sizeof(roaring_bitmap_t *) * rowCount);
    storage->productFeaturesExt = malloc(sizeof(roaring_bitmap_t *) * rowCount);

    storage->featureProducts = malloc(sizeof(roaring_bitmap_t *) * columnCount);
    memset(storage->featureProducts, 0, sizeof(roaring_bitmap_t *) * columnCount);
    storage->featureGroups = malloc(sizeof(roaring_bitmap_t *) * columnCount);
    memset(storage->featureGroups, 0, sizeof(roaring_bitmap_t *) * columnCount);
    storage->featureProductsExt = malloc(sizeof(roaring_bitmap_t *) * columnCount);
    memset(storage->featureProductsExt, 0, sizeof(roaring_bitmap_t *) * columnCount);

    storage->indexToProduct = malloc(sizeof(uint32_t) * rowCount);
    storage->indexToGroup = malloc(sizeof(uint32_t) * rowCount);
    storage->indexToGroupOrder = malloc(sizeof(uint32_t) * rowCount);

    storage->productAttributes = hash_map_create();
    storage->sortingIndexes = hash_map_create();
}

JNIEXPORT void JNICALL Java_ua_com_ubuntuzone_features_ProductFeaturesNativeRoaring_addItem
        (JNIEnv *env, jclass class, jlong pointer, jint index, jint productId, jint groupId, jint groupOrder,
         jintArray featuresArray, jintArray extFeaturesArray, jobjectArray attributeNamesArray,
         jfloatArray attributeValuesArray) {

    jroaring_t *storage = (jroaring_t *) pointer;

    storage->productFeatures[index] = roaring_bitmap_from_jint_array(env, featuresArray);
    storage->productFeaturesExt[index] = roaring_bitmap_from_jint_array(env, extFeaturesArray);

    storage->indexToProduct[index] = productId;
    storage->indexToGroup[index] = groupId;
    storage->indexToGroupOrder[index] = groupOrder;

    //minimums
    if (productId < storage->minProduct) {
        storage->minProduct = productId;
    }
    if (groupId > storage->minGroup) {
        storage->minGroup = groupId;
    }
    {
        uint32_t localMinFeature = roaring_bitmap_minimum(storage->productFeatures[index]);
        if (localMinFeature > storage->minFeature) {
            storage->minFeature = localMinFeature;
        }
    }
    {
        uint32_t localMinFeatureExt = roaring_bitmap_minimum(storage->productFeaturesExt[index]);
        if (localMinFeatureExt > storage->minFeatureExt) {
            storage->minFeatureExt = localMinFeatureExt;
        }
    }

    //maximums
    if (productId > storage->maxProduct) {
        storage->maxProduct = productId;
    }
    if (groupId > storage->maxGroup) {
        storage->maxGroup = groupId;
    }
    {
        uint32_t localMaxFeature = roaring_bitmap_maximum(storage->productFeatures[index]);
        if (localMaxFeature > storage->maxFeature) {
            storage->maxFeature = localMaxFeature;
        }
    }
    {
        uint32_t localMaxFeatureExt = roaring_bitmap_maximum(storage->productFeaturesExt[index]);
        if (localMaxFeatureExt > storage->maxFeatureExt) {
            storage->maxFeatureExt = localMaxFeatureExt;
        }
    }

    jint attributeCount = (*env)->GetArrayLength(env, attributeNamesArray);
    if (attributeCount > 0) {
        jboolean isCopy;
        jfloat *values = (*env)->GetFloatArrayElements(env, attributeValuesArray, &isCopy);
        for (jint i = 0; i < attributeCount; i++) {
            jstring name = (*env)->GetObjectArrayElement(env, attributeNamesArray, i);
            addAttribute(env, storage, index, productId, name, values[i]);
        }
        (*env)->ReleaseFloatArrayElements(env, attributeValuesArray, values, JNI_ABORT);
    }
}

JNIEXPORT void JNICALL Java_ua_com_ubuntuzone_features_ProductFeaturesNativeRoaring_completeLoadData
        (JNIEnv *env, jclass class, jlong pointer) {
    jroaring_t *storage = (jroaring_t *) pointer;

    {
        uint32_t length;

        length = sizeof(uint32_t) * (storage->maxProduct + 1);
        storage->productToIndex = malloc(length);
        memset(storage->productToIndex, 0, length);

        length = sizeof(roaring_bitmap_t *) * (storage->maxGroup + 1);
        storage->groupProducts = malloc(length);
        memset(storage->groupProducts, 0, length);

        length = sizeof(roaring_bitmap_t *) * (storage->maxGroup + 1);
        storage->groupFeatures = malloc(length);
        memset(storage->groupFeatures, 0, length);
    }

    for (uint32_t i = 0; i < storage->productCount; i++) {
        storage->productToIndex[storage->indexToProduct[i]] = i;

        if (!storage->groupProducts[storage->indexToGroup[i]]) {
            storage->groupProducts[storage->indexToGroup[i]] = roaring_bitmap_create();
        }
        roaring_bitmap_add(storage->groupProducts[storage->indexToGroup[i]], storage->indexToProduct[i]);

        if (!storage->groupFeatures[storage->indexToGroup[i]]) {
            storage->groupFeatures[storage->indexToGroup[i]] = roaring_bitmap_create();
        }

        roaring_uint32_iterator_t *iterator;

        iterator = roaring_create_iterator(storage->productFeatures[i]);
        while (iterator->has_value) {
            if (!storage->featureProducts[iterator->current_value]) {
                storage->featureProducts[iterator->current_value] = roaring_bitmap_create();
            }
            roaring_bitmap_add(storage->featureProducts[iterator->current_value], storage->indexToProduct[i]);
            if (!storage->featureGroups[iterator->current_value]) {
                storage->featureGroups[iterator->current_value] = roaring_bitmap_create();
            }
            roaring_bitmap_add(storage->featureGroups[iterator->current_value], storage->indexToGroup[i]);
            roaring_bitmap_add(storage->groupFeatures[storage->indexToGroup[i]], iterator->current_value);
            roaring_advance_uint32_iterator(iterator);
        }
        roaring_free_uint32_iterator(iterator);

        iterator = roaring_create_iterator(storage->productFeaturesExt[i]);
        while (iterator->has_value) {
            if (!storage->featureProductsExt[iterator->current_value]) {
                storage->featureProductsExt[iterator->current_value] = roaring_bitmap_create();
            }
            roaring_bitmap_add(storage->featureProductsExt[iterator->current_value], storage->indexToProduct[i]);
            roaring_advance_uint32_iterator(iterator);
        }
        roaring_free_uint32_iterator(iterator);

        //roaring_bitmap_run_optimize(storage->productFeatures[i]);
        //roaring_bitmap_run_optimize(storage->productFeaturesExt[i]);
    }

    /*for(uint32_t i = 0; i < storage->featureCount; i++) {
        if(storage->featureProducts[i]) {
            roaring_bitmap_run_optimize(storage->featureProducts[i]);
        }
        if(storage->featureProductsExt[i]) {
            roaring_bitmap_run_optimize(storage->featureProductsExt[i]);
        }
        if(storage->featureGroups[i]) {
            roaring_bitmap_run_optimize(storage->featureGroups[i]);
        }
    }

    for(uint32_t i = storage->minGroup; i < storage->maxGroup + 1; i++) {
        if(storage->groupProducts[i]) {
            roaring_bitmap_run_optimize(storage->groupProducts[i]);
        }
        if(storage->groupFeatures[i]) {
            roaring_bitmap_run_optimize(storage->groupFeatures[i]);
        }
    }*/

    uint32_t *sortedProducts = malloc(sizeof(uint32_t) * storage->productCount);
    for (uint32_t i = 0; i < storage->attributeNameCount; i++) {
        const char *attribName = storage->attributeNames[i];
        uint32_t nameLength = strlen(attribName);
        product_attribute_t *attributes = hash_map_get(storage->productAttributes, nameLength, attribName);
        qsort(attributes, storage->productCount, sizeof(product_attribute_t), compareAttributes);
        for (uint32_t j = 0; j < storage->productCount; j++) {
            sortedProducts[j] = attributes[j].productId;
        }
        setSortingIndex(storage, nameLength, attribName, storage->productCount, sortedProducts);
    }
    free(sortedProducts);
}

JNIEXPORT jlong JNICALL Java_ua_com_ubuntuzone_features_ProductFeaturesNativeRoaring_setSortingIndex
        (JNIEnv *env, jclass class, jlong pointer, jstring sortingIdString, jintArray sortingValuesArray) {

    jroaring_t *storage = (jroaring_t *) pointer;

    jint sortedProductCount = (*env)->GetArrayLength(env, sortingValuesArray);
    jint nameLength = (*env)->GetStringUTFLength(env, sortingIdString);
    const char *nameChars = (*env)->GetStringUTFChars(env, sortingIdString, NULL);
    jint *sortedProducts = (*env)->GetIntArrayElements(env, sortingValuesArray, NULL);
    setSortingIndex(storage, nameLength, nameChars, sortedProductCount, (const uint32_t *) sortedProducts);
    (*env)->ReleaseIntArrayElements(env, sortingValuesArray, sortedProducts, JNI_ABORT);
    (*env)->ReleaseStringUTFChars(env, sortingIdString, nameChars);

    return 1;
}

JNIEXPORT jobject JNICALL Java_ua_com_ubuntuzone_features_ProductFeaturesNativeRoaring_lookupProducts
        (JNIEnv *env, jclass class, jlong pointer, jstring expressionString, jboolean isGrouped,
         jobjectArray filterNamesArray, jfloatArray filterFromValuesArray, jfloatArray filterToValuesArray,
         jstring sortingIdString, jboolean isAscending, jint fromBit, jint toBit) {

    jroaring_t *storage = (jroaring_t *) pointer;

    roaring_bitmap_t *matches = roaring_bitmap_create();
    getMatches(env, storage, expressionString, matches);
    applyFilters(env, storage, matches, filterNamesArray, filterFromValuesArray, filterToValuesArray);

    uint32_t matchesCardinality = roaring_bitmap_get_cardinality(matches);
    uint32_t resultLength = sizeof(jfloat) * 2 + sizeof(jint) * 2 + sizeof(jint) * matchesCardinality;
    uint32_t *result = malloc(resultLength);
    sorting_index_t *sortingIndex = NULL;

    if (sortingIdString) {
        jsize sortingIdLength = (*env)->GetStringUTFLength(env, sortingIdString);
        const char *sortingId = (*env)->GetStringUTFChars(env, sortingIdString, NULL);
        sortingIndex = hash_map_get(storage->sortingIndexes, sortingIdLength, sortingId);
        (*env)->ReleaseStringUTFChars(env, sortingIdString, sortingId);
        if (!sortingIndex) {
            roaring_bitmap_free(matches);
            free(result);
            return 0;
        }
        roaring_bitmap_t *sortedMatches = roaring_bitmap_create();
        roaring_uint32_iterator_t *iterator = roaring_create_iterator(matches);
        while (iterator->has_value) {
            uint32_t index = sortingIndex->indices[iterator->current_value];
            index = index != -1 ? index : (storage->productCount + iterator->current_value);
            roaring_bitmap_add(sortedMatches, index);
            roaring_advance_uint32_iterator(iterator);
        }
        roaring_free_uint32_iterator(iterator);
        roaring_bitmap_free(matches);
        matches = sortedMatches;
    }
    float minPrice = 0;
    float maxPrice = 0;
    uint32_t *maxGroupOrderIndices = malloc(sizeof(uint32_t) * (storage->maxGroup + 1));
    {
        const char *priceAttributeName = "price";
        product_attribute_t *priceAttributes = hash_map_get(storage->productAttributes, strlen(priceAttributeName),
                                                            priceAttributeName);
        roaring_uint32_iterator_t *iterator = roaring_create_iterator(matches);
        uint32_t i = 0;
        while (iterator->has_value) {
            uint32_t resultIndex = isAscending ? i : matchesCardinality - i - 1;
            uint32_t productId = sortingIdString ? iterator->current_value >= storage->productCount ?
                                                   iterator->current_value - storage->productCount :
                                                   sortingIndex->products[iterator->current_value] :
                                 iterator->current_value;
            result[4 + resultIndex] = productId;

            float price = priceAttributes[storage->productToIndex[productId]].value;
            if (price > maxPrice)
                maxPrice = price;
            if (price < minPrice)
                minPrice = price;

            if (isGrouped) {
                uint32_t productIndex = storage->productToIndex[productId];
                uint32_t groupId = storage->indexToGroup[productIndex];
                uint32_t groupOrder = storage->indexToGroupOrder[productIndex];
                if (groupOrder > storage->indexToGroupOrder[maxGroupOrderIndices[groupId]])
                    maxGroupOrderIndices[groupId] = productIndex;
            }
            roaring_advance_uint32_iterator(iterator);
        }
        roaring_free_uint32_iterator(iterator);
    }

    uint32_t groupCount = matchesCardinality;
    if (isGrouped) {
        for (uint32_t i = 0; i < matchesCardinality; i++) {
            uint32_t index = storage->productToIndex[result[4 + i]];
            if (index != maxGroupOrderIndices[storage->indexToGroup[index]]) {
                result[4 + i] = -1;
                groupCount--;
            }
        }
    }

    result[0] = minPrice;
    result[1] = maxPrice;
    result[2] = matchesCardinality;
    result[3] = groupCount;

    free(maxGroupOrderIndices);
    roaring_bitmap_free(matches);

    return (*env)->NewDirectByteBuffer(env, result, resultLength);
}

JNIEXPORT jobject JNICALL Java_ua_com_ubuntuzone_features_ProductFeaturesNativeRoaring_getSimilarProducts
        (JNIEnv *env, jclass class, jlong pointer, jint productId, jint maxProducts, jintArray extFeaturesArray) {

    jroaring_t *storage = (jroaring_t *) pointer;
    uint32_t productIndex = storage->productToIndex[productId];
    maxProducts = min(maxProducts, storage->productCount - 1);
    similar_product_t *similarProducts = malloc(sizeof(similar_product_t) * storage->productCount);
    memset(similarProducts, 0, sizeof(similar_product_t) * storage->productCount);
    uint32_t similarProductCount = 0;

    if (extFeaturesArray) {
        jsize extFeaturesCount = (*env)->GetArrayLength(env, extFeaturesArray);
        if (extFeaturesCount < 1)
            return 0;
        jint *extFeatures = (*env)->GetIntArrayElements(env, extFeaturesArray, NULL);
        if(!storage->featureProductsExt[extFeatures[0]])
            return 0;
        roaring_bitmap_t *matches = roaring_bitmap_copy(storage->featureProductsExt[extFeatures[0]]);
        for (uint32_t i = 0; i < extFeaturesCount; i++) {
            roaring_bitmap_t *bm = storage->featureProductsExt[extFeatures[i]];
            if(bm) {
                roaring_bitmap_and_inplace(matches, bm);
            } else {
                roaring_bitmap_free(matches);
                return 0;
            }
        }
        (*env)->ReleaseIntArrayElements(env, extFeaturesArray, extFeatures, JNI_ABORT);
        roaring_uint32_iterator_t *iterator = roaring_create_iterator(matches);
        uint32_t i = 0;
        while (iterator->has_value) {
            if(iterator->current_value != productId) {
                similarProducts[i].productId = iterator->current_value;
                similarProducts[i].hitPercent = round(roaring_bitmap_jaccard_index(
                        storage->productFeatures[productIndex],
                        storage->productFeatures[storage->productToIndex[iterator->current_value]]) * 100);
                i++;
            }
            roaring_advance_uint32_iterator(iterator);
        }
        similarProductCount = i;
        roaring_free_uint32_iterator(iterator);
        roaring_bitmap_free(matches);
    } else {
        uint32_t i;
        for (i = 0; i < productIndex; i++) {
            similarProducts[i].productId = storage->indexToProduct[i];
            similarProducts[i].hitPercent = round(roaring_bitmap_jaccard_index(
                    storage->productFeatures[productIndex], storage->productFeatures[i]) * 100);
        }
        for (i = productIndex + 1; i < storage->productCount; i++) {
            similarProducts[i].productId = storage->indexToProduct[i];
            similarProducts[i].hitPercent = round(roaring_bitmap_jaccard_index(
                    storage->productFeatures[productIndex], storage->productFeatures[i]) * 100);
        }
        similarProductCount = i;
    }

    qsort(similarProducts, similarProductCount, sizeof(similar_product_t), compareSimilar);
    uint32_t resultCount = min(similarProductCount, maxProducts);
    uint32_t *result = malloc(sizeof(uint32_t) * resultCount);
    for (uint32_t i = 0; i < resultCount; i++) {
        result[i] = similarProducts[i].productId;
    }
    free(similarProducts);

    return (*env)->NewDirectByteBuffer(env, result, sizeof(uint32_t) * resultCount);
}

JNIEXPORT jobject JNICALL Java_ua_com_ubuntuzone_features_ProductFeaturesNativeRoaring_countProducts
        (JNIEnv *env, jclass class, jlong pointer, jstring expressionString, jintArray includedFeaturesArray,
         jint tailItem, jboolean isGrouped, jobjectArray filterNamesArray, jfloatArray filterFromValuesArray,
         jfloatArray filterToValuesArray) {

    jroaring_t *storage = (jroaring_t *) pointer;

    roaring_bitmap_t *matches = roaring_bitmap_create();
    roaring_bitmap_t *groups = roaring_bitmap_create();
    getMatches(env, storage, expressionString, matches);
    applyFilters(env, storage, matches, filterNamesArray, filterFromValuesArray, filterToValuesArray);

    uint32_t *infos;
    uint32_t infoCount = 0;
    jsize includedFeatureCount = (*env)->GetArrayLength(env, includedFeaturesArray);
    if (includedFeatureCount > 0) {
        jboolean isCopy;
        jint *includedFeatures = (*env)->GetIntArrayElements(env, includedFeaturesArray, &isCopy);
        infos = malloc(sizeof(uint32_t) * 4 * includedFeatureCount);
        for (jsize i = 0; i < includedFeatureCount; i++) {
            if (storage->featureProducts[includedFeatures[i]]) {
                roaring_bitmap_t *bitmap = roaring_bitmap_and(matches, storage->featureProducts[includedFeatures[i]]);
                infos[infoCount * 4] = includedFeatures[i];
                infos[infoCount * 4 + 1] = roaring_bitmap_get_cardinality(bitmap);
                if (isGrouped)
                    infos[infoCount * 4 + 2] = getGroupSize(storage, bitmap, groups);
                else
                    infos[infoCount * 4 + 2] = 0;
                infos[infoCount * 4 + 3] = false;
                infoCount++;
                roaring_bitmap_free(bitmap);
            }
        }
        (*env)->ReleaseIntArrayElements(env, includedFeaturesArray, includedFeatures, JNI_ABORT);
    } else {
        infos = malloc(sizeof(uint32_t) * 4 * storage->featureCount);
        for (uint32_t i = 0; i < storage->featureCount; i++) {
            if (storage->featureProducts[i]) {
                roaring_bitmap_t *bitmap = roaring_bitmap_and(matches, storage->featureProducts[i]);
                infos[infoCount * 4] = i;
                infos[infoCount * 4 + 1] = roaring_bitmap_get_cardinality(bitmap);
                if (isGrouped)
                    infos[infoCount * 4 + 2] = getGroupSize(storage, bitmap, groups);
                else
                    infos[infoCount * 4 + 2] = 0;
                infos[infoCount * 4 + 3] = false;
                infoCount++;
                roaring_bitmap_free(bitmap);
            }
        }
    }

    roaring_bitmap_free(matches);
    roaring_bitmap_free(groups);

    return (*env)->NewDirectByteBuffer(env, infos, 16 * infoCount);
}

JNIEXPORT jobject JNICALL Java_ua_com_ubuntuzone_features_ProductFeaturesNativeRoaring_countAllProducts
        (JNIEnv *env, jclass class, jlong pointer, jboolean isGrouped) {

    jroaring_t *storage = (jroaring_t *) pointer;

    uint32_t *infos = malloc(sizeof(uint32_t) * 4 * storage->featureCount);
    for (uint32_t i = 0; i < storage->featureCount; i++) {
        infos[i * 4] = i;
        infos[i * 4 + 1] = storage->featureProducts[i] ? roaring_bitmap_get_cardinality(storage->featureProducts[i])
                                                       : 0;
        if (isGrouped)
            infos[i * 4 + 2] = roaring_bitmap_get_cardinality(storage->featureGroups[i]);
        else
            infos[i * 4 + 2] = 0;
        infos[i * 4 + 3] = false;
    }

    return (*env)->NewDirectByteBuffer(env, infos, 16 * storage->featureCount);
}

JNIEXPORT void JNICALL Java_ua_com_ubuntuzone_features_ProductFeaturesNativeRoaring_destroy
        (JNIEnv *env, jclass class, jlong pointer) {
    jroaring_t *storage = (jroaring_t *) pointer;
    clearStorage(storage);
    free(storage);
}