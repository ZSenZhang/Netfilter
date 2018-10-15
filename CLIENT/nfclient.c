//命令行编译方法：gcc -o queue_fw queue_fw.c /usr/lib/libnetfilter_queue.a /usr/lib/libnflink.a /usr/lib/libcrypto.a -ldl
#include <sys/types.h>//
#include <stdio.h>
#include <stdlib.h>//系统调用
#include <unistd.h>//getopt()解析命令行
#include <netinet/in.h>
#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <string.h>
#include<time.h>
#include<sys/time.h>
#include<linux/ip.h>
#include<linux/tcp.h>
#include<linux/udp.h>
#include<linux/icmp.h>
#include <openssl/aes.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<fcntl.h>
#include <openssl/dh.h>
#include <memory.h>
#include<errno.h>
#include<openssl/rsa.h>
#include<openssl/pem.h>
#include<openssl/err.h>
#define OPENSSLKEY "/home/goldfish22/Desktop/test.key"//加密aes密钥的私钥
#define PUBLICKEY "/home/goldfish22/Desktop/test_pub.key"//加密aes密钥的公钥
#define MATCH 0
#define NMATCH 1
#define KEY_LEN 32
#define MSG_LEN pdata_len-piphdr->ihl*4//ip数据部分长度（不含头部）

int enable_flag = 1;
char method[64];
int exchange=0;//exchange=0 代表不用交换密钥，即为包过滤防火墙功能，=1代表需要交换密钥，为加密通信系统功能
unsigned int controlled_protocol = 0;
unsigned short controlled_srcport = 0;
unsigned short controlled_dstport = 0;
unsigned int controlled_saddr = 0;
unsigned int controlled_daddr = 0;
unsigned int encrypt_daddr=0;
unsigned int decrypt_saddr=0;
char *rsa="Using RSA Encryption to Exchange key!\n";
char *dh="Using DH Algorithm to Exchange key\n";

struct iphdr *piphdr;
static int fd;
static struct nfq_handle *h;
static struct nfq_q_handle *qh;
static struct nfnl_handle *nh;
unsigned char sharekey1[KEY_LEN];

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
	piphdr = (struct iphdr *)pdata;
    else
	return 1;

    int dealmethod = NF_DROP;

    char srcstr[32],deststr[32];
    inet_ntop(AF_INET,&(piphdr->saddr),srcstr,32);
    inet_ntop(AF_INET,&(piphdr->daddr),deststr,32);fflush(stdout);
    printf("get a packet: %s -> %s\n",srcstr,deststr);fflush(stdout);
    
    //包过滤功能
    if((!exchange)&&controlled_protocol==0)//所有协议都过滤
    {
        dealmethod=NF_DROP;
        printf("a packet is dropped!\n");
    }
    else if((!exchange)&&(piphdr->protocol == controlled_protocol))//特定协议过滤
    {
	if (piphdr->protocol  == 1)  //ICMP packet
                dealmethod = icmp_check();
	else if (piphdr->protocol  == 6) //TCP packet
                dealmethod = tcp_check();
	else if (piphdr->protocol  == 17) //UDP packet
                dealmethod = udp_check();
	else{
		printf("Unkonwn type's packet! \n");
                //dealmethod = NF_ACCEPT;
	}
        fflush(stdout);
    }
    else
        dealmethod = NF_ACCEPT;//放行
    
    //加密通信功能
    if((encrypt_daddr==piphdr->daddr)&&((piphdr->protocol==controlled_protocol)||controlled_protocol==0))//不指定协议则默认加密，亦可选择特定流加密
    {
        
        printf("this packet is encrypted!\n");
        unsigned char buf1[MSG_LEN];
        unsigned char buf2[MSG_LEN];
        memset(buf1,0,sizeof(buf1));  //plain
        memset(buf2,0,sizeof(buf2));  //encrypted
        memcpy(buf1,pdata+piphdr->ihl*4,pdata_len-piphdr->ihl*4);//需要加密的数据包明文存放在buf中

        AES_KEY aeskey;
        AES_set_encrypt_key(sharekey1,KEY_LEN*8,&aeskey);

        for(int i=0;i<sizeof(buf1);i+=16)//aes加密
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
        memcpy(pdata+piphdr->ihl*4,buf2,MSG_LEN);//buf2存放密文
    }
    //解密通信功能
    if((decrypt_saddr==piphdr->saddr)&&((piphdr->protocol==controlled_protocol)||controlled_protocol==0))
    {
        printf("This packet is decrypted!\n");
        unsigned char buf2[MSG_LEN];//encrypted msg
        unsigned char buf3[MSG_LEN];//decrypted msg
        memset(buf2,0,sizeof(buf2));
        memset(buf3,0,sizeof(buf3));
        memcpy(buf2,pdata+piphdr->ihl*4,MSG_LEN);

        AES_KEY aeskey;
        AES_set_decrypt_key(sharekey1,KEY_LEN*8,&aeskey);

        for(int i=0;i<sizeof(buf2);i+=16)//解密
        {
           if(sizeof(buf2)-i>=16)
               AES_decrypt(buf2+i,buf3+i,&aeskey);
           else
               memcpy(buf3+i,buf2+i,sizeof(buf2)-i);
        }

        memcpy(pdata+piphdr->ihl*4,buf3,MSG_LEN);
    }
    
    return nfq_set_verdict(qh, id, dealmethod, pdata_len, pdata);
}

