#include <stdio.h>

int is_within(char ch, char *str) {
    while (*str != '\0') {
        if (*str == ch) {
            return 1;
        }
        str++;
    }
    return 0;
}

int main() {
    char ch;
    char str[100];
    scanf("%s", str);
    while (1) {
        scanf(" %c", &ch);
        if (ch == '#') {
            break;
        }
        if (is_within(ch, str)) {
            printf("字符 '%c' 在字符串中\n", ch);
        } else {
            printf("字符 '%c' 不在字符串中\n", ch);
        }
    }
    return 0;
}