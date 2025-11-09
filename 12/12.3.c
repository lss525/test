#include <stdio.h>
#include <stdlib.h>
#include <time.h>
int main() {
    int counts[10 + 1] = {0}; 
    int seed;
    for (seed = 1; seed <= 10; seed++) {
    
        for (int i = 1; i <= 10; i++) {
            counts[i] = 0;
        }
        srand(seed); 
        for (int i = 0; i < 1000; i++) {
            int randNum = rand() % 10 + 1; 
            counts[randNum]++;
        }
        printf("%d：\n", seed);
        for (int i = 1; i <= 10; i++) {
            printf("%d：%d\n", i, counts[i]);
        }
        printf("\n");
    }
    return 0;
}