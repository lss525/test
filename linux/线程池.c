#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h> 
 
#define MAX_THREAD 10
#define MAX_QUEUE 100

typedef struct {
    int rows;
    int cols;
    double **date;
}juzheng;

typedef struct {
    int task_id;
    juzheng *a;
    juzheng *b;
    juzheng *task_end;
    double task_time;
    void(*huidiao)(int task_id,juzheng *task_end,double task_time);
}task;


task *duilie[MAX_QUEUE];
int duiqian=0;
int duiwei=0;
int geshu=0;
int yx=1;
int huodong=0;
int hd=0;
int wc=0;
int rwbh=1;

pthread_t xc[MAX_THREAD];
pthread_mutex_t suo=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t tj=PTHREAD_COND_INITIALIZER;

//创建矩阵
juzheng* creat_juzheng(int rows,int cols){
    juzheng *jz;
    int i;
    jz = (juzheng*)malloc(sizeof(juzheng));
    jz->rows = rows;
    jz->cols = cols;
    jz->date = (double**)malloc(rows * sizeof(double*));
    for(i = 0; i < rows; i++){
        jz->date[i] = (double*)malloc(cols * sizeof(double));
    }
    return jz;
} 

//释放
void shifang(juzheng *jz){
    int i;
    if(jz==NULL){
        return;
    }
    for(i=0;i<jz->rows;i++){
        free(jz->date[i]);
    }
    free(jz->date);
    free(jz);
}

juzheng* suiji(int rows,int cols){
    juzheng *jz;
    int i,j;
    jz=creat_juzheng(rows,cols);
    for(i=0;i<rows;i++){
       for(j=0;j<cols;j++){
          jz->date[i][j]=(double)rand()/RAND_MAX *10;
       }
    }
    return jz;
}

// 矩阵乘法 
juzheng*chengfa(juzheng *a,juzheng *b){
    juzheng *jg;
    int i,j,k;
    double he;
    if(a->cols!=b->rows){
        return NULL;
    }
    jg=creat_juzheng(a->rows,b->cols);
    for(i=0;i<a->rows;i++){
        for(j=0;j<b->cols;j++){
            he=0.0;
            for(k=0;k<a->cols;k++){
                he+=a->date[i][k]*b->date[k][j];
            }
            jg->date[i][j]=he;
        }
    }
    return jg;
}

void tianjia(task *rw){
    pthread_mutex_lock(&suo);
    duilie[duiwei]=rw;
    duiwei=(duiwei+1)%MAX_QUEUE;
    geshu++;
    pthread_cond_signal(&tj);
    pthread_mutex_unlock(&suo);
}

task* huoqu(){
    task *rw;
    pthread_mutex_lock(&suo);
    while(geshu==0&&yx){
        pthread_cond_wait(&tj,&suo);
    }
    if(geshu==0){
        pthread_mutex_unlock(&suo);
        return NULL;
    }
    rw=duilie[duiqian];
    duiqian=(duiqian+1)%MAX_QUEUE;
    geshu--;
    pthread_mutex_unlock(&suo);
    return rw;
}

void moren(int task_id,juzheng *jg,double sj){
    int i,j;
    double he=0.0;
    printf("任务%d完成，耗时：%.2fms\n",task_id,sj);
    if(jg!=NULL){
        printf("答案：%d*%d\n",jg->rows,jg->cols);
        for(i = 0; i < jg->rows && i < 3; i++){
            for(j = 0; j < jg->cols && j < 3; j++){
                he += jg->date[i][j];
            }
        }
    }

}

void* gongzuo(void *cs) {
    task *rw;
    struct timeval ks, js;  
    double sc;             
    
    while(yx||geshu>0){
        rw=huoqu();
        if(rw==NULL){
            continue;
        }
        
        pthread_mutex_lock(&suo);
        huodong++;
        pthread_mutex_unlock(&suo);
        
        printf("线程%ld任务%d\n", pthread_self(), rw->task_id);
        
        gettimeofday(&ks, NULL);
        rw->task_end = chengfa(rw->a, rw->b);
        gettimeofday(&js, NULL);
        
        if(rw->huidiao!=NULL){
            rw->huidiao(rw->task_id, rw->task_end, sc);
        }
        
        shifang(rw->a);
        shifang(rw->b);
        if(rw->task_end!=NULL){
            shifang(rw->task_end);
        }
        free(rw);
        
        pthread_mutex_lock(&suo);
        huodong--;
        wc++;
        pthread_mutex_unlock(&suo);
    }
    return NULL;
}

void chushi(int shu){
    int i;
    for(i=0;i<shu;i++){
        pthread_create(&xc[i],NULL,gongzuo,NULL);
        printf("creat线程%d\n",i+1);
    }
}

int tijiao(juzheng *a, juzheng *b, void (*hd)(int, juzheng*, double)) {
    task *rw;
    rw = (task*)malloc(sizeof(task));
    rw->task_id = rwbh++;
    rw->a = a;
    rw->b = b;
    rw->task_end = NULL;
    rw->task_time = 0.0;
    rw->huidiao = hd;
    tianjia(rw);
    return rw->task_id;
}


void guanbi(int shu) {
    int i;
    yx = 0;
    pthread_cond_broadcast(&tj);
    for(i = 0; i < shu; i++){
        pthread_join(xc[i], NULL);
    }
    printf("完成%d\n", wc);
}


void xiangxi(int task_id, juzheng *jg, double sj) {
    FILE *wj;     
    int i, j;
    double he = 0.0, pj; 
    
    wj = fopen("rw_rizhi.txt", "a");
    if(wj == NULL){
        return;
    }
    
    fprintf(wj, "rw%d完成", task_id);
    if(jg != NULL){
        for( i = 0; i < jg->rows; i++){
            for(j = 0; j < jg->cols; j++){
                he += jg->date[i][j];
            }
        }
        pj = he / (jg->rows * jg->cols);
    }
    fclose(wj);
}

int main(){
    int i;
    int cc[]={50,100,150};
    int xcshu=5;
    int rwshu=8;
    juzheng *a,*b;
    printf("");
    srand(time(NULL));
    FILE *wj = fopen("rw_rizhi.txt", "w");
    if(wj != NULL){
        fclose(wj);
    }
    
    chushi(xcshu);
    printf("\n");
    
    for(i=0; i < rwshu; i++){
        int dx=cc[rand() % 3];  
        a=suiji(dx, dx);
        b=suiji(dx, dx);
        if(i % 2==0){
            tijiao(a, b, xiangxi);
        } else {
            tijiao(a, b, moren);
        }
    }
    
    printf("\n");
    sleep(1);
        for(i = 0; i < 5; i++){
        sleep(1);
    }
    

    guanbi(xcshu);
    
    printf("\n结束\n");
    return 0;
}

