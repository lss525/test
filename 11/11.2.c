#include <stdio.h>
#include <ctype.h>

void readWordWithLimit(char str[], int n) {
    int ch, i = 0;
    while ((ch = getchar()) != EOF && isspace(ch)) {
        continue;
    }
    if (ch != EOF && !isspace(ch)) {
        while (i < n - 1 && ch != EOF && !isspace(ch)) {
            str[i++] = ch;
            ch = getchar();
        }
    }
    str[i] = '\0';
    while (ch != EOF && !isspace(ch)) {
        ch = getchar();
    }
}

int main() {
    char word[100];
    int limit;
    scanf("%d", &limit);
    getchar();
    readWordWithLimit(word, limit);
    printf("读取的单词：%s\n", word);
    return 0;
}