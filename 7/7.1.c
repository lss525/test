#include<stdio.h>
#define MONEY 10.00
#define ANOTHER_MONEY 15.00
#define QIANSHUI 0.15
#define XVSHUI 0.20
#define DUOSHU 0.25
int main(){
    printf("scanf worktime :");
    double time;
    scanf("%lf",&time);
    double money=0.00,x=0.00;
    if(time<=40.00){
        money=MONEY*time;
        if(time<=30){
            x=QIANSHUI*money;
        }
        else{
            x=45.00+XVSHUI*(money-300);
        }
    }
    else if(time>40&&time<=45){
        money=400.00+ANOTHER_MONEY*(time-40.00);
        x=45.00+XVSHUI*(money-300);
    }
    else{
        money=400.00+ANOTHER_MONEY*(time-40.00);
        x=75+DUOSHU*(money-450);
    }
    printf("%.2lf %.2lf %.2lf",money,x,money-x);
    return 0;
}