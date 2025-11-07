#include<stdio.h>
void daoxv(double arr[],int n){
    for(int i=0;i<n-1;i++){
        for(int j=0;j<n-1-i;j++){
            if(arr[j+1]>arr[j]){
                int temp=arr[j+1];
                    arr[j+1]=arr[j];
                    arr[j]=temp;   
            }
        }
    }
}
int main(){
    int n;
    scanf("%d",&n);
    double arr[1101];
    for(int i=0;i<n;i++){
        scanf("%lf",&arr[i]);
    }
    daoxv(arr,n);
    for(int i=0;i<n;i++){
        printf("%lf ",arr[i]);
    }
    return 0;
}