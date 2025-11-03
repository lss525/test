#include<stdio.h>
int duqv(char a[]){
    if(strlen(a)<=1){
        return -1;
    }
    for(int i=0;i<strlen(a);i++){
        if(a[i]>='a'&&a[i]<='z'||a[i]>='A'&&a[i]<='Z'){
            if(a[i]>='a'&&a[i]<='z'){
                printf("%d ",a[i]-'a'+1);
            }
            else{
                printf("%d ",a[i]-'A'+1);
            }
        }
    }
}
int main(){
    char a[20];
    scanf("%s",a);
    duqv(a);
    return 0;
}