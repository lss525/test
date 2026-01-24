#include<stdio.h>
struct Student{
    int id;
    char s[10];
    int sore;
};
int main(){
    int n;
    int sum=0;
    scanf("&d",&n);
    struct Student st[10];
    for(int i=0;i<n;i++){
        scanf("%d %s %d",&st[i].id,&st[i].s,&st[i].sore);
    }
    for(int i=0;i<n;i++){
        sum+=st[i].sore;
    }
    double av=1.0*sum/n;
    printf("%.2lf",av);
    for(int i=0;i<n;i++){
        if(st[i].sore<av){
            printf("%s %d\n",st[i].s,st[i],id);
        }
    }
    return 0;
}