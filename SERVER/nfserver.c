#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/icmp.h>
#include <openssl/aes.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <openssl/dh.h>
#include <memory.h>
#include <errno.h>
#include<openssl/rsa.h>
#include<openssl/pem.h>
#include<openssl/err.h>
#include <openssl/sha.h>

#define OPENSSLKEY "/home/goldfish22/Desktop/test.key"//加密aes密钥的私钥
#define PUBLICKEY "/home/goldfish22/Desktop/test_pub.key"//加密aes密钥的公钥

#define MATCH 0
#define NMATCH 1
#define KEY_LEN 32
#define MSG_LEN pdata_len-piphdr->ihl*4//ip数据部分长度（不含头部）
char method[64];
int exchange=0;//exchange=0 代表不用交换密钥，即为包过滤防火墙功能，=1代表需要交换密钥，为加密通信系统功能
int enable_flag = 1;
unsigned int decrypt_saddr=0;
unsigned int encrypt_daddr=0;
unsigned int controlled_protocol = 0;
unsigned short controlled_srcport = 0;
unsigned short controlled_dstport = 0;
unsigned int controlled_saddr = 0;
unsigned int controlled_daddr = 0; 
char *rsa="Using RSA Encryption to Exchange key!\n";
char *dh="Using DH Algorithm to Exchange key\n";

struct iphdr *piphdr;
static int fd;
static struct nfq_handle *h;
static struct nfq_q_handle *qh;
static struct nfnl_handle *nh;
unsigned char sharekey2[KEY_LEN];

int icmp_check();
int tcp_check();
int udp_check();
int getpara(int,char**);
static int callback(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg, struct nfq_data *nfa, void *data)
{
    struct nfqnl_msg_packet_hdr *ph;
    ph = nfq_get_msg_packet_hdr(nfa);
    if (ph == NULL)
	return 1;
    
    int id = 0;
    id = ntohl(ph->packet_id);
    
    if (enable_flag == 0)
        return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
    
    int pdata_len;
    unsigned char *pdata = NULL;
    pdata_len = nfq_get_payload(nfa, (unsigned char**)&pdata);//queue上来的数据包及长度
    if (pdata != NULL)
	piphdr = (struct iphdr *)pdata;//pdata=packet payload(ip header+ip data)
    else 
	return 1;

    char srcstr[32],deststr[32];
	inet_ntop(AF_INET,&(piphdr->saddr),srcstr,32);
	inet_ntop(AF_INET,&(piphdr->daddr),deststr,32);
    printf("\nget a packet: %s -> %s\n",srcstr,deststr);
    fflush(stdout);
    
    //包过滤功能
    int dealmethod = NF_DROP;
    if((!exchange)&&controlled_protocol==0)
        {
            dealmethod=NF_DROP;
            printf("a packet is dropped!\n");
        }
    else if((!exchange)&&(piphdr->protocol == controlled_protocol))
    {
	if (piphdr->protocol  == 1)  //ICMP packet
	    dealmethod = icmp_check();
	else if (piphdr->protocol  == 6) //TCP packet
    	    dealmethod = tcp_check();
	else if (piphdr->protocol  == 17) //UDP packet
	    dealmethod = udp_check();
        else
        {
	    printf("Unkonwn type's packet! \n");
	    dealmethod = NF_ACCEPT;
	}
    }
    else
        dealmethod = NF_ACCEPT;
    
    //解密通信功能
    if(decrypt_saddr==piphdr->saddr&&((piphdr->protocol==controlled_protocol)||controlled_protocol==0))
    {
        printf("This packet is decrypted!\n");fflush(stdout);
        unsigned char buf2[MSG_LEN];//encrypted msg
        unsigned char buf3[MSG_LEN];//decrypted msg
        memset(buf2,0,sizeof(buf2));
        memset(buf3,0,sizeof(buf3));
        memcpy(buf2,pdata+piphdr->ihl*4,MSG_LEN);
	
        AES_KEY aeskey;
        AES_set_decrypt_key(sharekey2,KEY_LEN*8,&aeskey);//设置加密密钥
        for(int i=0;i<sizeof(buf2);i+=16)
        {
            if(sizeof(buf2)-i>=16)
                AES_decrypt(buf2+i,buf3+i,&aeskey);
            else
                memcpy(buf3+i,buf2+i,sizeof(buf2)-i);
        }
        memcpy(pdata+piphdr->ihl*4,buf3,MSG_LEN);
    }
    
    //加密通信功能
    if(encrypt_daddr==piphdr->daddr&&((piphdr->protocol==controlled_protocol)||controlled_protocol==0))
    {
        printf("this packet is encrypted!\n");fflush(stdout);
        unsigned char buf1[MSG_LEN];
        unsigned char buf2[MSG_LEN];


        memset(buf1,0,sizeof(buf1));  //plain
        memset(buf2,0,sizeof(buf2));  //encrypted
        memcpy(buf1,pdata+piphdr->ihl*4,pdata_len-piphdr->ihl*4);

        AES_KEY aeskey;
        AES_set_encrypt_key(sharekey2,KEY_LEN*8,&aeskey);//设置解密密钥

        for(int i=0;i<sizeof(buf1);i+=16)
        {
            if(sizeof(buf1)-i>=16)
            {
                AES_encrypt(buf1+i,buf2+i,&aeskey);
            }
            else
            {
                memcpy(buf2+i,buf1+i,sizeof(buf1)-i);
            }
        }
        
        memcpy(pdata+piphdr->ihl*4,buf2,MSG_LEN);
    }
    
    return nfq_set_verdict(qh, id, dealmethod, pdata_len, pdata);
}

