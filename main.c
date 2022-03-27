#include <stdio.h>
#include "ttyio.h"

int main(int argc, char *argv[]) {
    int i = 0;
    PRINTF("Hello World %f\r\n", 25.2f);
    PRINTF("%c %d %p %x %l  %f", 'A', 124, &i, 0x12345678, 23523245, 234.23f);
    return 0;
}