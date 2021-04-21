#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILENAME "write_file.txt"
void write(int i)
{
    char str[4] = {};
    FILE *fp = fopen(FILENAME, "r+");
    sprintf(str, "%d", i);
    fwrite(str, 1, strlen(str), fp);
    fclose(fp);
}
int main()
{
    int i = 0;
    while (i < 100)
    {
        write(i);
        i++;
        printf("%d\n", i);
        usleep(10000);
    }
}