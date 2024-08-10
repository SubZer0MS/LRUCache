#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define HASH_MAP_SIZE 1048573
#define MAX_CAPACITY 7000000
#define MAX_KEY_LENGTH 100
#define MAX_VALUE_LENGTH 4096

typedef char* PCHAR;
typedef void* PVOID;
typedef unsigned int UINT;
typedef unsigned char BYTE;
typedef BYTE* PBYTE;

typedef struct _NODE {
    UINT hashKey;
    PCHAR key;
    PVOID serializedValue;
    size_t valueSize;
    struct _NODE* prev;
    struct _NODE* next;
    struct _NODE* hnext;
} NODE, *PNODE;

typedef struct _LRUCACHE {
    UINT capacity;
    UINT size;
    size_t sizeInBytes;
    PNODE head;
    PNODE tail;
    PNODE* nodes;
} LRUCACHE, *PLRUCACHE;

PVOID LRUSerializeData(PVOID value, size_t dataSize)
{
    void* serializedData = malloc(dataSize);

    if (serializedData)
    {
        memcpy(serializedData, value, dataSize);
    }

    return serializedData;
}

PVOID LRUDeserializeData(PVOID serializedData, size_t dataSize)
{
    void* value = malloc(dataSize);

    if (value)
    {
        memcpy(value, serializedData, dataSize);
    }

    return value;
}

UINT LRUCacheCreateHash(PCHAR str)
{
    int c;
    UINT hash = 5381;

    while ((c = *str++))
    {
        hash = ((hash << 5) + hash) + c;
    }

    hash ^= (hash >> 16);

    return hash;
}

UINT LRUCacheGetHashIndex(PCHAR key)
{
    return LRUCacheCreateHash(key) % HASH_MAP_SIZE;
}

PLRUCACHE LRUCacheCreate(UINT capacity)
{
    PLRUCACHE cache = (PLRUCACHE)malloc(sizeof(LRUCACHE));

    if (!cache)
    {
        return NULL;
    }

    cache->capacity = capacity;
    cache->size = 0;
    cache->sizeInBytes = HASH_MAP_SIZE * sizeof(PNODE);
    cache->head = NULL;
    cache->tail = NULL;
    cache->nodes = (PNODE*)calloc(HASH_MAP_SIZE, sizeof(PNODE));

    if (!cache->nodes)
    {
        free(cache);

        return NULL;
    }

    return cache;
}

PNODE LRUCacheNodeCreate(PCHAR key, PVOID value, size_t size)
{
    PNODE node = (PNODE)malloc(sizeof(NODE));

    if (!node)
    {
        return NULL;
    }

    node->hashKey = LRUCacheCreateHash(key);
    node->key = strdup(key);

    if (!node->key)
    {
        free(node);

        return NULL;
    }

    node->serializedValue = value;
    node->valueSize = size;
    node->prev = NULL;
    node->next = NULL;
    node->hnext = NULL;

    return node;
}

int LRUCacheFreeNode(PNODE node)
{
    free(node->serializedValue);
    free(node->key);
    free(node);

    return 0;
}

int LRUCacheFree(PLRUCACHE cache)
{
    PNODE node = cache->head;
    PNODE next = NULL;

    while (node)
    {
        next = node->next;
        LRUCacheFreeNode(node);
        node = next;
    }

    free(cache->nodes);
    free(cache);

    return 0;
}

int LRUCacheMoveNodeToHead(PLRUCACHE cache, PNODE node)
{
    if (node == cache->head)
    {
        return 0;
    }

    if (node == cache->tail)
    {
        cache->tail = node->prev;
    }

    if (node->prev)
    {
        node->prev->next = node->next;
    }

    if (node->next)
    {
        node->next->prev = node->prev;
    }

    node->prev = NULL;
    node->next = cache->head;

    if (cache->head)
    {
        cache->head->prev = node;
    }

    cache->head = node;

    if (!cache->tail)
    {
        cache->tail = node;
    }

    return 0;
}

