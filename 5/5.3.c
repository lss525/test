#include<stdio.h>
void temperrature(double n){
    printf("%.2lf\n",n);
    printf("%.2lf\n",5.0/9.0*(n-32.0));
    printf("%.2lf\n",5.0/9.0*(n-32.0)+273.16);
}
int main(){
    double n=0.00;
    while(1){
        printf("scanf t :");
        int x=scanf("%lf",&n);
        if(x!=1){
            break;
        }
        else{
            temperrature((double)n);
            continue;
        }
    }
    return 0;
}