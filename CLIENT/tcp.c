#include<stdio.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<string.h>
#include<fcntl.h>
//测试程序，发送tcp包
int main(int argc,char** argv){
    int sock,n;
    char buff[32];
    struct sockaddr_in servaddr;
    sock=socket(AF_INET,SOCK_STREAM,0);
    memset(&servaddr,0,sizeof(servaddr));//clear the memory of the servaddr
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(8000);//sent to the server's 8000 port
    servaddr.sin_addr.s_addr=inet_addr("192.168.146.144");   
        

    connect(sock,(struct sockaddr*)&servaddr,sizeof(servaddr));

    memset(buff,0,32);
	strcpy(buff,argv[1]);
    //strcpy(buff,"SJTU,helloworld!");
    if(send(sock,buff,sizeof(buff),0)<0){
            printf("fatal error:unable to send the msg!\n");
            return -1;
        }
    
    close(sock);
    return 0;
}
