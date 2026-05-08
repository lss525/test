/*
 * ftp_fuwuqi.c
 * 单文件 FTP 服务器
 * 编译: gcc -o ftp_fuwuqi ftp_fuwuqi.c -lpthread
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <time.h>
#include <ctype.h>

/* ============ 宏定义 ============ */
#define DK        2100        /* duankou 端口 */
#define HCHANG    1024        /* 缓冲长度 */
#define GENMULU   "/tmp/ftp_root"  /* 根目录 */

/* ============ 全局变量 ============ */
int fuwu_sock = -1;    /* 服务器 socket */
int yunxing = 1;       /* 运行标志 */

/* ============ 工具函数 ============ */
void daxie(char* str) {
    for (int i = 0; str[i]; i++) str[i] = toupper(str[i]);
}

void quhuanhang(char* str) {
    int len = strlen(str);
    if (len > 0 && str[len-1] == '\n') str[len-1] = '\0';
    if (len > 1 && str[len-2] == '\r') str[len-2] = '\0';
}

/* ============ 发送响应 ============ */
void mingling_fasong(int sock, const char* geshi, ...) {
    char buf[HCHANG];
    va_list args;
    va_start(args, geshi);
    vsnprintf(buf, sizeof(buf), geshi, args);
    va_end(args);
    send(sock, buf, strlen(buf), MSG_NOSIGNAL);
    printf("[发送] %s", buf);
}

/* ============ 接收命令 ============ */
int mingling_jieshou(int sock, char* buf, int len) {
    int i = 0;
    char ch;
    while (i < len - 1 && recv(sock, &ch, 1, 0) > 0) {
        if (ch == '\r') { recv(sock, &ch, 1, 0); break; }
        buf[i++] = ch;
    }
    buf[i] = '\0';
    if (i > 0) printf("[接收] %s\n", buf);
    return i;
}

/* ============ 目录列表 ============ */
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
        
        char qx[11];  /* quanxian 权限 */
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

/* ============ 信号处理 ============ */
void xinhao_chuli(int xh) {
    printf("\n收到信号 %d，关闭服务器...\n", xh);
    yunxing = 0;
    if (fuwu_sock >= 0) { close(fuwu_sock); fuwu_sock = -1; }
    exit(0);
}

/* ============ 命令处理函数 ============ */
void chuli_USER(int sock, char* arg, char* yonghu) {
    strcpy(yonghu, arg);
    mingling_fasong(sock, "331 用户名正确，需要密码\r\n");
}

void chuli_PASS(int sock, int* renzheng, char* yonghu) {
    if (strcmp(yonghu, "anonymous") == 0 || strcmp(yonghu, "admin") == 0) {
        *renzheng = 1;
        mingling_fasong(sock, "230 登录成功\r\n");
    } else {
        mingling_fasong(sock, "530 登录失败\r\n");
    }
}

void chuli_PWD(int sock, char* mulu) {
    mingling_fasong(sock, "257 \"%s\" 是当前目录\r\n", mulu);
}

void chuli_CWD(int sock, char* mulu, char* arg) {
    if (strcmp(arg, "/") == 0) strcpy(mulu, "/");
    else if (strcmp(arg, "..") == 0) {
        if (strcmp(mulu, "/") != 0) {
            char* pos = strrchr(mulu, '/');
            if (pos == mulu) strcpy(mulu, "/");
            else *pos = '\0';
        }
    } else {
        if (strcmp(mulu, "/") == 0) snprintf(mulu, 256, "/%s", arg);
        else { strcat(mulu, "/"); strcat(mulu, arg); }
    }
    mingling_fasong(sock, "250 目录已改变\r\n");
}

