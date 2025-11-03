#include<stdio.h>
int main(){
    char s;
    scanf("%c",&s);
    int n=s-'A'+1;
    for(int i=0;i<n;i++){
        for(int j=0;j<n-1-i;j++){
            printf(" ");
        }
        for(int j=0;j<=i;j++){
            printf("%c",'A'+j);
            
        }
        for(int j=i-1;j>=0;j--){
            printf("%c",'A'+j);
        }
        printf("\n");
    }
    return 0;
}