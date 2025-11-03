#include<stdio.h>
int fibonacci(int n){
    int a=1,b=1;
    if(n==1||n==2){
        return 1;
    }
   return fibonacci(n-1)+fibonacci(n-2);
}
int main(){
    int n;
    scanf("%d",&n);
    int x=fibonacci(n);
    printf("%d",x);
    return 0;
}