int LRUCacheRemoveTail(PLRUCACHE cache)
{
    PNODE node = cache->tail;

    if (!node)
    {
        return -1;
    }

    if (node == cache->head)
    {
        cache->head = NULL;
        cache->tail = NULL;
    }
    else
    {
        cache->tail = node->prev;
        cache->tail->next = NULL;
    }

    UINT index = LRUCacheGetHashIndex(node->key);

    if (cache->nodes[index] == node)
    {
        cache->nodes[index] = NULL;
    }
    else
    {
        PNODE prev = cache->nodes[index];
        while (prev->next != node)
        {
            prev = prev->next;
        }
        prev->next = node->next;
    }

    cache->size--;
    cache->sizeInBytes -= (sizeof(NODE) + strlen(node->key) + 1 + node->valueSize);

    LRUCacheFreeNode(node);

    return 0;
}

PNODE LRUCacheGet(PLRUCACHE cache, PCHAR key)
{
    UINT index = LRUCacheGetHashIndex(key);
    PNODE node = cache->nodes[index];

    while (node)
    {
        if (strncmp(node->key, key, MAX_KEY_LENGTH) == 0)
        {
            LRUCacheMoveNodeToHead(cache, node);

            return node;
        }

        node = node->hnext;
    }

    return NULL;
}

int LRUCachePut(PLRUCACHE cache, PCHAR key, PVOID value, size_t size)
{
    if (size <= 0)
    {
        return -1;
    }

    PVOID serializedValue = LRUSerializeData(value, size);

    if (!serializedValue)
    {
        return -1;
    }

    PNODE node = LRUCacheGet(cache, key);

    if (node)
    {
        cache->sizeInBytes -= node->valueSize;
        cache->sizeInBytes += size;

        free(node->serializedValue);
        node->serializedValue = serializedValue;

        LRUCacheMoveNodeToHead(cache, node);

        return 0;
    }

    node = LRUCacheNodeCreate(key, serializedValue, size);

    if (!node)
    {
        free(serializedValue);

        return -1;
    }

    if (cache->size == cache->capacity)
    {
        LRUCacheRemoveTail(cache);
    }

    UINT index = LRUCacheGetHashIndex(key);
    node->hnext = cache->nodes[index];
    cache->nodes[index] = node;

    node->next = cache->head;

    if (cache->head)
    {
        cache->head->prev = node;
    }

    cache->head = node;

    if (!cache->tail)
    {
        cache->tail = node;
    }

    cache->size++;
    cache->sizeInBytes += sizeof(NODE) + strlen(key) + 1 + size;

    return 0;
}

int LRUCacheRemove(PLRUCACHE cache, PCHAR key)
{
    PNODE node = LRUCacheGet(cache, key);

    if (!node)
    {
        return -1;
    }

    if (node->prev)
    {
        node->prev->next = node->next;
    }
    else
    {
        cache->head = node->next;
    }

    if (node->next)
    {
        node->next->prev = node->prev;
    }
    else
    {
        cache->tail = node->prev;
    }

    UINT hashIndex = LRUCacheGetHashIndex(key);

    PNODE prev = NULL;
    PNODE curr = cache->nodes[hashIndex];

    while (curr)
    {
        if (curr == node)
        {
            if (prev)
            {
                prev->hnext = curr->hnext;
            }
            else
            {
                cache->nodes[hashIndex] = curr->hnext;
            }

            break;
        }

        prev = curr;
        curr = curr->hnext;
    }

    LRUCacheFreeNode(node);

    cache->size--;
    cache->sizeInBytes -= (sizeof(NODE) + strlen(node->key) + 1 + node->valueSize);

    return 0;
}

