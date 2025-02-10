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