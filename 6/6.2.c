#include<stdio.h>
#include<string.h>
int main(){
    char s[10000];
    scanf("%s",s);
    char s1[10000];
    for(int i=0;i<strlen(s);i++){
        s1[i]=s[i];
    }
    for(int i=strlen(s)-1;i>=0;i--){
        printf("%c",s1[i]);
    }
    return 0;
}