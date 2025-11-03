#include<stdio.h>
#include<math.h>
int isPrime(int n){
    if(n<2) return 0;
    for(int i=2;i<=sqrt(n);i++){
        if(n%i==0){
            return 0;
        }
    }
    return 1;
}
int main(){
    int n=0;
    scanf("%d",&n);
    for(int i=0;i<=n;i++){
        if(isPrime(i)){
            printf("%d",i);

        }
    }
    return 0;
}