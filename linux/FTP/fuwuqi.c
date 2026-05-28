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
#include <fcntl.h>
#include <sys/sendfile.h>

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

void* shuju_chuanshu(void* arg) {

    Chuanshu_Canshu* cs = (Chuanshu_Canshu*)arg;
    
    int kehu_sock = accept(cs->shuju_sock, NULL, NULL);
    if (kehu_sock < 0) {
        close(cs->shuju_sock);
        free(cs);
        return NULL;
    }
    
    if (cs->leixing == 0) {
        char lb[8192];
        mulu_list(cs->lujing, lb, sizeof(lb));
        send(kehu_sock, lb, strlen(lb), MSG_NOSIGNAL);
    }

    else if (cs->leixing == 1) {

        int fd = open(cs->lujing, O_RDONLY);

        if (fd < 0) {
            printf("nohave");
            close(kehu_sock);
            close(cs->shuju_sock);
            free(cs);
            return NULL;
        }
        
        struct stat st;
        fstat(fd, &st);
        off_t offset = 0;
        
        while (offset < st.st_size) {
            ssize_t n = sendfile(kehu_sock, fd, &offset,
                                 st.st_size - offset);
            if (n <= 0) break;
        }
        
        close(fd);
    }

    else if (cs->leixing == 2) {
        FILE* ff = fopen(cs->lujing, "wb");
        if (ff) {
            char buf[8192];
            int n;
            while ((n = recv(kehu_sock, buf, sizeof(buf), 0)) > 0) {
                fwrite(buf, 1, n, ff);
            }
            fclose(ff);
            printf("%s\n", cs->lujing);
        }
    }
    
    close(kehu_sock);
    close(cs->shuju_sock);
    free(cs);
    return NULL;
}

void chuli_USER(int sock,char* arg,char* yonghu){
    strcpy(yonghu,arg);
    mingling_fasong(sock,"331,mima、\r\n");
    
}

void chuli_PASS(int sock, int* renzheng, char* yonghu, char* arg) {
    if (strcmp(yonghu, "anonymous") == 0 || 
        (strcmp(yonghu, "admin") == 0 && strcmp(arg, "admin") == 0)) {
        *renzheng = 1;
        mingling_fasong(sock, "230 ok\r\n");
    } else {
        mingling_fasong(sock, "530 no\r\n");
    }
}

void chuli_PWD(int sock, char* mulu) {
    mingling_fasong(sock, "257 \"%s\" now mulu\r\n", mulu);
}

void chuli_CWD(int sock, char* mulu, char* arg) {
    if (strcmp(arg, "/") == 0) strcpy(mulu, "/");
    else if (strcmp(arg, "..") == 0) {
        if (strcmp(mulu, "/") != 0) {
            char* pos = strrchr(mulu, '/');
            if (pos == mulu) strcpy(mulu, "/");
            else *pos = '\0';
        }
    } 
    else {
        if (strcmp(mulu, "/") == 0) snprintf(mulu, 256, "/%s", arg);
        else { strcat(mulu, "/"); strcat(mulu, arg); }
    }
    mingling_fasong(sock, "250 mulu change\r\n");
}

void chuli_TYPE(int sock, char* arg) {
    if (arg[0] == 'A') mingling_fasong(sock, "200 turnA\r\n");
    else if (arg[0] == 'I') mingling_fasong(sock, "200 turnI\r\n");
    else mingling_fasong(sock, "504 no zhichi\r\n");
}

void chuli_QUIT(int sock) {
    mingling_fasong(sock, "221 by");
}