int SendBigNum(int socket,BIGNUM* bignum){
    int sizeOfArray=bignum->dmax;
    if(send(socket,(char*)bignum,sizeof(BIGNUM),0)<0){
            printf("fatal error:unable to send the msg!\n");
            return -1;
        }
    BN_ULONG num[sizeOfArray];
    for(int j=0;j<sizeOfArray;j++)//将d成员地址里的数据存放在数组里进行传输
	{
    	num[j]=bignum->d[j];
	}
    if(send(socket,(char*)num,sizeof(BN_ULONG)*(sizeOfArray),0)<0){
            printf("fatal error:unable to send the msg!\n");
            return -1;
        }
    return 0;
}

int RecvBigNum(int socket,char num[])
{ 
    recv(socket,num,sizeof(BIGNUM),0);
    int sizeOfArray=((BIGNUM*)num)->dmax;
    int bytesOfArray=sizeOfArray*sizeof(BN_ULONG);

    char numrecv[bytesOfArray];
    recv(socket,numrecv,bytesOfArray,0);

    BN_ULONG* numorz=(BN_ULONG*)malloc(bytesOfArray);//接收数组
    for(int j=0;j<sizeOfArray;j++)
    {
	numorz[j]=(*((BN_ULONG*)(numrecv)+j));
    }
    ((BIGNUM*)num)->d=numorz;//数组首地址赋值给成员d
}

void printBigNum(BIGNUM* bignum,char* c)
{
    printf("%s:",c);
    BN_print_fp(stdout, bignum);
    printf("\n");

}

char *my_encrypt(char *str,char *path_key)//rsa公钥加密函数
{    
    char *p_en;   
    RSA *p_rsa;  
    FILE *file;   
    int flen,rsa_len; 
 
    if((file=fopen(path_key,"r"))==NULL){      
        perror("open key file error");
        return NULL;
    } 

    if((p_rsa=PEM_read_RSA_PUBKEY(file,NULL,NULL,NULL))==NULL){
        ERR_print_errors_fp(stdout);
        return NULL;
    }   
    flen=strlen(str); 
    rsa_len=RSA_size(p_rsa); 
    p_en=(unsigned char *)malloc(rsa_len+1);  
    memset(p_en,0,rsa_len+1);
    
    if(RSA_public_encrypt(rsa_len,(unsigned char *)str,(unsigned char*)p_en,p_rsa,RSA_NO_PADDING)<0) 
    {
        return NULL;
    }
    
    RSA_free(p_rsa);  
    fclose(file);  
    return p_en;
}

