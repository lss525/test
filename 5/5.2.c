#include<stdio.h>
double lifang(double n){
    return n*n*n;
}
int main(){
    double n;
    scanf("%lf",&n);
    double z=lifang(n);
    printf("%.2lf",z);
    return 0;
}