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
    int socket1,socket2;
    //INITIALIZE SOCKET
    socket1=socket(AF_INET,SOCK_STREAM,0);
    struct sockaddr_in server;
    memset(&server,0,sizeof(server));
    server.sin_family=AF_INET;
    server.sin_port=htons(8000);
    server.sin_addr.s_addr=htonl(INADDR_ANY);
    bind(socket1,(struct sockaddr*)&server,sizeof(server));
    listen(socket1,10);
    socket2=accept(socket1, NULL, NULL);
    char recvbuf[512];
    int n;
    //fp=fopen("output1.png","w");
    while((n=recv(socket2,recvbuf,512,0))>0)
    {
	printf("%s\n",recvbuf);
        //fputs(recvbuf,fp);
        //fwrite(recvbuf, 1, n, fp);
        memset(recvbuf,0,512);
    }
    
    //fclose(fp);
    close(socket2);
    close(socket1);
}