int create_socket(){
    int socket1,socket2;
    socket1=socket(AF_INET,SOCK_STREAM,0);

    struct sockaddr_in server;
    memset(&server,0,sizeof(server));
    server.sin_family=AF_INET;
    server.sin_port=htons(8000);
    server.sin_addr.s_addr=htonl(INADDR_ANY);
    bind(socket1,(struct sockaddr*)&server,sizeof(server));

    listen(socket1,10);
    socket2=accept(socket1, NULL, NULL);
    close(socket1);
    return socket2;
}

void generate_sharekey2_rsa(){
    unsigned char s[256],md[KEY_LEN];   
    printf("Generate Key and encrypt it with RSA!\n");fflush(stdout);
    srand((int)time(0));//随机生成密钥
    for(int i=0;i<256;i++){
        s[i]=rand()%256;
    }
    SHA256((const unsigned char *)s, 256 , md); 

    unsigned char src[128];
    memset(src,0,sizeof(src));
    memcpy(src,md,KEY_LEN);

    char* en=NULL;
    en=my_encrypt(src,PUBLICKEY);
 
    for(int i=0;i<KEY_LEN;i++){
	sharekey2[i]=md[i];
    }

    int socket=create_socket();
    send(socket,en,128,0);

    close(socket); 

}

void generate_sharekey2_dh(){
    DH *d2;
    int i;
    
    d2=DH_new();

    DH_generate_parameters_ex(d2,256,DH_GENERATOR_2,NULL);   

    int socket2=create_socket();

    char p[20],g[20],d1_pubkey[20];

    RecvBigNum(socket2,p);
    d2->p=BN_dup((BIGNUM*)p);
    RecvBigNum(socket2,g);
    d2->g=BN_dup((BIGNUM*)g);
    printBigNum(d2->p,"d2_p");fflush(stdout);
    printBigNum(d2->g,"d2_g");fflush(stdout);

    DH_generate_key(d2);
    
    printBigNum(d2->pub_key,"d2_pub_key");fflush(stdout);

    RecvBigNum(socket2,d1_pubkey);
    printBigNum((BIGNUM*)d1_pubkey,"d1_pub_key");fflush(stdout);

    SendBigNum(socket2,d2->pub_key);

    DH_compute_key(sharekey2,(BIGNUM*)d1_pubkey,d2);
    

    close(socket2);   
}

void exchange_sharekey(){
    switch(exchange){
    case 1:
	generate_sharekey2_rsa();
	break;
    case 2:
	generate_sharekey2_dh();
	break;
    default:
	exit(1);
    }
}

