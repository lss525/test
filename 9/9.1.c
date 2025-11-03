#include<stdio.h>
void exchange(double* a,double* b,double* c){
    double max=(*a>*b)?((*a>*c)?*a:*c):((*b>*c)?*b:*c);
    double min=(*b>*a)?((*c>*a)?*a:*c):((*c>*b)?*b:*c);
    double temp=*a;
           *a=max;
           max=temp;
    double temp2=*c;
           *c=min;
           min=temp2;
}
int main(){
    double a,b,c;
    scanf("%lf %lf %lf",&a,&b,&c);
    exchange(&a,&b,&c);
    printf("%lf %lf %lf",a,b,c);
    return 0;
}