int SendBigNum(int socket,BIGNUM* bignum)//dh密钥交换传送大整数
{
    int sizeOfArray=bignum->dmax;
    if(send(socket,(char*)bignum,sizeof(BIGNUM),0)<0){
            printf("fatal error:unable to send the msg!\n");
            return -1;
        }
    BN_ULONG num[sizeOfArray];
    for(int j=0;j<sizeOfArray;j++)
	{
    	num[j]=bignum->d[j];
	}
    if(send(socket,(char*)num,sizeof(BN_ULONG)*(sizeOfArray),0)<0){
            printf("fatal error:unable to send the msg!\n");
            return -1;
        }
    return 0;
}

int RecvBigNum(int socket,char num[])//dh密钥接收传送大整数
{
    recv(socket,num,sizeof(BIGNUM),0);
    int sizeOfArray=((BIGNUM*)num)->dmax;
    int bytesOfArray=sizeOfArray*sizeof(BN_ULONG);

    char numrecv[bytesOfArray];
    recv(socket,numrecv,bytesOfArray,0);

    BN_ULONG* numorz=(BN_ULONG*)malloc(bytesOfArray);
    for(int j=0;j<sizeOfArray;j++)
    {
	numorz[j]=(*((BN_ULONG*)(numrecv)+j));
    }
    ((BIGNUM*)num)->d=numorz;
}

void printBigNum(BIGNUM* bignum,char* c)//dh密钥交换打印大整数
{
    printf("%s:",c);
    BN_print_fp(stdout, bignum);
    printf("\n");

}

char *my_decrypt(char *str,char *path_key){//rsa解密函数
    char *p_de;
    RSA *p_rsa;
    FILE *file;
    int rsa_len;

    if((file=fopen(path_key,"r"))==NULL){
    perror("open key file error");
    return NULL;
    }

    if((p_rsa=PEM_read_RSAPrivateKey(file,NULL,NULL,NULL))==NULL){
    ERR_print_errors_fp(stdout);
    return NULL;
    }
    rsa_len=RSA_size(p_rsa);
    p_de=(unsigned char *)malloc(rsa_len+1);
    memset(p_de,0,rsa_len+1);
    if(RSA_private_decrypt(rsa_len,(unsigned char *)str,(unsigned char*)p_de,p_rsa,RSA_NO_PADDING)<0){
       return NULL;
    }
    RSA_free(p_rsa);
    fclose(file);
    return p_de;
}

int create_socket(){//ike中的socket通信
    int sock,n;
    struct sockaddr_in servaddr;

    if((sock=socket(AF_INET,SOCK_STREAM,0))<0)
    memset(&servaddr,0,sizeof(servaddr));
    servaddr.sin_family=AF_INET;
    servaddr.sin_port=htons(8000);
    servaddr.sin_addr.s_addr=inet_addr("192.168.146.144");

    if(connect(sock,(struct sockaddr*)&servaddr,sizeof(servaddr))<0){
        printf("connect error,errorno=%d\n",errno);fflush(stdout);
        close(sock);
	return -1;
    }
    else
	return sock;
}

void generate_sharekey1_rsa(){//rsa获得sharekey
    int sock=create_socket();
    char* en=(char*)malloc(128);
    recv(sock,en,128,0);
    char* de=my_decrypt(en,OPENSSLKEY);
    memcpy(sharekey1,de,KEY_LEN);

    close(sock);
}

