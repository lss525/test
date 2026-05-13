#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <time.h>
#include <ctype.h>


#define DK 2100
#define HCHANG 1024
#define GENMULU "/tmp/ftp_root"

int fuwu_sock=-1;
int yunxing=1;

typedef struct{
    int shuju_sock;
    int leixing;
    char lujing[512];
} Chuanshu_Canshu;

void daxie(char*str){
    for(int i=0;str[i];i++){
        str[i]=toupper(str[i]);
                         
    }
}
void mingling_fasong(int sock, const char* geshi, ...) {
    char buf[HCHANG];
    va_list args;
    va_start(args, geshi);
    vsnprintf(buf, sizeof(buf), geshi, args);
    va_end(args);
    send(sock, buf, strlen(buf), MSG_NOSIGNAL);
    printf("[发送] %s", buf);
}

int mingling_jieshuP(int sock,char* buf,int len){
    int i=0;
    char ch;
    while(i<len-1&&recv(sock,&ch,1,0)>0){
        if(ch=='\r'){
            recv(sock,&ch,1,0);
            break;
        }
        buf[i++]=ch;
    }
    buf[i]='\0';
    if(i>0){
        printf("accept %s\n",buf);
    }
    return i;


}

void mulu_list(const char* lujing, char* out, int len) {
    DIR* dir = opendir(lujing);
    if (!dir) { out[0] = '\0'; return; }
    
    int pos = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        
        char fp[512];
        snprintf(fp, sizeof(fp), "%s/%s", lujing, entry->d_name);
        
        struct stat st;
        stat(fp, &st);
        
        char tb[64];
        strftime(tb, sizeof(tb), "%b %d %H:%M", localtime(&st.st_mtime));
        
        char qx[11];
        qx[0] = S_ISDIR(st.st_mode) ? 'd' : '-';
        qx[1] = (st.st_mode & S_IRUSR) ? 'r' : '-';
        qx[2] = (st.st_mode & S_IWUSR) ? 'w' : '-';
        qx[3] = (st.st_mode & S_IXUSR) ? 'x' : '-';
        qx[4] = (st.st_mode & S_IRGRP) ? 'r' : '-';
        qx[5] = (st.st_mode & S_IWGRP) ? 'w' : '-';
        qx[6] = (st.st_mode & S_IXGRP) ? 'x' : '-';
        qx[7] = (st.st_mode & S_IROTH) ? 'r' : '-';
        qx[8] = (st.st_mode & S_IWOTH) ? 'w' : '-';
        qx[9] = (st.st_mode & S_IXOTH) ? 'x' : '-';
        qx[10] = '\0';
        
        pos += snprintf(out + pos, len - pos, "%s 1 ftp ftp %ld %s %s\r\n",
                       qx, st.st_size, tb, entry->d_name);
        if (pos >= len - 1) break;
    }
    closedir(dir);
}

void xinhao_chuli(int xh){
    yunxing=0;
    if(fuwu_sock>=0){
        close(fuwu_sock);
        fuwu_sock=-1;
    }
}


void* jieshu_lianjie(void* arg){

    while(yunxing){

        struct sockaddr_in ka;
        socklen_t al=sizeof(ka);

        int ks=accept(fuwu_sock,(struct sockaddr*)&ka,&al);

        char ip[16];
        inet_ntop(AF_INET, &ka.sin_addr, ip, sizeof(ip));
        printf("link ip%s:%d\n",ntohs(ka.sin_port));

        int* sock_ptr=(int*)malloc(sizeof(int));
        *sock_ptr=ks;

        pthread_t tid;
        pthread_create(&tid, NULL, kongzhi_xiancheng, sock_ptr);
        pthread_detach(tid);

        printf("creat");
    }

    return NULL;

}

int main(int argc,char*argv[]){

    signal(SIGINT,xinhao_chuli);
    signal(SIGTERM,xinhao_chuli);
    signal(SIGPIPE,SIG_IGN);

    int dk=DK;
    if(argc>1)dk=atoi(argv[1]);

    //根目录
    mkdir(GENMULU,0755);

    char fp[512];
    snprintf(fp,sizeof(fp),"%s/welcome.txt",GENMULU);
    FILE*f=fopen(fp,"w");
    if(f){
        fclose(f);
    }
    snprintf(fp,sizeof(fp),"%s/test.txt",GENMULU);
    fopen(fp,"w");
    if(f){
        fclose(f);
    }
    
    //socket
    fuwu_sock=socket(AF_INET,SOCK_STREAM,0);
    if(fuwu_sock<0){
        perror("socket");
        return 1;
    }

    int opt=1;
    setsockopt(fuwu_sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(dk);

    if(bind(fuwu_sock,(struct sockaddr*)&addr,sizeof(addr))<0){
        perror("bind");
        close(fuwu_sock);
        return 1;
    }
    if(listen(fuwu_sock,10)<0){
        perror("listen");
        close(fuwu_sock);
        return 1;

    }

    printf("duankou:%d\n",dk);
    printf("%s\n",GENMULU);

    jieshu_lianjie(NULL);
    close(fuwu_sock);
    
    
    return 0;

}