#include<stdio.h>
int main(){
    double n;
    scanf("%lf",&n);
    printf("Enter a floating-point value: %f\n",n);
    printf("fixed-point notation: %.6f\n",n);
    printf("exponential notation:%e\n",n);
    printf("p notation:%a",n);
    return 0;
}