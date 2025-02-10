#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned int U32;
typedef unsigned long int U64;

typedef char I8;
typedef short I16;
typedef int I32;
typedef long int I64;

typedef unsigned char B8;

typedef float F32;
typedef double F64;

typedef struct String8
{
    I8 *data;
    U64 len;
} String8;

typedef enum JsonType
{
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT,
} JsonType;

typedef struct JsonArray
{
    struct JsonArrayBlock *head;
    struct JsonArrayBlock *last;
    U64 len;
} JsonArray;

typedef struct JsonValue
{
    JsonType type;
    union
    {
        struct JsonArray array;
        // TODO: use better data structure for objects, for now even number are keys, odd are the values...
        struct JsonArray object;
        String8 string;
        // TODO: support floating point numbers...
        U64 number;
        B8 boolean;
    };
} JsonValue;

#define JSON_ARRAY_BLOCK_SIZE 0x10
typedef struct JsonArrayBlock
{
    struct JsonArrayBlock *next;
    struct JsonValue values[JSON_ARRAY_BLOCK_SIZE];
} JsonArrayBlock;

typedef struct JsonParseResult
{
    JsonValue value;
} JsonParseResult;

typedef struct
{
    void *base_ptr;
    size_t reserved_size;
    size_t commit_size;
    size_t committed_size;
    size_t offset;
} ArenaAllocator;

JsonParseResult Json_Parse(ArenaAllocator *arena, String8 *string);

static const String8 NULL_STRING8 = {
    .data = "null",
    .len = sizeof("null") - 1,
};

static const String8 TRUE_STRING8 = {
    .data = "true",
    .len = sizeof("true") - 1,
};

static const String8 FALSE_STRING8 = {
    .data = "false",
    .len = sizeof("false") - 1,
};

#define TRUE 1
#define FALSE 0

void arena_create(size_t reserve_size, size_t commit_size, ArenaAllocator *arena)
{
    assert(commit_size <= reserve_size && "Commit size must be <= reserve size");

    arena->base_ptr = mmap(NULL, reserve_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (arena->base_ptr == NULL)
    {
        assert(FALSE && "TODO: handle mmap failed...");
        return;
    }

    arena->reserved_size = reserve_size;
    arena->commit_size = commit_size;
    arena->committed_size = 0;
    arena->offset = 0;
}

int commit_memory(ArenaAllocator *arena, size_t size)
{
    size_t new_commit_size = ((arena->offset + size + arena->commit_size - 1) / arena->commit_size) * arena->commit_size;

    if (new_commit_size > arena->reserved_size)
    {
        return 0;
    }

    if (new_commit_size > arena->committed_size)
    {
        size_t commit_increment = new_commit_size - arena->committed_size;

        if (mprotect((char *)arena->base_ptr + arena->committed_size, commit_increment, PROT_READ | PROT_WRITE) != 0)
        {
            long page_size = sysconf(_SC_PAGESIZE);
            if ((uintptr_t)((char *)arena->base_ptr + arena->committed_size) % page_size != 0)
            {
                printf("Error: Address is not page-aligned\n");
                exit(1);
            }

            return 0;
        }

        arena->committed_size = new_commit_size;
    }

    return 1;
}

// Function to allocate memory from the arena
void *arena_alloc(ArenaAllocator *arena, size_t size)
{
    if (arena->offset + size > arena->reserved_size)
    {
        return NULL;
    }

    if (!commit_memory(arena, size))
    {
        return NULL;
    }

    void *ptr = (char *)arena->base_ptr + arena->offset;
    arena->offset += size;
    return ptr;
}

// Function to reset the arena (without freeing reserved memory)
void arena_reset(ArenaAllocator *arena)
{
    arena->offset = 0;
}

// Function to free the entire arena
void arena_destroy(ArenaAllocator *arena)
{
    if (!arena)
        return;

    munmap(arena->base_ptr, arena->reserved_size);

    free(arena);
}

String8 Read_File_To_String(const I8 *filename)
{
    String8 result = {0};

    int fd = open(filename, O_RDONLY);
    if (fd == -1)
    {
        perror("open");
        return result;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1)
    {
        perror("fstat");
        close(fd);
        return result;
    }

    U64 file_size = sb.st_size;
    if (file_size == 0)
    {
        close(fd);
        return result;
    }

    U8 *mapped = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        return result;
    }

    I8 *file_contents = malloc(file_size + 1);
    if (!file_contents)
    {
        perror("malloc");
        munmap(mapped, file_size);
        close(fd);
        return result;
    }

    memcpy(file_contents, mapped, file_size);
    file_contents[file_size] = '\0';

    munmap(mapped, file_size);
    close(fd);
    result = (String8){
        .data = file_contents,
        .len = file_size,
    };
    return result;
}

