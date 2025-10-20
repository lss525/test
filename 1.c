#include<stdio.h>
int main(){
    printf("输入英寸值n：");
    int n=0;
    scanf("%d",&n);
    double cm=(double)n*2.54;
    printf("%.2lf",cm);
    return 0;
}