void chuli_PASV(int sock, int* shuju, int* shuju_dk, char* fuwu_ip) {
    if (*shuju >= 0) close(*shuju);
    
    *shuju = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(*shuju, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0;
    bind(*shuju, (struct sockaddr*)&addr, sizeof(addr));
    
    socklen_t al = sizeof(addr);
    getsockname(*shuju, (struct sockaddr*)&addr, &al);
    *shuju_dk = ntohs(addr.sin_port);
    listen(*shuju, 1);
    
    int p1 = *shuju_dk / 256;
    int p2 = *shuju_dk % 256;
    
    char ip_fmt[32];
    strcpy(ip_fmt, fuwu_ip);
    for (int i = 0; ip_fmt[i]; i++)
        if (ip_fmt[i] == '.') ip_fmt[i] = ',';
    
    mingling_fasong(sock, "227 进入被动模式 (%s,%d,%d)\r\n", ip_fmt, p1, p2);
}

void chuli_TYPE(int sock, char* arg) {
    if (arg[0] == 'A') mingling_fasong(sock, "200 类型设置为 A\r\n");
    else if (arg[0] == 'I') mingling_fasong(sock, "200 类型设置为 I\r\n");
    else mingling_fasong(sock, "504 类型不支持\r\n");
}

void chuli_QUIT(int sock) {
    mingling_fasong(sock, "221 再见\r\n");
}

/* ============ 数据连接 ============ */
int shuju_lianjie(int* shuju) {
    return accept(*shuju, NULL, NULL);
}

/* ============ LIST ============ */
void chuli_LIST(int sock, int* shuju, char* mulu) {
    mingling_fasong(sock, "150 打开数据连接用于目录列表\r\n");
    
    int ds = shuju_lianjie(shuju);
    if (ds < 0) {
        mingling_fasong(sock, "425 无法打开数据连接\r\n");
        return;
    }
    
    char fp[512], lb[8192];
    snprintf(fp, sizeof(fp), "%s%s", GENMULU, mulu);
    mulu_list(fp, lb, sizeof(lb));
    send(ds, lb, strlen(lb), MSG_NOSIGNAL);
    close(ds);
    
    mingling_fasong(sock, "226 目录发送完毕\r\n");
}

/* ============ 下载文件 ============ */
void chuli_RETR(int sock, int* shuju, char* mulu, char* arg) {
    char fp[512];
    snprintf(fp, sizeof(fp), "%s%s/%s", GENMULU, mulu, arg);
    
    FILE* ff = fopen(fp, "rb");
    if (!ff) {
        mingling_fasong(sock, "550 文件不存在\r\n");
        return;
    }
    fclose(ff);
    
    mingling_fasong(sock, "150 打开数据连接用于文件下载\r\n");
    int ds = shuju_lianjie(shuju);
    if (ds < 0) {
        mingling_fasong(sock, "425 无法打开数据连接\r\n");
        return;
    }
    
    ff = fopen(fp, "rb");
    char buf[8192];
    int n;
    while ((n = fread(buf, 1, sizeof(buf), ff)) > 0)
        send(ds, buf, n, MSG_NOSIGNAL);
    fclose(ff);
    close(ds);
    
    mingling_fasong(sock, "226 传输完成\r\n");
}

/* ============ 上传文件 ============ */
void chuli_STOR(int sock, int* shuju, char* mulu, char* arg) {
    char fp[512];
    snprintf(fp, sizeof(fp), "%s%s/%s", GENMULU, mulu, arg);
    
    mingling_fasong(sock, "150 打开数据连接用于文件上传\r\n");
    int ds = shuju_lianjie(shuju);
    if (ds < 0) {
        mingling_fasong(sock, "425 无法打开数据连接\r\n");
        return;
    }
    
    FILE* ff = fopen(fp, "wb");
    char buf[8192];
    int n;
    while ((n = recv(ds, buf, sizeof(buf), 0)) > 0)
        fwrite(buf, 1, n, ff);
    fclose(ff);
    close(ds);
    
    mingling_fasong(sock, "226 传输完成\r\n");
}

/* ============ 命令路由 ============ */
void mingling_zhixing(int sock, char* cmd, 
                      char* yonghu, char* mulu, int* renzheng,
                      int* shuju, int* shuju_dk, char* fuwu_ip) {
    char ml[32], cs[256];
    cs[0] = '\0';
    sscanf(cmd, "%s %[^\n]", ml, cs);
    daxie(ml);
    
    if (strcmp(ml, "USER") == 0) chuli_USER(sock, cs, yonghu);
    else if (strcmp(ml, "PASS") == 0) chuli_PASS(sock, renzheng, yonghu);
    else if (strcmp(ml, "PWD") == 0) chuli_PWD(sock, mulu);
    else if (strcmp(ml, "CWD") == 0) chuli_CWD(sock, mulu, cs);
    else if (strcmp(ml, "PASV") == 0) chuli_PASV(sock, shuju, shuju_dk, fuwu_ip);
    else if (strcmp(ml, "LIST") == 0) chuli_LIST(sock, shuju, mulu);
    else if (strcmp(ml, "RETR") == 0) chuli_RETR(sock, shuju, mulu, cs);
    else if (strcmp(ml, "STOR") == 0) chuli_STOR(sock, shuju, mulu, cs);
    else if (strcmp(ml, "TYPE") == 0) chuli_TYPE(sock, cs);
    else if (strcmp(ml, "SYST") == 0) mingling_fasong(sock, "215 UNIX Type: L8\r\n");
    else if (strcmp(ml, "FEAT") == 0) {
        mingling_fasong(sock, "211-Features:\r\n PASV\r\n UTF8\r\n211 End\r\n");
    }
    else if (strcmp(ml, "QUIT") == 0) chuli_QUIT(sock);
    else mingling_fasong(sock, "502 命令未实现\r\n");
}

/* ============ 会话处理（每个客户端一个线程） ============ */
void* huihe_chuli(void* arg) {
    int sock = *(int*)arg;
    free(arg);
    
    char yonghu[64] = "";
    char mulu[256] = "/";
    char fuwu_ip[16] = "127.0.0.1";
    int renzheng = 0;
    int shuju = -1;
    int shuju_dk = 0;
    char buf[HCHANG];
    
    mingling_fasong(sock, "220 欢迎使用 FTP 服务器\r\n");
    
    while (1) {
        int n = mingling_jieshou(sock, buf, sizeof(buf));
        if (n == 0) break;
        
        mingling_zhixing(sock, buf, yonghu, mulu, &renzheng, 
                        &shuju, &shuju_dk, fuwu_ip);
        
        if (strncmp(buf, "QUIT", 4) == 0) break;
    }
    
    if (shuju >= 0) close(shuju);
    close(sock);
    return NULL;
}

/* ============ 连接接收线程 ============ */
void* jieshou_lianjie(void* arg) {
    while (yunxing) {
        struct sockaddr_in ka;
        socklen_t al = sizeof(ka);
        int ks = accept(fuwu_sock, (struct sockaddr*)&ka, &al);
        if (ks < 0) { if (yunxing) perror("accept"); continue; }
        
        char ip[16];
        inet_ntop(AF_INET, &ka.sin_addr, ip, sizeof(ip));
        printf("新连接: %s:%d\n", ip, ntohs(ka.sin_port));
        
        int* sock_ptr = (int*)malloc(sizeof(int));
        *sock_ptr = ks;
        
        pthread_t tid;
        pthread_create(&tid, NULL, huihe_chuli, sock_ptr);
        pthread_detach(tid);
    }
    return NULL;
}

/* ============ 主函数 ============ */
int main(int argc, char* argv[]) {
    signal(SIGINT, xinhao_chuli);
    signal(SIGTERM, xinhao_chuli);
    signal(SIGPIPE, SIG_IGN);
    
    int dk = DK;
    if (argc > 1) dk = atoi(argv[1]);
    
    /* 创建根目录 */
    mkdir(GENMULU, 0755);
    
    /* 创建测试文件 */
    char fp[512];
    snprintf(fp, sizeof(fp), "%s/welcome.txt", GENMULU);
    FILE* f = fopen(fp, "w");
    if (f) { fprintf(f, "欢迎使用 FTP 服务器！\n"); fclose(f); }
    
    snprintf(fp, sizeof(fp), "%s/test.txt", GENMULU);
    f = fopen(fp, "w");
    if (f) { fprintf(f, "测试文件内容\n第二行\n第三行\n"); fclose(f); }
    
    /* 创建 socket */
    fuwu_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (fuwu_sock < 0) { perror("socket"); return 1; }
    
    int opt = 1;
    setsockopt(fuwu_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(dk);
    
    if (bind(fuwu_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(fuwu_sock); return 1;
    }
    
    if (listen(fuwu_sock, 10) < 0) {
        perror("listen"); close(fuwu_sock); return 1;
    }
    
    printf("FTP 服务器启动，端口: %d\n", dk);
    printf("根目录: %s\n", GENMULU);
    printf("按 Ctrl+C 停止\n");
    
    jieshou_lianjie(NULL);
    
    close(fuwu_sock);
    return 0;
}