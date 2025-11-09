#include <stdio.h>
#define HARMONIC(a, b) (2 * (a) * (b) / ((a) + (b)))

int main() {
    int num1, num2;
    scanf("%d", &num1);
    scanf("%d", &num2);
    int result = HARMONIC(num1, num2);
    printf("%d 和 %d %d\n", num1, num2, result);
    return 0;
}