int main(int argc, char **argv)
{ 
    char buf[1600];
    //resolve parameters
    int length;
    if (argc == 1)
	enable_flag = 0;
    else { 
	getpara(argc, argv);
        //printf("input info: p = %d, x = %d y = %d m = %d n = %d   \n%s", controlled_protocol,controlled_saddr,controlled_daddr,controlled_srcport,controlled_dstport,method);
        fflush(stdout);
    }

    if(decrypt_saddr){
	char srcstr[32];
	inet_ntop(AF_INET,&(decrypt_saddr),srcstr,32);
	printf("decrypt msg from ip:%s\n",srcstr);
        fflush(stdout);
    }
    if(encrypt_daddr){
            char srcstr[32];
            inet_ntop(AF_INET,&(encrypt_daddr),srcstr,32);
            printf("encrypt msg to ip:%s\n",srcstr);
            fflush(stdout);
        }
    if(exchange)
    {
        exchange_sharekey();
        printf("sharekey is:");fflush(stdout);
        for(int i=0;i<KEY_LEN;i++)
        {
            printf("%.2x",sharekey2[i]);fflush(stdout);
        }

    fflush(stdout);
    }
    //queue所有出去的包和进来的包
    system("sudo iptables -A INPUT -j QUEUE");
    system("sudo iptables -A OUTPUT -j QUEUE");
	//printf("opening library handle\n");
	h = nfq_open();//a new netlink connection obtained
	if (!h) {
		fprintf(stderr, "error during nfq_open()\n");
		exit(1);
	}

	//printf("unbinding existing nf_queue handler for AF_INET (if any)\n");
	if (nfq_unbind_pf(h, AF_INET) < 0) {
		fprintf(stderr, "already nfq_unbind_pf()\n");
		exit(1);
	}

	//printf("binding nfnetlink_queue as nf_queue handler for AF_INET\n");
	if (nfq_bind_pf(h, AF_INET) < 0) {
		fprintf(stderr, "error during nfq_bind_pf()\n");
		exit(1);
	}

	//printf("binding this socket to queue '0'\n");
	qh = nfq_create_queue(h,0, &callback, NULL);
	if (!qh) {
                fprintf(stderr, "error during nfq_create_queue()\n");fflush(stdout);
		exit(1);
	}

	//printf("setting copy_packet mode\n");
	if (nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff) < 0) {
		fprintf(stderr, "can't set packet_copy mode\n");
		exit(1);
	}

	nh = nfq_nfnlh(h);
	fd = nfnl_fd(nh);
   	while(1)
    	{
		length=recv(fd,buf,1600,0);
		nfq_handle_packet(h, buf,length);
        }
	nfq_destroy_queue(qh);
	nfq_close(h);
	exit(0);
}



void display_usage(char *commandname)
{
	printf("Usage 1: %s \n", commandname);
	printf("Usage 2: %s -x saddr -y daddr -m srcport -n dstport \n", commandname);
}

int getpara(int argc, char *argv[]){
	int optret;
	unsigned short tmpport;
        optret = getopt(argc,argv,"pxymnheft");
	while( optret != -1 ) {
        	switch( optret ) {
        	case 'p':
                        if (strncmp(argv[optind], "Ping",4) == 0 )
					controlled_protocol = 1;
                                else if ( strncmp(argv[optind], "TCP",3) == 0  )
					controlled_protocol = 6;
                                else if ( strncmp(argv[optind], "UDP",3) == 0 )
					controlled_protocol = 17;
				else {
					printf("Unkonwn protocol! please check and try again! \n");
					exit(1);
				}
        		break;
         case 'x':  
				if ( inet_aton(argv[optind], (struct in_addr* )&controlled_saddr) == 0){//str to network ip fails
					printf("Invalid source ip address! please check and try again! \n ");
					exit(1);
				}
         	break;
         case 'y':   
				if ( inet_aton(argv[optind], (struct in_addr* )&controlled_daddr) == 0){//in_addr union 4 types
					printf("Invalid destination ip address! please check and try again! \n ");
					exit(1);
				}
         	break;
         case 'm':   
				tmpport = atoi(argv[optind]);//string to integer
				if (tmpport == 0){
					printf("Invalid source port! please check and try again! \n ");
					exit(1);
                }
				controlled_srcport = htons(tmpport);
         	break;
        case 'n':   
				tmpport = atoi(argv[optind]);
				if (tmpport == 0){
					printf("Invalid destination port! please check and try again! \n ");
					exit(1);
				}
				controlled_dstport = htons(tmpport);
         	break;
	case 'e':			
                                if(strncmp(argv[optind],"RSA",3)==0)
                                        {exchange=1;strcpy(method, rsa);}
                                else if(strncmp(argv[optind],"DH",2)==0)
                                        {exchange=2;strcpy(method, dh);}
				else{
					printf("Invalid method!please check and try again! \n ");			
				}
		break;
	case 'f':
				if ( inet_aton(argv[optind], (struct in_addr* )&decrypt_saddr) == 0){
					printf("Invalid decrypt source ip address! please check and try again! \n ");
				}
		break;
        case 't':
                                if ( inet_aton(argv[optind], (struct in_addr* )&encrypt_daddr) == 0){//in_addr union 4 types
                                        printf("Invalid encrypt destination ip address! please check and try again! \n ");
                                                  exit(1);
                                }
                break;
         case 'h':   /* fall-through is intentional */
         case '?':
         	display_usage(argv[0]);
         	exit(1);;
                
         default:
				printf("Invalid parameters! \n ");
         	display_usage(argv[0]);
         	exit(1);;
        	}
                optret = getopt(argc,argv,"pxymnheft");
	}
}



