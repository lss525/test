#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>
#define HCHANG 4096

int kongzhi_sock=-1;

char pasv_ip[32] = "";      // 数据连接的 IP
int pasv_dk = 0;            // 数据连接的端口
int pasv_ok = 0;  


void fasong_ml(const char* geshi,...){

    char buf[HCHANG];
    va_list args;
    va_start(args, geshi);
    vsnprintf(buf,sizeof(buf),geshi,args);
    va_end(args);
    send(kongzhi_sock,buf,strlen(buf),0);
    printf("%s",buf);

}
void jieshou_yd(){
    char buf[HCHANG];
    int n=recv(kongzhi_sock,buf,sizeof(buf)-1,0);
    if(n>0){
        buf[n]='\0';
        printf("accept:%s",buf);
    }
}

void ml_yd(const char* cmd){
    fasong_ml("%s\r\n",cmd);
    jieshou_yd();
}

int jiexi_pasv(const char* yd, char* ip, int* dk) {
    int h1, h2, h3, h4, p1, p2;


    char* zuo = strchr(yd, '(');
    char* you = strchr(yd, ')');
    if (!zuo || !you) return 0;
    
    sscanf(zuo, "(%d,%d,%d,%d,%d,%d)", &h1, &h2, &h3, &h4, &p1, &p2);
    
    snprintf(ip, 32, "%d.%d.%d.%d", h1, h2, h3, h4);
    *dk = p1 * 256 + p2;
    
    return 1;
}


int shuju_lianjie(const char* ip, int dk) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(dk);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("no link");
        close(sock);
        return -1;
    }
    
    printf("already link %s:%d\n", ip, dk);
    return sock;
}


void denglu(const char* yonghu, const char* mima) {
    fasong_ml("USER %s\r\n", yonghu);
    jieshou_yd();
    
    fasong_ml("PASS %s\r\n", mima);
    jieshou_yd();
}


void kaiqi_pasv() {
    fasong_ml("PASV\r\n");
    
    char yd[HCHANG];
    int n = recv(kongzhi_sock, yd, sizeof(yd) - 1, 0);
    yd[n] = '\0';
    printf("accept%s", yd);
    
    if (jiexi_pasv(yd, pasv_ip, &pasv_dk)) {
        pasv_ok = 1;
        printf("pasv begin %s:%d\n", pasv_ip, pasv_dk);
    } else {
        pasv_ok = 0;
        printf("no pasv\n");
    }
}

void liechu_mulu() {

    if (!pasv_ok) {
        printf("please start pasv\n");
        return;
    }
    
   
    
    char yd[HCHANG];
    int n = recv(kongzhi_sock, yd, sizeof(yd) - 1, 0);
    yd[n] = '\0';
    printf("accept %s", yd);
    

    char ip[32];
    int dk;
    if (!jiexi_pasv(yd, ip, &dk)) {
        printf("no PASV \n");
        return;
    }

    int shuju = shuju_lianjie(pasv_ip, pasv_dk);
    if (shuju < 0) {
        pasv_ok = 0;
        return;
    }
    fasong_ml("LIST\r\n");
    jieshou_yd();  // 150
    
    char data[HCHANG];
    n = recv(shuju, data, sizeof(data) - 1, 0);
    if (n > 0) {
        data[n] = '\0';
        printf("%s", data);

    }
    
    close(shuju);
    pasv_ok = 0;  // ===== 新增：用完端口就失效 =====
    jieshou_yd(); // 226
}

