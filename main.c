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
    JSON_BOOL,
    JSON_NULL,
    JSON_NUMBER,
    JSON_ARRAY,
} JsonType;

typedef struct JsonValue
{
    JsonType type;
    union
    {
        struct
        {
            struct JsonArrayBlock *head;
            U64 len;
        } array;

        String8 string;

        U64 number; // TODO: support floating point numbers...

        B8 boolean;
    };
} JsonValue;

typedef struct JsonArrayBlock
{
    JsonValue values[0x10];
    struct JsonArrayBlock *next;
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

static const String8 TRUE_STR = {
    .data = "true",
    .len = sizeof("true") - 1,
};

static const String8 FALSE_STR = {
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
    { // Handle empty files
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
    file_contents[file_size] = '\0'; // Null-terminate the string

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

B8 String8_Compare_Literal(String8 a, I8 *b)
{
    if (a.len != sizeof(b))
    {
        return FALSE;
    }

    U64 i = 0;
    while (i < a.len)
    {
        if (a.data[i] != b[i])
        {
            return FALSE;
        }
        i += 1;
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

String8 Json_Skip_Whitespace(String8 string)
{
    while (string.len > 0 && string.data[0] == ' ')
    {
        string.len -= 1;
        string.data += 1;
    }

    return string;
}

JsonParseResult Json_Parse_Bool(String8 string)
{
    JsonParseResult result = {0};
    result.value.type = JSON_BOOL;
    result.string = string;

    if (String8_Is_Prefix(TRUE_STR, string))
    {
        result.value.boolean = TRUE;
        result.string.data += TRUE_STR.len;
        result.string.len -= TRUE_STR.len;
    }

    else if (String8_Is_Prefix(FALSE_STR, string))
    {
        result.value.boolean = FALSE;
        result.string.data += FALSE_STR.len;
        result.string.len -= FALSE_STR.len;
    }

    else
    {
        assert(FALSE && "TODO: trying to parse invalid string as bool...");
    }

    return result;
}

JsonParseResult Json_Parse_Null(String8 string)
{
    JsonParseResult result = {0};
    result.value.type = JSON_NULL;
    result.string = string;

    if (String8_Is_Prefix(NULL_STR, string))
    {
        result.string.data += NULL_STR.len;
        result.string.len -= NULL_STR.len;
    }

    else
    {
        assert(FALSE && "TODO: trying to parse invalid string as null...");
    }

    return result;
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

        string.data += 1;
        string.len -= 1;
    }

    result.string = string;
    return result;
}

JsonParseResult Json_Parse_Array(String8 string)
{
    JsonParseResult result = {0};
    // consume the [
    string.data += 1;
    string.len -= 1;

    string = Json_Skip_Whitespace(string);

    while (string.len > 0)
    {
        JsonParseResult result = Json_Parse(string);
        (void)result.value; // TODO: add to the array object...

        string = result.string;

        string = Json_Skip_Whitespace(string);

        assert(string.len > 0);

        if (string.data[0] == ',')
        {
            string.data += 1;
            string.len -= 1;
        }

        else if (string.data[0] == ']')
        {
            string.data += 1;
            string.len -= 1;
            break;
        }

        else
        {
            assert(FALSE && "TODO: handle invalid character...");
        }

        string = Json_Skip_Whitespace(string);
    }

    string.data += 1;
    string.len -= 1;

    return result;
}

JsonParseResult Json_Parse(String8 string)
{
    string = Json_Skip_Whitespace(string);

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

    case '[':
        result = Json_Parse_Array(string);
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
    (void)result;
    return 0;
}
