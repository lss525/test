#include<stdio.h>
int main(){
    float v,s;

    printf("请输入下载速度：");
    scanf("%f",&v);
    printf("请输入下载大小：");
    scanf("%f",&s);
    printf("At %.2f megabits per second ,a file of %.2f megbytes downloads in %.2f seconds.",v,s,s*8/v);
    return 0;
}