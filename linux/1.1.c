#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>

jmp_buf env;

void xjump() {
    printf("在 function_with_jump 中，设置跳转点\n");
    
    if (setjmp(env) == 0) {
        printf("第一次调用  0\n");
        return;
    } else {
        printf("跳回来\0");
    }
}

void tjump() {
    printf("2jump 已经返回\n");
    
    longjmp(env, 1);
}
int main() {
    printf("实验\n");
    
    xjump();
    
    printf("xjump 已经返回\n");   
    tjump();
    
    return 0;
}