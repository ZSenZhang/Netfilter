//
//  main.c
//  3os
//
//  Created by 余金 on 17/5/27.
//  Copyright © 2017年 apple. All rights reserved.

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>

int main(int argc, char** argv)
{
    int socket1;
    //INITIALIZE SOCKET
    socket1=socket(AF_INET,SOCK_DGRAM,0);
    struct sockaddr_in server;
    memset(&server,0,sizeof(server));
    server.sin_family=AF_INET;
    server.sin_port=htons(8000);
    server.sin_addr.s_addr=htonl(INADDR_ANY);
    bind(socket1,(struct sockaddr*)&server,sizeof(server));

    char recvbuf[512];
    int n;
    //fp=fopen("output1.png","w");
    recvfrom(socket1,recvbuf,512,0,(struct sockaddr*)&server,sizeof(struct sockaddr));
	printf("%s\n",recvbuf);
    close(socket1);
}
