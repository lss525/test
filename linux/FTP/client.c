#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define HCHANG 4096

int kongzhi_sock=-1;

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
    
    // 从响应中提取 6 个数字
    char* zuo = strchr(yd, '(');
    char* you = strchr(yd, ')');
    if (!zuo || !you) return 0;
    
    sscanf(zuo, "(%d,%d,%d,%d,%d,%d)", &h1, &h2, &h3, &h4, &p1, &p2);
    
    snprintf(ip, 32, "%d.%d.%d.%d", h1, h2, h3, h4);
    *dk = p1 * 256 + p2;
    
    return 1;
}

/* ============ 数据连接 ============ */
int shuju_lianjie(const char* ip, int dk) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(dk);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("数据连接失败");
        close(sock);
        return -1;
    }
    
    printf("[数据连接] 已连接 %s:%d\n", ip, dk);
    return sock;
}

/* ============ 登录 ============ */
void denglu(const char* yonghu, const char* mima) {
    fasong_ml("USER %s\r\n", yonghu);
    jieshou_yd();
    
    fasong_ml("PASS %s\r\n", mima);
    jieshou_yd();
}

/* ============ 列出目录 ============ */
void liechu_mulu() {
    // 1. 进入被动模式
    fasong_ml("PASV\r\n");
    
    char yd[HCHANG];
    int n = recv(kongzhi_sock, yd, sizeof(yd) - 1, 0);
    yd[n] = '\0';
    printf("[接收] %s", yd);
    
    // 2. 解析 IP 和端口
    char ip[32];
    int dk;
    if (!jiexi_pasv(yd, ip, &dk)) {
        printf("解析 PASV 失败\n");
        return;
    }
    
    // 3. 连接数据端口
    int shuju = shuju_lianjie(ip, dk);
    if (shuju < 0) return;
    
    // 4. 发送 LIST 命令
    fasong_ml("LIST\r\n");
    jieshou_yd();  // 150
    
    // 5. 接收目录内容
    char data[HCHANG];
    n = recv(shuju, data, sizeof(data) - 1, 0);
    if (n > 0) {
        data[n] = '\0';
        printf("\n========== 目录列表 ==========\n");
        printf("%s", data);
        printf("================================\n\n");
    }
    
    close(shuju);
    jieshou_yd();  // 226
}

/* ============ 下载文件 ============ */
void xiazai(const char* ming) {
    // 1. PASV
    fasong_ml("PASV\r\n");
    char yd[HCHANG];
    int n = recv(kongzhi_sock, yd, sizeof(yd) - 1, 0);
    yd[n] = '\0';
    printf("[接收] %s", yd);
    
    char ip[32];
    int dk;
    if (!jiexi_pasv(yd, ip, &dk)) return;
    
    // 2. 连接数据端口
    int shuju = shuju_lianjie(ip, dk);
    if (shuju < 0) return;
    
    // 3. 发送 RETR 命令
    fasong_ml("RETR %s\r\n", ming);
    jieshou_yd();  // 150
    
    // 4. 接收文件内容，保存到本地
    FILE* ff = fopen(ming, "wb");
    if (!ff) {
        printf("无法创建文件: %s\n", ming);
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
    
    printf("文件已下载: %s\n", ming);
    jieshou_yd();  // 226
}

/* ============ 上传文件 ============ */
void shangchuan(const char* ming) {
    // 检查本地文件是否存在
    FILE* ff = fopen(ming, "rb");
    if (!ff) {
        printf("本地文件不存在: %s\n", ming);
        return;
    }
    
    // 1. PASV
    fasong_ml("PASV\r\n");
    char yd[HCHANG];
    int n = recv(kongzhi_sock, yd, sizeof(yd) - 1, 0);
    yd[n] = '\0';
    printf("[接收] %s", yd);
    
    char ip[32];
    int dk;
    if (!jiexi_pasv(yd, ip, &dk)) { fclose(ff); return; }
    
    // 2. 连接数据端口
    int shuju = shuju_lianjie(ip, dk);
    if (shuju < 0) { fclose(ff); return; }
    
    // 3. 发送 STOR 命令
    fasong_ml("STOR %s\r\n", ming);
    jieshou_yd();  // 150
    
    // 4. 发送文件内容
    char buf[8192];
    while ((n = fread(buf, 1, sizeof(buf), ff)) > 0) {
        send(shuju, buf, n, 0);
    }
    
    fclose(ff);
    close(shuju);
    
    printf("文件已上传: %s\n", ming);
    jieshou_yd();  // 226
}

/* ============ 交互界面 ============ */
void jiaohu() {
    char buf[HCHANG];
    
    printf("\nFTP 客户端命令:\n");
    printf("  ls          列出目录\n");
    printf("  get 文件名   下载文件\n");
    printf("  put 文件名   上传文件\n");
    printf("  cd  目录名   切换目录\n");
    printf("  pwd         当前目录\n");
    printf("  quit        退出\n\n");
    
    while (1) {
        printf("ftp> ");
        fgets(buf, sizeof(buf), stdin);
        
        // 去掉末尾换行
        int len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
        
        if (strncmp(buf, "ls", 2) == 0) {
            liechu_mulu();
        }
        else if (strncmp(buf, "get ", 4) == 0) {
            xiazai(buf + 4);
        }
        else if (strncmp(buf, "put ", 4) == 0) {
            shangchuan(buf + 4);
        }
        else if (strncmp(buf, "cd ", 3) == 0) {
            ml_yd(buf);  // CWD 命令
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
    
}