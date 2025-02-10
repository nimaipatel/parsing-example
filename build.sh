#!/bin/bash

gcc -O0 -Werror -Wall -Wpedantic -Wextra -Wno-unused-function  -Wno-unused-variable -Wno-unused-parameter  -ggdb main.c -o ./main
# gcc -O3 -Werror -Wall -Wpedantic -Wextra -Wno-unused-function  -Wno-unused-variable -Wno-unused-parameter  main.c -o ./main