void LRUDisplayCache(PLRUCACHE cache)
{
    PNODE node = cache->head;

    printf("Cache size: %u, Capacity: %u, Cache size in bytes: %zu\n", cache->size, cache->capacity, cache->sizeInBytes);

    while (node)
    {
        printf("Key: %s, Value: ", node->key);
        
        PBYTE data = (PBYTE)node->serializedValue;

        for (size_t i = 0; i < node->valueSize; i++)
        {
            printf("%02X ", data[i]);
        }

        printf("\n");

        node = node->next;
    }
}

// cahe definition ends here

void generateRandomString(PCHAR str, size_t size)
{
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    for (size_t i = 0; i < size - 1; i++)
    {
        str[i] = charset[rand() % (sizeof(charset) - 1)];
    }

    str[size - 1] = '\0';
}

void testLRUCache()
{
    srand((UINT)time(NULL));

    PLRUCACHE cache = LRUCacheCreate(500);
    if (!cache) {
        printf("Failed to create LRU cache.\n");
        return;
    }

    for (int i = 0; i < 1111; ++i) {
        char key[MAX_KEY_LENGTH];
        char value[MAX_VALUE_LENGTH];

        generateRandomString(key, sizeof(key));
        generateRandomString(value, sizeof(value));

        if (LRUCachePut(cache, key, strdup(value), strlen(value) + 1) == -1)
        {
            printf("Failed to put key: %s into cache.\n", key);
        }
    }

    for (int i = 0; i < 10; ++i)
    {
        char key[MAX_KEY_LENGTH];
        generateRandomString(key, sizeof(key));

        PVOID value = LRUCacheGet(cache, key);
        if (value)
        {
            printf("Key '%s' retrieved from cache.\n", key);
        }
        else
        {
            printf("Key '%s' not found in cache.\n", key);
        }
    }

    printf("\nCurrent cache contents:\n");
    LRUDisplayCache(cache);

    for (int i = 0; i < 10; ++i)
    {
        char key[MAX_KEY_LENGTH];
        generateRandomString(key, sizeof(key));

        if (LRUCacheRemove(cache, key) == 0)
        {
            printf("Key '%s' removed from cache.\n", key);
        }
        else
        {
            printf("Failed to remove key '%s' from cache.\n", key);
        }
    }

    printf("\nCache contents after removals:\n");
    LRUDisplayCache(cache);

    LRUCacheFree(cache);
    printf("LRU cache freed.\n");
}

