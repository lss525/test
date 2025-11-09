#include <stdio.h>
int countSetBits(int n) {
    int count = 0;
    while (n != 0) {
        count += n & 1;
        n >>= 1;
    }
    return count;
}
int main() {
    int num;
    scanf("%d", &num);
    int result = countSetBits(num);
    printf("%d %d\n", num, result);
    return 0;
}