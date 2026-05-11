#include <cstdarg>
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