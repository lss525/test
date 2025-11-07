#include<stdio.h>
void copy1(double brr1[],double arr[],int n){
    for(int i=0;i<n;i++){
        brr1[i]=arr[i];
    }
}
void copy2(double brr2[],double arr[],int n){
    double* p = &arr[0];
    int count=0;
    while(count<n){
        brr2[count++]=*(p++);
    }
}
void copy3(double brr3[],double arr[],double *p){
    double *pp=&arr[0];

    while(*pp!=*p){
        static int count=0;
        brr3[count++]=*(pp++);
    }

}
int main(){
    double arr[5]={1.1,2.2,3.3,4.4,5.5};
    double brr1[5],brr2[5],brr3[5];
    copy1(brr1,arr,5);
    copy2(brr2,arr,5);
    copy3(brr3,arr,arr+5);
    for(int i=0;i<5;i++){
        printf("%.1lf ",brr1[i]);
        printf("%.1lf ",brr2[i]);
        printf("%.1lf ",brr3[i]);
    }
    return 0;
    }
