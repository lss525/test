#include <stdio.h>
#include <stdlib.h>

int main() {
    char file[100];
    scanf("%s", file);

    FILE *fp = fopen(file, "r");
    if (fp == NULL) {
        printf("无法打开文件 %s\n", file);
        return 1;
    }

    char buffer[1024];

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        printf("%s", buffer);
    }

    fclose(fp);
    return 0;
}