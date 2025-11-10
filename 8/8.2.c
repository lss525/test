#include<stdio.h>
#include<string.h>
int main(){
    int ch;
    int da=0,xiao=0;
    while((ch=getchar())!=EOF){
        if(ch>='a'&&ch<='z'){
          xiao++;
        }
        if(ch>='A'&&ch<='Z'){
            da++;
        }
    }
    printf("%d %d",da,xiao);
    return 0;
}