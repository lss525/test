#include<stdio.h>
#include<string.h>
int main(){
    char f[100];
    char n[100];
    printf("Please scanf first name:");
    scanf("%s",f);
    printf("Please scanf last name:");
    scanf("%s",n);
    printf("%s %s\n",f,n);
    for(int i=0;i<strlen(f)-1;i++){
        printf(" ");
    }
    printf("%d ",strlen(f));
    for(int i=0;i<strlen(n)-1;i++){
        printf(" ");
    }
    printf("%d\n",strlen(n));
 
    printf("%s %s\n",f,n);
    printf("%d",strlen(f));
    for(int i=0;i<strlen(f);i++){
        printf(" ");
    }
    printf("%d",strlen(n));
    return 0;
}