int main()
{
    UINT capacity = 0;

    printf("Enter capacity (max %d): ", MAX_CAPACITY);

    while (scanf("%u", &capacity) != 1 || capacity > MAX_CAPACITY || capacity == 0)
    {
        printf("Invalid capacity. Please enter a positive number less than or equal to %d.\n", MAX_CAPACITY);

        scanf("%*[^\n]");
        getchar();
    }

    PLRUCACHE cache = LRUCacheCreate(capacity);

    if (!cache)
    {
        printf("Failed to create LRU cache.\n");

        return -1;
    }

    int option = 0;

    while (1)
    {
        printf("\nChoose an option:\n");
        printf("1. Display current LRU Cache\n");
        printf("2. Add key-value pair to LRU Cache\n");
        printf("3. Get value by key from LRU Cache\n");
        printf("4. Test adding some custom data structure to LRU Cache\n");
        printf("5. Remove by key from LRU Cache\n");
        printf("6. Test LRU Cache\n");
        printf("0. Exit\n");
        printf("Enter your choice: ");

        if (scanf("%d", &option) != 1)
        {
            printf("Invalid input. Please enter a number.\n");

            scanf("%*[^\n]");
            getchar();
            
            continue;
        }

        scanf("%*[^\n]");
        getchar();

        switch (option)
        {
            case 1:

                printf("Displaying LRU Cache:\n");
                LRUDisplayCache(cache);

                break;

            case 2:
                {
                    char key[MAX_KEY_LENGTH + 1] = {0};
                    char value[MAX_VALUE_LENGTH + 1] = {0};

                    printf("Enter key,value pair: ");

                    if (fgets(key, sizeof(key), stdin) == NULL)
                    {
                        printf("Failed to read input.\n");

                        continue;
                    }

                    PCHAR comma = strchr(key, ',');

                    if (!comma)
                    {
                        printf("Invalid input format. Please enter in 'key,value' format.\n");

                        continue;
                    }

                    *comma = '\0';
                    strncpy(value, comma + 1, MAX_VALUE_LENGTH);

                    value[strcspn(value, "\n")] = 0;

                    char* endptr;
                    long val = strtol(value, &endptr, 10);

                    if (*endptr != '\0' || val < 0 || val > UINT_MAX)
                    {
                        printf("Invalid value. Please enter a valid positive integer.\n");

                        continue;
                    }

                    LRUCachePut(cache, key, &val, sizeof(UINT));
                    printf("Added key-value pair to LRU Cache.\n");
                    LRUDisplayCache(cache);

                    break;
                }

            case 3:
                {
                    char key[MAX_KEY_LENGTH + 1] = {0};

                    printf("Enter key to get value: ");

                    if (fgets(key, sizeof(key), stdin) == NULL)
                    {
                        printf("Failed to read input.\n");

                        continue;
                    }

                    key[strcspn(key, "\n")] = 0;

                    PNODE node = LRUCacheGet(cache, key);

                    if (node)
                    {
                        printf("Key '%s' has value: %u\n", key, (UINT)(uintptr_t)node->serializedValue);
                    }
                    else
                    {
                        printf("Key '%s' not found in LRU Cache.\n", key);
                    }

                    LRUDisplayCache(cache);

                    break;
                }

            case 4:
                {
                    typedef struct {
                        UINT id;
                        char name[20];
                        PCHAR description;
                    } CUSTOM_STRUCT;

                    CUSTOM_STRUCT customData = {
                        1,
                        "Custom Data",
                        "This is a custom data structure."
                    };

                    char key[MAX_KEY_LENGTH + 1] = {0};

                    printf("Enter key to get value: ");

                    if (fgets(key, sizeof(key), stdin) == NULL)
                    {
                        printf("Failed to read input.\n");

                        continue;
                    }

                    key[strcspn(key, "\n")] = 0;

                    LRUCachePut(cache, key, &customData, sizeof(customData));
                    
                    printf("Added custom type \"CUSTOM_STRUCT\" key-value pair to LRU Cache.\n");

                    PNODE node = LRUCacheGet(cache, key);

                    customData = *(CUSTOM_STRUCT*)node->serializedValue;

                    if (node)
                    {
                        printf("Key '%s' has value converted of CUSTOM_STRUCT - here the struct:\n\tid: %d\n\tname: %s\n\tdescription: %s\n", key, customData.id, customData.name, customData.description);
                    }
                    else
                    {
                        printf("Key '%s' not found in LRU Cache.\n", key);
                    }

                    LRUDisplayCache(cache);

                    break;
                }

            case 5:
                {
                    char key[MAX_KEY_LENGTH + 1] = {0};

                    printf("Enter key to remove: ");

                    if (fgets(key, sizeof(key), stdin) == NULL)
                    {
                        printf("Failed to read input.\n");

                        continue;
                    }

                    key[strcspn(key, "\n")] = 0;

                    if (LRUCacheRemove(cache, key) == 0)
                    {
                        printf("Key '%s' removed from LRU Cache.\n", key);
                    }
                    else
                    {
                        printf("Key '%s' not found in LRU Cache.\n", key);
                    }

                    LRUDisplayCache(cache);

                    break;
                }

            case 6:

                testLRUCache();

                break;

            case 0:

                LRUCacheFree(cache);
                printf("Exiting program.\n");

                return 0;

            default:

                printf("Invalid option. Please choose a valid option.\n");

                break;
        }
    }

    return 0;
}