    JsonArray array = {0};

    for (int i = 0; i < 100; i += 1)
    {
        JsonValue value = (JsonValue) {
            .type = JSON_NUMBER,
            .number = i,
        };
        Json_Array_Append(&array, value);
    }

    JsonArrayBlock *curr = array.head;
    U64 i = 0;

    while (i < array.len)
    {
        JsonValue value = curr->values[i % JSON_ARRAY_BLOCK_SIZE];

        printf("%ld\n", value.number);

        i += 1;
        if (i % JSON_ARRAY_BLOCK_SIZE == 0)
        {
            curr = curr->next;
        }
    }