B8 String8_Is_Prefix(String8 prefix, String8 string)
{
    if (string.len < prefix.len)
    {
        return FALSE;
    }

    for (U64 i = 0; i < prefix.len; i += 1)
    {
        if (prefix.data[i] != string.data[i])
        {
            return FALSE;
        }
    }

    return TRUE;
}

B8 String8_Compare(String8 a, String8 b)
{
    if (a.len != b.len)
    {
        return FALSE;
    }

    U64 i = 0;
    while (i < a.len)
    {
        if (a.data[i] != b.data[i])
        {
            return FALSE;
        }
        i += 1;
    }

    return TRUE;
}

void String8_Drop_First_N(U64 N, String8 *string)
{
    assert(N <= string->len);

    string->data += N;
    string->len -= N;
}

void String8_Drop_First(String8 *string)
{
    String8_Drop_First_N(1, string);
}

void String8_Trim_Whitespace_Left(String8 *string)
{
    while (string->len > 0 && string->data[0] == ' ')
    {
        String8_Drop_First(string);
    }
}

JsonParseResult Json_Parse_Bool(ArenaAllocator *arena, String8 *string)
{
    B8 value = JSON_BOOL;

    if (String8_Is_Prefix(TRUE_STRING8, *string))
    {
        value = TRUE;
        String8_Drop_First_N(TRUE_STRING8.len, string);
    }

    else if (String8_Is_Prefix(FALSE_STRING8, *string))
    {
        value = FALSE;
        String8_Drop_First_N(FALSE_STRING8.len, string);
    }

    else
    {
        assert(FALSE && "TODO: trying to parse invalid string as bool...");
    }

    return (JsonParseResult){
        .value = (JsonValue){
            .type = JSON_BOOL,
            .boolean = value,
        },
    };
}

JsonParseResult Json_Parse_Null(ArenaAllocator *arena, String8 *string)
{
    if (String8_Is_Prefix(NULL_STRING8, *string))
    {
        String8_Drop_First_N(NULL_STRING8.len, string);
    }

    else
    {
        assert(FALSE && "TODO: trying to parse invalid string as null...");
    }

    return (JsonParseResult){
        .value = (JsonValue){
            .type = JSON_NULL,
        },
    };
}

JsonParseResult Json_Parse_Number(ArenaAllocator *arena, String8 *string)
{
    JsonParseResult result = {0};
    result.value.type = JSON_NUMBER;
    result.value.number = 0;

    while (string->len > 0 && '0' <= string->data[0] && string->data[0] <= '9')
    {
        result.value.number *= 10;
        result.value.number += string->data[0] - '0';
        String8_Drop_First(string);
    }

    return result;
}

void Json_Array_Append(ArenaAllocator *arena, JsonArray *array, JsonValue value)
{
    // init...
    if (array->len == 0)
    {
        array->head = arena_alloc(arena, sizeof(JsonArrayBlock));
        array->head->next = NULL;
        array->last = array->head;
    }

    // if size is multiple of block size, add new block...
    else if (array->len % JSON_ARRAY_BLOCK_SIZE == 0)
    {
        JsonArrayBlock *new_block = arena_alloc(arena, sizeof(JsonArrayBlock));
        new_block->next = NULL;
        array->last->next = new_block;
        array->last = new_block;
    }

    array->last->values[array->len % JSON_ARRAY_BLOCK_SIZE] = value;
    array->len += 1;
}

JsonParseResult Json_Parse_Array(ArenaAllocator *arena, String8 *string)
{
    // consume the [
    String8_Drop_First(string);
    String8_Trim_Whitespace_Left(string);

    JsonArray array = {0};

    while (string->len > 0)
    {
        JsonParseResult result = Json_Parse(arena, string);
        Json_Array_Append(arena, &array, result.value);

        String8_Trim_Whitespace_Left(string);

        assert(string->len > 0);

        if (string->data[0] == ',')
        {
            String8_Drop_First(string);
        }

        else if (string->data[0] == ']')
        {
            String8_Drop_First(string);
            break;
        }

        else
        {
            assert(FALSE && "TODO: handle invalid character...");
        }

        String8_Trim_Whitespace_Left(string);
    }

    return (JsonParseResult){
        .value = (JsonValue){
            .type = JSON_ARRAY,
            .array = array,
        },
    };
}

