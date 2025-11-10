#include<stdio.h>
int main(){
    int count=0;
    int ch;
    while((ch=getchar())!=EOF){
        count++;
    }
    printf("%d",count);
    return 0;
}