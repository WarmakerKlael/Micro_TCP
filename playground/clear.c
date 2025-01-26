#include <stdio.h>

int main()
{
    printf("\033[3J\033[2J\033[H");
    system("clear");
    printf("Screen cleared using ANSI escape codes!\n");
    return 0;
}