int chuli_PASV(int sock, char* fuwu_ip) {
    int shuju_sock = socket(AF_INET, SOCK_STREAM, 0);
    
    int opt = 1;
    setsockopt(shuju_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0;  // 系统分配端口
    bind(shuju_sock, (struct sockaddr*)&addr, sizeof(addr));
    
    socklen_t al = sizeof(addr);
    getsockname(shuju_sock, (struct sockaddr*)&addr, &al);
    int dk = ntohs(addr.sin_port);  // 获取分配的端口
    
    listen(shuju_sock, 1);
    
    int p1 = dk / 256;
    int p2 = dk % 256;
    
    char ip_fmt[32];
    strcpy(ip_fmt, fuwu_ip);
    for (int i = 0; ip_fmt[i]; i++)
        if (ip_fmt[i] == '.') ip_fmt[i] = ',';
    
    mingling_fasong(sock, "227 (%s,%d,%d)\r\n", ip_fmt, p1, p2);
    
    printf("%d\n", dk);
    return shuju_sock;  // 返回数据监听 socket

}

void chuli_LIST(int sock,int shuju_sock,char* mulu){

    mingling_fasong(sock,"150 open mulu\r\n");
    Chuanshu_Canshu* cs=(Chuanshu_Canshu*)malloc(sizeof(Chuanshu_Canshu));
    cs->shuju_sock=shuju_sock;
    cs->leixing=0;
    snprintf(cs->lujing,sizeof(cs->lujing),"%s%s",GENMULU,mulu);

    pthread_t tid;
    pthread_create(&tid, NULL, shuju_chuanshu, cs);
    pthread_detach(tid);
    sleep(1);
    mingling_fasong(sock,"226 mulu put\r\n");


}

void chuli_RETR(int sock, int shuju_sock, char* mulu, char* arg){
    char fp[512];
    snprintf(fp, sizeof(fp), "%s%s/%s", GENMULU, mulu, arg);
    FILE* ff=fopen(fp,"rb");
    fclose(ff);
    Chuanshu_Canshu* cs=(Chuanshu_Canshu*)malloc(sizeof(Chuanshu_Canshu));
    cs->shuju_sock=shuju_sock;
    cs->leixing=1;
    strcpy(cs->lujing,fp);
    pthread_t tid;
    pthread_create(&tid, NULL, shuju_chuanshu, cs);
    pthread_detach(tid);
    sleep(1);
    mingling_fasong(sock,"226 put wancheng\r\n");

}

void chuli_STOR(int sock, int shuju_sock, char* mulu, char* arg){
    char fp[512];
    snprintf(fp,sizeof(fp),"%s%s/%s",GENMULU,mulu,arg);
    mingling_fasong(sock,"150 open fp");
    Chuanshu_Canshu* cs=(Chuanshu_Canshu*)malloc(sizeof(Chuanshu_Canshu));
    cs->shuju_sock=shuju_sock;
    cs->leixing=2;
    strcpy(cs->lujing,fp);

    pthread_t tid;
    pthread_create(&tid, NULL, shuju_chuanshu, cs);
    pthread_detach(tid);
    sleep(1);
    mingling_fasong(sock,"226 put wancheng\r\n");

}


void mingling_zhixing(int sock, char* cmd,
                      char* yonghu, char* mulu, int* renzheng,
                      int* shuju_sock, char* fuwu_ip) {
    char ml[32], cs[256];
    cs[0] = '\0';
    sscanf(cmd, "%s %[^\n]", ml, cs);
    daxie(ml);
    
    if (strcmp(ml, "USER") == 0) chuli_USER(sock, cs, yonghu);
    else if (strcmp(ml, "PASS") == 0) chuli_PASS(sock, renzheng, yonghu, cs);
    else if (strcmp(ml, "PWD") == 0) chuli_PWD(sock, mulu);
    else if (strcmp(ml, "CWD") == 0) chuli_CWD(sock, mulu, cs);
    else if (strcmp(ml, "PASV") == 0) {
        // 先关闭旧的数据 socket
        if (*shuju_sock >= 0) close(*shuju_sock);
        *shuju_sock = chuli_PASV(sock, fuwu_ip);
    }
    else if (strcmp(ml, "LIST") == 0) chuli_LIST(sock, *shuju_sock, mulu);
    else if (strcmp(ml, "RETR") == 0) chuli_RETR(sock, *shuju_sock, mulu, cs);
    else if (strcmp(ml, "STOR") == 0) chuli_STOR(sock, *shuju_sock, mulu, cs);
    else if (strcmp(ml, "TYPE") == 0) chuli_TYPE(sock, cs);
    else if (strcmp(ml, "SYST") == 0) mingling_fasong(sock, "215 UNIX Type: L8\r\n");
    else if (strcmp(ml, "FEAT") == 0) {
        mingling_fasong(sock, "211-Features:\r\n PASV\r\n UTF8\r\n211 End\r\n");
    }
    else if (strcmp(ml, "QUIT") == 0) chuli_QUIT(sock);
    else mingling_fasong(sock, "502 no achieve\r\n");
}

void* kongzhi_xiancheng(void* arg){
    int sock=*(int*)arg;
    free(arg);
    char yonghu[64]="";
    char mulu[512]="/";
    char fuwu_ip[16]="127.0.0.1";
    int renzheng=0;
    int shuju_sock=-1;
    char buf[HCHANG];
    printf("%d\n",sock);
    mingling_fasong(sock,"220 welcome to use ftp");
    while(1){
        int n=mingling_jieshuP(sock,buf,sizeof(buf));
        if(n==0){
            break;
        }
        mingling_zhixing(sock,buf,yonghu,mulu,&renzheng,&shuju_sock,fuwu_ip);
        if(strncmp(buf,"QUIT",4)==0){
            break;
        }

    }
    if(shuju_sock>=0){
        close(shuju_sock);
    }
    close(sock);
    printf("jieshu\r\n");
    return NULL;


} 

void* jieshou_lianjie(void* arg) {
    while(yunxing){

        struct sockaddr_in ka;
        socklen_t al=sizeof(ka);

        int ks=accept(fuwu_sock,(struct sockaddr*)&ka,&al);

        char ip[16];
        inet_ntop(AF_INET, &ka.sin_addr, ip, sizeof(ip));
        printf("link ip%s:%d\n",ip,ntohs(ka.sin_port));

        int* sock_ptr=(int*)malloc(sizeof(int));
        *sock_ptr=ks;

        pthread_t tid;
        pthread_create(&tid, NULL, kongzhi_xiancheng, sock_ptr);
        pthread_detach(tid);

        printf("creat\r\n");
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

    jieshou_lianjie(NULL);
    close(fuwu_sock);
    
    
    return 0;

}