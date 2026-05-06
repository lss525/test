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

int main(int argc,char*argv[]){
    signal(SIGINT,xinhao_chuli);
    signal(SIGTERM,xinhao_chuli);
    signal(SIGPIPE,SIG_IGN);
    int dk=DK;
    if(argc>1)dk=atoi(argv[1]);
    //根目录
    mkdir(GENMULU,0755);
    //socket
    //

}