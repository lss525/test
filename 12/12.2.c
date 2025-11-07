#include<stdio.h>
#include<time.h>
#include<stdlib.h>
int main(){
    int arr[1000];
    for(int i=0;i<100;i++){
        arr[i]=rand()%10;
    }
    for(int i=0;i<100-1;i++){
        for(int j=0;j<100-i-1;j++){
            if(arr[j]<arr[j+1]){
               int temp=arr[j];
               arr[j]=arr[j+1];
               arr[j+1]=temp;
            }
        }
    }
    for(int i=0;i<100;i++){
        printf("%d",arr[i]);
    }
    return 0;
}