#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<string.h>
#include<fcntl.h>
//测试程序，发送udp包
int main(int argc,char** argv){
    int sock,n;
    char buff[512];
    struct sockaddr_in servaddr;
    sock=socket(AF_INET,SOCK_DGRAM,0);
    memset(&servaddr,0,sizeof(servaddr));//clear the memory of the servaddr
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(8000);//sent to the server's 8000 port
    servaddr.sin_addr.s_addr=inet_addr("192.168.146.144");   
        

    memset(buff,0,512);
    strcpy(buff,"SJTU,helloworld,Udp!");
    sendto(sock,buff,sizeof(buff),0,(struct sockaddr*)&servaddr,sizeof(struct sockaddr));
    close(sock);
    return 0;
}
