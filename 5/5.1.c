#include<stdio.h>
int main(){
    float h=0.0f;
   while(1){
    printf("scanf high:");
        scanf("%f",&h);
        if(h<0){
            printf("Enter a height in cm:%.1f\nbye");
            break;
        }
        int x=h/30.84;
        float i=h/2.54;
        if(h>0) {
            printf("Enter a height in cm:%.1f\n",h);
            printf("%.1f cm= %d feet, %.1f inch\n",h,x,i);
            continue;
        }
   }
    return 0;
}