void xiazai(const char* ming) {

    if (!pasv_ok) {
        printf("please start pasv\n");
        return;
    }

    char yd[HCHANG];
    int n = recv(kongzhi_sock, yd, sizeof(yd) - 1, 0);
    yd[n] = '\0';
    printf("accept %s", yd);
    
    char ip[32];
    int dk;
    if (!jiexi_pasv(yd, ip, &dk)) return;

   int shuju = shuju_lianjie(pasv_ip, pasv_dk);
    if (shuju < 0) {
        pasv_ok = 0;
        return;
    }
    

    fasong_ml("RETR %s\r\n", ming);
    jieshou_yd();  // 150
    

    FILE* ff = fopen(ming, "wb");
    if (!ff) {
        printf("do not create file: %s\n", ming);
        close(shuju);
        jieshou_yd();
        return;
    }
    
    char buf[8192];
    while ((n = recv(shuju, buf, sizeof(buf), 0)) > 0) {
        fwrite(buf, 1, n, ff);
    }
    
    fclose(ff);
    close(shuju);
    pasv_ok = 0;

    printf("file already download %s\n", ming);
    jieshou_yd();  // 226

}


void shangchuan(const char* ming) {

    if (!pasv_ok) {
        printf("please start pasv \n");
        return;
    }

    FILE* ff = fopen(ming, "rb");
    if (!ff) {
        printf("local file no there: %s\n", ming);
        return;
    }

    
    char yd[HCHANG];
    int n = recv(kongzhi_sock, yd, sizeof(yd) - 1, 0);
    yd[n] = '\0';
    printf("accept%s", yd);
    
    char ip[32];
    int dk;
    if (!jiexi_pasv(yd, ip, &dk)) { fclose(ff); return; }
    

    int shuju = shuju_lianjie(pasv_ip, pasv_dk);
    if (shuju < 0) {
        fclose(ff);
        pasv_ok = 0;
        return;
    }
    

    fasong_ml("STOR %s\r\n", ming);
    jieshou_yd();  // 150
    
    char buf[8192];
    while ((n = fread(buf, 1, sizeof(buf), ff)) > 0) {
        send(shuju, buf, n, 0);
    }
    
    fclose(ff);
    close(shuju);
    
    printf("file already upload: %s\n", ming);
    jieshou_yd();  // 226
}


void jiaohu() {
    char buf[HCHANG];
    
    printf("\nFTP \n");
    printf("  pasv  \n");
    printf("  ls  \n");
    printf("  get\n");
    printf("  put \n");
    printf("  cd  \n");
    printf("  pwd \n");
    printf("  quit \n\n");
    
    while (1) {
        printf("ftp> ");
        fgets(buf, sizeof(buf), stdin);

        int len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
        
        if (strcmp(buf, "pasv") == 0) {
            kaiqi_pasv();                  }
        else if (strncmp(buf, "ls", 2) == 0) {
            liechu_mulu();
        }
        else if (strncmp(buf, "get ", 4) == 0) {
            xiazai(buf + 4);
        }
        else if (strncmp(buf, "put ", 4) == 0) {
            shangchuan(buf + 4);
        }
        else if (strncmp(buf, "cd ", 3) == 0) {
            ml_yd(buf);
        }
        else if (strcmp(buf, "pwd") == 0) {
            ml_yd("PWD");
        }
        else if (strcmp(buf, "quit") == 0) {
            ml_yd("QUIT");
            break;
        }
        else {
            printf("未知命令: %s\n", buf);
        }
    }
    
}

int main(int argc,char*argv[]){
    char* ip="127.0.0.1";
    int dk=2100;
    if(argc>=2){
        ip=argv[1];
    }
    if(argc>=3){
        dk=atoi(argv[2]);
    }
    //socket
    kongzhi_sock=socket(AF_INET,SOCK_STREAM,0);
    if(kongzhi_sock<0){
        perror("socket");
        return 1;
    }
        struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(dk);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    
    if (connect(kongzhi_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect failed");
        close(kongzhi_sock);
        return 1;
    }
    
    printf("already link %s:%d\n", ip, dk);
    

    jieshou_yd();
    

    denglu("anonymous", "guest");
    

    jiaohu();
    

    close(kongzhi_sock);
    printf("already duankai\n");
    return 0;
}