JsonParseResult Json_Parse_String(ArenaAllocator *arena, String8 *string)
{
    String8_Drop_First(string);

    U64 len = 0;
    while (len < string->len && string->data[len] != '"')
    {
        len += 1;
    }

    char *data = arena_alloc(arena, len * sizeof(char));
    memcpy(data, string->data, len);

    String8 parsed_string = (String8){
        .data = data,
        .len = len,
    };

    String8_Drop_First_N(len + 1, string);

    return (JsonParseResult){
        .value = (JsonValue){
            .type = JSON_STRING,
            .string = parsed_string,
        },
    };
}

JsonParseResult Json_Parse_Object(ArenaAllocator *arena, String8 *string)
{
    // consume the {
    String8_Drop_First(string);

    JsonArray object = {0};

    while (string->len > 0)
    {
        String8_Trim_Whitespace_Left(string);

        JsonParseResult pair_key = Json_Parse(arena, string);
        assert(pair_key.value.type == JSON_STRING);
        Json_Array_Append(arena, &object, pair_key.value);

        String8_Trim_Whitespace_Left(string);

        assert(string->len > 0 && string->data[0] == ':');
        String8_Drop_First(string);

        String8_Trim_Whitespace_Left(string);

        JsonParseResult pair_value = Json_Parse(arena, string);
        Json_Array_Append(arena, &object, pair_value.value);

        String8_Trim_Whitespace_Left(string);

        assert(string->len > 0);

        if (string->data[0] == ',')
        {
            String8_Drop_First(string);
        }

        else if (string->data[0] == '}')
        {
            String8_Drop_First(string);
            break;
        }

        else
        {
            assert(FALSE && "TODO: handle invalid character...");
        }

        String8_Trim_Whitespace_Left(string);
    }

    return (JsonParseResult){
        .value = (JsonValue){
            .type = JSON_OBJECT,
            .object = object,
        },
    };
}

JsonParseResult Json_Parse(ArenaAllocator *arena, String8 *string)
{
    String8_Trim_Whitespace_Left(string);

    JsonParseResult result;

    assert(string->len > 0 && "TODO: trying to parse empty string...");

    switch (string->data[0])
    {
    case 't':
    case 'f':
        result = Json_Parse_Bool(arena, string);
        break;

    case 'n':
        result = Json_Parse_Null(arena, string);
        break;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
        result = Json_Parse_Number(arena, string);
        break;

    case '"':
        result = Json_Parse_String(arena, string);
        break;

    case '[':
        result = Json_Parse_Array(arena, string);
        break;

    case '{':
        result = Json_Parse_Object(arena, string);
        break;

    default:
        assert(FALSE && "TODO");
        break;
    }

    return result;
}

int main(void)
{
    String8 input = Read_File_To_String("input.json");
    ArenaAllocator arena = {0};
    // NOTE: commit size to be multiple of page size...
    arena_create(1024 * 1024, 16384, &arena);
    JsonParseResult result = Json_Parse(&arena, &input);

    JsonArray array = result.value.array;
    JsonArrayBlock *curr = array.head;
    U64 i = 0;

    while (i < array.len)
    {
        JsonValue value = curr->values[i % JSON_ARRAY_BLOCK_SIZE];

        switch (value.type)
        {
        case JSON_NULL:
            printf("null, ");
            break;

        case JSON_BOOL:
            if (value.boolean)
            {
                printf("true, ");
            }
            else
            {
                printf("false, ");
            }
            break;

        case JSON_NUMBER:
            printf("%ld, ", value.number);
            break;

        case JSON_STRING:
            printf("%*s, ", (int)value.string.len, value.string.data);
            break;

        case JSON_ARRAY:
            printf("[nested array], ");
            break;

        case JSON_OBJECT:
            printf("[nested object], ");
            break;

        default:
            printf("[unknown tag], ");
            break;
        }

        i += 1;
        if (i % JSON_ARRAY_BLOCK_SIZE == 0)
        {
            curr = curr->next;
        }
    }

    return 0;
}
