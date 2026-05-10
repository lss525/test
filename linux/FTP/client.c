#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define HCHANG 4096

int kongzhi_sock=-1;
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