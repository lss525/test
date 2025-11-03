#include <stdio.h>
#include <string.h>
int main() {
    char input[1000];
    int count = 0;
 while(1){
        char c = getchar();
        if (c == '#') {
            break;
        }
        input[count++] = c;
    }
    input[count] = '\0'; 
    int replaceCount = 0;
    for (int i = 0; i < count; i++) {
        switch (input[i]) {
            case '!':
                if (i + 1 < count && input[i + 1] == '!') {
                    i++;
                } else {
                    for (int j = count; j > i; j--) {
                        input[j] = input[j - 1];
                    }
                    input[i + 1] = '!';
                    count++;
                    replaceCount++;
                }
                break;
            case '.':
                input[i] = '!';
                replaceCount++;
                break;
            default:
                break;
        }
    }
   
    printf("%d\n", replaceCount);
    return 0;
}