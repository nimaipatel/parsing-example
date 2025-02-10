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
    String8 string;
} JsonParseResult;

JsonParseResult Json_Parse(String8 string);

static const String8 NULL_STR = {
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

String8 String8_Drop_First_N(U64 N, String8 string)
{
    assert(N <= string.len);

    string.data += N;
    string.len -= N;

    return string;
}

String8 String8_Drop_First(String8 string)
{
    return String8_Drop_First_N(1, string);
}

String8 String8_Trim_Whitespace_Left(String8 string)
{
    while (string.len > 0 && string.data[0] == ' ')
    {
        string = String8_Drop_First(string);
    }

    return string;
}

JsonParseResult Json_Parse_Bool(String8 string)
{
    B8 value = JSON_BOOL;

    if (String8_Is_Prefix(TRUE_STRING8, string))
    {
        value = TRUE;
        string = String8_Drop_First_N(TRUE_STRING8.len, string);
    }

    else if (String8_Is_Prefix(FALSE_STRING8, string))
    {
        value = FALSE;
        string = String8_Drop_First_N(FALSE_STRING8.len, string);
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
        .string = string,
    };
}

JsonParseResult Json_Parse_Null(String8 string)
{
    if (String8_Is_Prefix(NULL_STR, string))
    {
        string = String8_Drop_First_N(NULL_STR.len, string);
    }

    else
    {
        assert(FALSE && "TODO: trying to parse invalid string as null...");
    }

    return (JsonParseResult){
        .value = (JsonValue){
            .type = JSON_NULL,
        },
        .string = string,
    };
}

JsonParseResult Json_Parse_Number(String8 string)
{
    JsonParseResult result = {0};
    result.value.type = JSON_NUMBER;
    result.value.number = 0;

    while (string.len > 0 && '0' <= string.data[0] && string.data[0] <= '9')
    {
        result.value.number *= 10;
        result.value.number += string.data[0] - '0';

        string = String8_Drop_First(string);
    }

    result.string = string;
    return result;
}

void Json_Array_Append(JsonArray *array, JsonValue value)
{
    // init...
    if (array->len == 0)
    {
        array->head = malloc(sizeof(JsonArrayBlock));
        array->head->next = NULL;
        array->last = array->head;
    }

    // if size is multiple of block size, add new block...
    else if (array->len % JSON_ARRAY_BLOCK_SIZE == 0)
    {
        JsonArrayBlock *new_block = malloc(sizeof(JsonArrayBlock));
        new_block->next = NULL;
        array->last->next = new_block;
        array->last = new_block;
    }

    array->last->values[array->len % JSON_ARRAY_BLOCK_SIZE] = value;
    array->len += 1;
}

JsonParseResult Json_Parse_Array(String8 string)
{
    // consume the [
    string = String8_Drop_First(string);
    string = String8_Trim_Whitespace_Left(string);

    JsonArray array = {0};

    while (string.len > 0)
    {
        JsonParseResult result = Json_Parse(string);
        Json_Array_Append(&array, result.value);

        string = result.string;

        string = String8_Trim_Whitespace_Left(string);

        assert(string.len > 0);

        if (string.data[0] == ',')
        {
            string = String8_Drop_First(string);
        }

        else if (string.data[0] == ']')
        {
            string = String8_Drop_First(string);
            break;
        }

        else
        {
            assert(FALSE && "TODO: handle invalid character...");
        }

        string = String8_Trim_Whitespace_Left(string);
    }

    return (JsonParseResult){
        .value = (JsonValue){
            .type = JSON_ARRAY,
            .array = array,
        },
        .string = string,
    };
}

JsonParseResult Json_Parse_String(String8 string)
{
    string = String8_Drop_First(string);

    U64 len = 0;
    while (len < string.len && string.data[len] != '"')
    {
        len += 1;
    }

    char *data = malloc(len * sizeof(char));
    memcpy(data, string.data, len);

    String8 parsed_string = (String8){
        .data = data,
        .len = len,
    };

    string = String8_Drop_First_N(len + 1, string);

    return (JsonParseResult){
        .value = (JsonValue){
            .type = JSON_STRING,
            .string = parsed_string,
        },
        .string = string,
    };
}

JsonParseResult Json_Parse_Object(String8 string)
{
    // consume the {
    string = String8_Drop_First(string);

    JsonArray object = {0};

    while (string.len > 0)
    {
        string = String8_Trim_Whitespace_Left(string);

        JsonParseResult pair_key = Json_Parse(string);
        string = pair_key.string;
        assert(pair_key.value.type == JSON_STRING);
        Json_Array_Append(&object, pair_key.value);

        string = String8_Trim_Whitespace_Left(string);

        assert(string.len > 0 && string.data[0] == ':');
        string = String8_Drop_First(string);

        string = String8_Trim_Whitespace_Left(string);

        JsonParseResult pair_value = Json_Parse(string);
        string = pair_value.string;
        Json_Array_Append(&object, pair_value.value);

        string = String8_Trim_Whitespace_Left(string);

        assert(string.len > 0);

        if (string.data[0] == ',')
        {
            string = String8_Drop_First(string);
        }

        else if (string.data[0] == '}')
        {
            string = String8_Drop_First(string);
            break;
        }

        else
        {
            assert(FALSE && "TODO: handle invalid character...");
        }

        string = String8_Trim_Whitespace_Left(string);
    }

    return (JsonParseResult){
        .value = (JsonValue){
            .type = JSON_OBJECT,
            .object = object,
        },
        .string = string,
    };
}

JsonParseResult Json_Parse(String8 string)
{
    string = String8_Trim_Whitespace_Left(string);

    JsonParseResult result;

    assert(string.len > 0 && "TODO: trying to parse empty string...");

    switch (string.data[0])
    {
    case 't':
    case 'f':
        result = Json_Parse_Bool(string);
        break;

    case 'n':
        result = Json_Parse_Null(string);
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
        result = Json_Parse_Number(string);
        break;

    case '"':
        result = Json_Parse_String(string);
        break;

    case '[':
        result = Json_Parse_Array(string);
        break;

    case '{':
        result = Json_Parse_Object(string);
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
    JsonParseResult result = Json_Parse(input);

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
        case JSON_OBJECT:
        default:
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
