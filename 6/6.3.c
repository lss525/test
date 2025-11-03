#include<stdio.h>
int main(){
    int arr1[10];
    int n;
    printf("scanf geshu : ");
    scanf("%d",&n);
    printf("scanf shu : ");
    for(int i=0;i<n;i++){
        scanf("%d",&arr1[i]);
    }
    for(int i=n-1;i>=0;i--){
     printf("%d",arr1[i]);
    }
    return 0;
}