void generate_sharekey1_dh(){//dh获得sharekey
    DH *d1;
    int ret,i;

    d1=DH_new();

    ret=DH_generate_parameters_ex(d1,256,DH_GENERATOR_2,NULL);

    // check parameters
    ret=DH_check(d1,&i);
    if(ret!=1) {
        printf("DH_check err!\n");
    }
    printf("DH parameters appear to be ok.\n");

    printBigNum(d1->p,"d1_p");
    printBigNum(d1->g,"d1_g");


    ret=DH_generate_key(d1);

    printBigNum(d1->pub_key,"d1_pub_key");

    int sock=create_socket();

    SendBigNum(sock,d1->p);
    SendBigNum(sock,d1->g);
    SendBigNum(sock,d1->pub_key);

    char d2_pubkey[20];
    RecvBigNum(sock,d2_pubkey);

    printBigNum((BIGNUM*)d2_pubkey,"d2_pub_key");

    DH_compute_key(sharekey1,(BIGNUM*)d2_pubkey,d1);

    close(sock);

}

void exchange_sharekey(){//通过密钥交换获得sharekey
    switch(exchange){
    case 1:
	generate_sharekey1_rsa();
	break;
    case 2:
	generate_sharekey1_dh();
	break;
    default:
	exit(1);
    }
}

int main(int argc, char **argv)
{
    char buf[1600];
    int length;

    if (argc == 1)
	enable_flag = 0;
    else {
	getpara(argc, argv);//命令行参数获取
    //printf("input info: p = %d, x = %d y = %d m = %d n = %d  \n%s\n", controlled_protocol,controlled_saddr,controlled_daddr,controlled_srcport,controlled_dstport,method);
     //fflush(stdout);
    }
    if(encrypt_daddr){
	char srcstr[32];
	inet_ntop(AF_INET,&(encrypt_daddr),srcstr,32);
        printf("encrypt msg to ip:%s\n",srcstr);fflush(stdout);
    }
    if(decrypt_saddr){
            char srcstr[32];
            inet_ntop(AF_INET,&(decrypt_saddr),srcstr,32);
        printf("decrypt msg from ip:%s\n",srcstr);fflush(stdout);
        }
    if(exchange)
    {
        exchange_sharekey();
        printf("sharekey is:");fflush(stdout);
        for(int i=0;i<KEY_LEN;i++){
            printf("%.2x",sharekey1[i]);fflush(stdout);
        }
        fflush(stdout);
    }
    //queue所有出去的包和进来的包
    system("sudo iptables -A OUTPUT -j QUEUE");
    system("sudo iptables -A INPUT -j QUEUE");
	//printf("opening library handle\n");
	h = nfq_open();
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
		nfq_handle_packet(h, buf,length);//trigger cb()
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
        optret = getopt(argc,argv,"pxymnhetf");
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
				if ( inet_aton(argv[optind], (struct in_addr* )&controlled_saddr) == 0){
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
				tmpport = atoi(argv[optind]);
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
	case 'e'://命令行参数，指定用何种方式交换密钥

                                if(strncmp(argv[optind],"RSA",3)==0)
                                        {exchange=1;strcpy(method, rsa);}
                                else if(strncmp(argv[optind],"DH",2)==0)
                                        {exchange=2;strcpy(method, dh);}
				else{
					printf("Invalid method!please check and try again! \n ");
				}
		break;
	case 't'://命令行参数，指定加密通信的对方ip地址，发往此ip的包加密
				if ( inet_aton(argv[optind], (struct in_addr* )&encrypt_daddr) == 0){//in_addr union 4 types
					printf("Invalid encrypt destination ip address! please check and try again! \n ");
					exit(1);
				}
		break;
        case 'f'://命令行参数，指定加密通信的对方ip地址，解密来自此ip的包
                                if ( inet_aton(argv[optind], (struct in_addr* )&decrypt_saddr) == 0){
                                    printf("Invalid decrypt source ip address! please check and try again! \n ");
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
                optret = getopt(argc,argv,"pxymnhetf");
	}
}

int port_check(unsigned short srcport, unsigned short dstport){
	if ((controlled_srcport== 0 ) && ( controlled_dstport == 0 ))
		return MATCH;
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

	picmphdr = (struct icmphdr *)((char *)piphdr +(piphdr->ihl*4));
	if((picmphdr->type != 0)&&(picmphdr->type != 8))
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
