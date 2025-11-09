#include<stdio.h>
int fan(void){
    static int j=0;
    j++;
    return j;
}
int main(){
    int n;
    scanf("%d",&n);
    int y=0;
    for(int i=0;i<n;i++){
      y=fan();
    }
    printf("%d",y);
    return 0;
}