int port_check(unsigned short srcport, unsigned short dstport){//check whether the port is blocked according to the rules,if blocked,return 0(match)
	if ((controlled_srcport== 0 ) && ( controlled_dstport == 0 ))
		return MATCH;//0
	if ((controlled_srcport != 0 ) && ( controlled_dstport == 0 ))
	{
		if (controlled_srcport == srcport) 
			return MATCH;
		else
			return NMATCH;
	}
	if ((controlled_srcport== 0 ) && ( controlled_dstport != 0 ))
	{
		if (controlled_dstport == dstport) 
			return MATCH;
		else
			return NMATCH;
	}
	if ((controlled_srcport != 0 ) && ( controlled_dstport != 0 ))
	{
		if ((controlled_srcport == srcport) && (controlled_dstport == dstport)) 
			return MATCH;
		else
			return NMATCH;
	}
	return NMATCH;
}


int ipaddr_check(unsigned int saddr, unsigned int daddr){   
	if ((controlled_saddr == 0 ) && ( controlled_daddr == 0 ))
		return MATCH;
	if ((controlled_saddr != 0 ) && ( controlled_daddr == 0 ))
	{
		if (controlled_saddr == saddr) 
			return MATCH;
		else
			return NMATCH;
	}
	if ((controlled_saddr == 0 ) && ( controlled_daddr != 0 ))
	{
\
		if (controlled_daddr == daddr) 
			return MATCH;
		else
			return NMATCH;
	}
	if ((controlled_saddr != 0 ) && ( controlled_daddr != 0 ))
	{
		if ((controlled_saddr == saddr) && (controlled_daddr == daddr)) 
			return MATCH;
		else
			return NMATCH;
	}
	return NMATCH;
}

int icmp_check(void){
	struct icmphdr *picmphdr;

	picmphdr = (struct icmphdr *)((char *)piphdr +(piphdr->ihl*4));//ihl=1 means 4 bytes,computer network
	if((picmphdr->type != 0)&&(picmphdr->type != 8)) //0:res,8:req  icmp/tcp
        return NF_ACCEPT;
	else   
	{
 		
  		if ((picmphdr->type == 0)&&(ipaddr_check(piphdr->saddr,piphdr->daddr) == MATCH)){
                        printf("an ICMP packet is denied.\n");fflush(stdout);
 			return NF_DROP;	
		}
		else if ((picmphdr->type == 8)&&(ipaddr_check(piphdr->saddr,piphdr->daddr) == MATCH)){
                        printf("an ICMP packet is denied.\n");fflush(stdout);
			return NF_DROP;
		}
		else
            return NF_ACCEPT;
	}
}

int tcp_check(void){
	struct tcphdr *ptcphdr;
   	ptcphdr = (struct tcphdr *)((char *)piphdr +(piphdr->ihl*4));
	if ((ipaddr_check(piphdr->saddr,piphdr->daddr) == MATCH) && (port_check(ptcphdr->source,ptcphdr->dest) == MATCH)){
                 printf("block an tcp packet.\n");fflush(stdout);
		return NF_DROP;
	}
	else
      return NF_ACCEPT;
}

int udp_check(void){
	struct udphdr *pudphdr;	
   	pudphdr = (struct udphdr *)((char *)piphdr +(piphdr->ihl*4));
	if ((ipaddr_check(piphdr->saddr,piphdr->daddr) == MATCH) && (port_check(pudphdr->source,pudphdr->dest) == MATCH) ){
                 printf("block an udp packet.\n");fflush(stdout);
		return NF_DROP;
	}
	else
      return NF_ACCEPT;
}
