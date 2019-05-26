#include <stdint.h>
#include <assert.h>
#include <sys/types.h>        
#include <sys/socket.h>  
#include <stdio.h>  
#include <stdlib.h>  
#include <arpa/inet.h>  
#include <unistd.h>  
#include <time.h>  
#include <string.h>  
#include <sys/select.h>  
#include <sys/time.h>  
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <pthread.h>
#define RTMP_SIG_SIZE 1536
#define SA struct sockaddr
static int chunksize=0;
static int streamid=0;
struct ConnectObj{
	char app[10];
	char flashver[10];
	char swfUrl[50];
	char tcUrl[100];
	int fpad;
	double audioCodecs;
	double videoCodecs;
	double videoFunction;
	char pageUrl[50];
	double objectEncoding;
}

int parseObject(char data[],int readk){
	int name_len=0;
	unsigned char* co;
	int i=0;
	while(1){
		if(data[readk++]==0x00&&data[readk++]==0x00){
			if(data[readk++]==0x09){
				printf("End of the Object\n");
				break;
			}
		}
		else{
			name_len=0;
			co=(unsigned char*)&name_len;
			co[1]=data[readk++];
			c0[0]=data[readk++];
			i=0;
			printf("Property \"");
			for(;i<name_len;i++)
				printf("%c",data[reak++]);
			printf("\"\n");
			if(data[readk++]==0x00){
				double value;
				co=(unsigned char*)&value;
				co[7]=data[readk++];
				co[6]=data[readk++];
				co[5]=data[readk++];
				co[4]=data[readk++];
				co[3]=data[readk++];
				co[2]=data[readk++];
				co[1]=data[readk++];
				co[0]=data[readk++];
				printf("double value %lf\n",value);
			}
			else if(data[readk++]==0x02){
				int len=0;
				co=(unsigned char*)&len;
				co[1]=data[readk++];
				co[0]=data[readk++];
				i=0;
				printf("string ");
				for(;i<len;i++)
					printf("%c",data[reak++]);
				printf("\n");
			}
			else if(data[readk++]==0x03){
				readk=parseObject(data[],readk);
			}
			else{
				printf("parseObject has new type\n");
			}
		}
	}
	return readk;
}

int parsedata(char data[],int datasize){
	int readk=0;
	int flag=0;
	while(readk<=datasize-1){
		if(data[readk++]==0x00){
			double value=0.0;
			unsigned char* co=(unsigned char*)&value;
			co[7]=data[readk++];
			co[6]=data[readk++];
			co[5]=data[readk++];
			co[4]=data[readk++];
			co[3]=data[readk++];
			co[2]=data[readk++];
			co[1]=data[readk++];
			co[0]=data[readk++];
			streamid=(int)value;
			printf("	the double data of _result is: %lf\n",value);
		}
		else if(data[readk++]==0x01){
			printf("the boolean data of _result is: %d\n",data[readk++]);
		}
		else if(data[readk++]==0x02){
			int data_len;
			int i=0;
			char name[30];
			data_len=data_len<<8|data[readk++];
			data_len=data_len<<8|data[readk++];
			printf("	the string data of _result length: %d\n",data_len);
			//for(;i<data_len;i++)
				//printf("	the string data of _result content: %c\n",data[readk++]);
			memset(name,0,sizeof(name));
			for(;i<data_len;i++)
				name[i]=data[readk++];
			name[data_len]='\0';
			printf("	the string data of _result content: %s\n",name);
			if(data_len==7){
				if(memcmp(name,"_result",7)== 0)
					flag=1;
			}
		}
		else if(data[readk++]==0x03){
			readk=parseObject(data,readk);
			
		}
		else if(data[readk++]==0x05){
			printf("	the data of _result is NULL\n");
		}
		else{
			printf("	the data type of _result is not know\n");
			break;
		}
	}
	return flag;
	
}

int addstring(char data[],int con_len,char name[],int content_len,char content[]){
	unsigned char* co;
	int name_len=0;
	name_len=strlen(name);
	int i=0;
	
	data[con_len++]=0x02;
	co=(unsigned char*)name_lan;
	data[con_len++]=co[1];
	data[con_len++]=co[0];
	for(;i<name_len;i++)
		data[con_len++]=name[i];
	if(con_len>0){
		data[con_len++]=0x02;
		co=(unsigned char*)content_len;
		data[con_len++]=co[1];
		data[con_len++]=co[0];
		i=0;
		for(;i<content_len;i++)
			data[con_len++]=content[i];
	}
	return con_len;
}

int addBoolen(char data[],int con_len,char name[],int value){
	unsigned char* co;
	int name_len=0;
	name_len=strlen(name);
	int i=0;
	
	data[con_len++]=0x02;
	co=(unsigned char*)name_lan;
	data[con_len++]=co[1];
	data[con_len++]=co[0];
	for(;i<name_len;i++)
		data[con_len++]=name[i];
	data[con_len++]=0x01;
	co=(unsigned char*)value;
	data[con_len++]=co[0];
	return con_len;
}

int addDouble(chae data[],int con_len,char name[],double value){
	unsigned char* co;
	int name_len=0;
	name_len=strlen(name);
	int i=0;
	
	if(name_len>0){
		data[con_len++]=0x02;
		co=(unsigned char*)name_lan;
		data[con_len++]=co[1];
		data[con_len++]=co[0];
		for(;i<name_len;i++)
			data[con_len++]=name[i];
	}
	data[con_len++]=0x00;
	co=(unsigned char*)value;
	data[con_len++]=co[7];
	data[con_len++]=co[6];
	data[con_len++]=co[5];
	data[con_len++]=co[4];
	data[con_len++]=co[3];
	data[con_len++]=co[2];
	data[con_len++]=co[1];
	data[con_len++]=co[0];
	return con_len;
}

int getresult(int rtmpfd,char data[]){
	//read Window Acknowledgement Size
	memset(data,0,sizeof(data));
	if(read(rtmpfd,data,12)!=12){
		printf("	write \"Window Acknowledgement Size\" error:%S\n",strerror(errno));
		return -1;
	}
	int format;
	int CSID;
	int type;
	unsigned char* co;
	co=(unsigned char*)&format;
	co[0]=data[0]>>6&0x01;
	printf("RTMP format :%d\n",format);
	
	co=(unsigned char*)&CSID;
	co[0]=data[0]&0x3f;
	printf("RTMP CSID :%d\n",CSID);
	
	co=(unsigned char*)&type;
	co[0]=data[7];
	printf("RTMP type :%d\n",type);
	
	datasize=0;
	co=(unsigned char*)&datasize;
	co[2]=data[4];
	co[1]=data[5];
	co[0]=data[6];
	printf("datasize %d\n",datasize);
	if(type==5){
		printf("Window Acknowledgement Size\n");
		if(read(rtmpfd,data,datasize)!=datasize){
			printf("	write \"Window Acknowledgement Size\" error:%S\n",strerror(errno));
			return -1;
		}
		int windowsize=0;
		co=(unsigned char*)&windowsize;
		co[3]=data[0];
		co[2]=data[1];
		co[1]=data[2];
		co[0]=data[3];
		printf("Window Size is :%d\n",windowsize);
	}
	else if(type==6){
		printf("Set Peer Bandwidth\n");
		if(read(rtmpfd,data,datasize)!=datasize){
			printf("	write \"Set Peer Bandwidth\" error:%S\n",strerror(errno));
			return -1;
		}
		int windowsize=0;
		co=(unsigned char*)&windowsize;
		co[3]=data[0];
		co[2]=data[1];
		co[1]=data[2];
		co[0]=data[3];
		printf("Window Size is :%d\n",windowsize);
		if(data[4]==0x02)
			printf("Limit type: Dynamic(2)\n");
	}
	else if(type==1){
		printf("Set Chunk Size\n");
		if(read(rtmpfd,data,datasize)!=datasize){
			printf("	write \"Set Chunk Size\" error:%S\n",strerror(errno));
			return -1;
		}
		int windowsize=0;
		co=(unsigned char*)&windowsize;
		co[3]=data[0];
		co[2]=data[1];
		co[1]=data[2];
		co[0]=data[3];
		chunksize=windowsize;
		printf("Window Size is :%d\n",windowsize);
	}
	else if(type==20){
		memset(data,0,sizeof(data));
		if(read(rtmpfd,data,datasize)!=datasize){
			printf("	write \"Set Chunk Size\" error:%S\n",strerror(errno));
			return -1;
		}
		if(parsedata(data,datasize)<0)
			return -1;
	}
	return 1;
}

int my_rtmpconnect(int rtmpfd,struct ConnectObj* con_obj){
	printf("my_connect\n");
	unsigned char data[300];
	unsigned char basichead,*co;
	memset(&basichead,0,1);
	basichead=0x02;

	char messagehead[11];
	memset(messagehead,0,11);
	messagehead[5]=0x04;
	messagehead[6]=0x01;

	uint32_t chunksize=128;
	int datasize=0;
	int i;
	memcpy(data,&basichead,1);
	memcpy(data+1,messagehead,11);
	//memcpy(data+12,&chunksize,4);
	co=(unsigned char*)&chunksize;
	data[12]=co[3];
	data[13]=co[2];
	data[14]=co[1];
	data[15]=co[0];
	//发送set chunk size
	i=0;
	for(;i<16;i++){
		printf("%02x ",data[i]);
	}
	printf("\n\n");
	if(write(rtmpfd,data,16)!=16){
		printf("	write \"set chunk size\" error:%S\n",strerror(errno));
		return -1;
	}
	
	
	//sent connect packet
	memset(&basichead,0,1);
	basichead=0x03;
	memset(messagehead,0,11);
	//messagehead[5]=0x04;//message length size ??
	messagehead[6]=0x14;
	memset(data,0,sizeof(data));
	
	memcpy(data,&basichead,1);
	memcpy(data+1,messagehead,11);
	int con_len=12;
	char name[10];
	int content_len=strlen(con_obj->app);
	if(content_len){
		memset(name,0,sizeof(name));
		strcpy(name,"app");
		con_len=addstring(data,con_len,name,content_len,con_obj->app);
		
	}
	
	content_len=strlen(con_obj->flashver);
	if(content_len){
		memset(name,0,sizeof(name));
		strcpy(name,"flashver");
		con_len=addstring(data,con_len,name,content_len,con_obj->flashver);
	}
	
	content_len=strlen(con_obj->swfUrl);
	if(content_len){
		memset(name,0,sizeof(name));
		strcpy(name,"swfUrl");
		con_len=addstring(data,con_len,name,content_len,con_obj->swfUrl);
	}
	
	content_len=strlen(con_obj->tcUrl);
	if(content_len){
		memset(name,0,sizeof(name));
		strcpy(name,"tcUrl");
		con_len=addstring(data,con_len,name,content_len,con_obj->tcUrl);
	}
	
	if(con_obj->fpad!=0){
		memset(name,0,sizeof(name));
		strcpy(name,"fpad");
		con_len=addBoolen(data,con_len,name,con_obj->fpad);
	}
	if(con_obj.audioCodecs!=0){
		memset(name,0,sizeof(name));
		strcpy(name,"audioCodecs");
		con_len=addDouble(data,con_len,name,con_obj->audioCodecs);
	}
	if(con_obj.videoCodecs!=0){
		memset(name,0,sizeof(name));
		strcpy(name,"videoCodecs");
		con_len=addDouble(data,con_len,name,con_obj->videoCodecs);
	}
	if(con_obj.videoFunction!=0){
		memset(name,0,sizeof(name));
		strcpy(name,"videoFunction");
		con_len=addDouble(data,con_len,name,con_obj->videoFunction);
	}
	
	content_len=strlen(con_obj->pageUrl);
	if(content_len){
		memset(name,0,sizeof(name));
		strcpy(name,"pageUrl");
		con_len=addstring(data,con_len,name,content_len,con_obj->pageUrl);
	}
	if(con_obj.objectEncoding!=0){
		memset(name,0,sizeof(name));
		strcpy(name,"objectEncoding");
		con_len=addDouble(data,con_len,name,con_obj->objectEncoding);
	}
	datasize=con_len-12;
	co=(unsigned char*)&datasize;
	data[4]=co[2];
	data[5]=co[1];
	data[6]=co[0];
	// i=0;
	// for(;i<con_len;i++){
		// printf("%02x ",data[i]);
	// }
	printf("\n\n");
	if(write(rtmpfd,data,con_len)!=con_len){
		printf("	write \"set chunk size\" error:%S\n",strerror(errno));
		return -1;
	}
	if(getresult(rtmpfd,data)<0)
		printf("get 1 result error\n");
	if(getresult(rtmpfd,data)<0)
		printf("get 2 result error\n");
	if(getresult(rtmpfd,data)<0)
		printf("get 3 result error\n");
	if(getresult(rtmpfd,data)<0)
		printf("get 4 result error\n");
	
	
	//read 
	return 1;
	
}

int my_handshake(int rtmpfd){
	unsigned char clibuf[1537];
	char type;
	unsigned char serverbuf[1536];
	char* clientsig=clibuf+1;
	uint32_t uptime;
	int i;
	char *reply=NULL;
	int count=0;
	
	
	//uptime = htonl(RTMP_GetTime());
	memset(clibuf,0,sizeof(clibuf));
	clibuf[0]=0x03;
	uptime = htonl(0);
	memcpy(clientsig, &uptime, 4);
	memset(&clientsig[4], 0, 4);
	
	//随机填充
	int32_t *ip = (int32_t *)(clientsig+8);
	for (i = 2; i < RTMP_SIG_SIZE/4; i++)
		*ip++ = rand();
	i=0;
	for(;i<1537;i++){
		printf("%02x  ",clibuf[i-1]);
		if(i%10==0)
			printf("\n");
	}
	printf("\n");
	//可以使用一个字符串数组，先填好随机的内容，然后再直接赋值发送
	
	if((count=write(rtmpfd,clibuf,1537))!=1537){//send c0 and c1
		printf("	send c0 c1 error %s\n",strerror(errno));
		return -1;
	}
	printf("write c0 c1 count:%d\n",count);
	if(read(rtmpfd,&type,1)!=1){//recvive s0
		printf("	recvive s0 error: %s\n",strerror(errno));
		return -1;
	}
	printf("type is %02x\n",type);
	if(type!=0x03){
		printf("	s0 is not 0x03\n");
		return -1;
	}
	memset(serverbuf,0,sizeof(serverbuf));
	if(read(rtmpfd,serverbuf,1536)!=1536){//recvive s1
		printf("	read s1 error:%s\n",strerror(errno));
		return -1;
	}
	i=1;
	for(;i<1537;i++){
		printf("%02x  ",serverbuf[i-1]);
		if(i%10==0)
			printf("\n");
	}
	printf("\n");
	memcpy(&uptime, serverbuf, 4);
	uptime = ntohl(uptime);
	printf("serverbuf uptime is %d\n",uptime);
	reply = serverbuf;
	
	//uptime = htonl(RTMP_GetTime());//再次获得时间，并替换serversig中的时间戳
	uptime = htonl(0);
	memcpy(reply, &uptime, 4);
	if((count=write(rtmpfd,reply,RTMP_SIG_SIZE))!=RTMP_SIG_SIZE){//发送c2
		printf("	send c2 error:%s\n",strerror(errno));
		return -1;
	}
	printf("write c2 count:%d\n",count);
	// 2nd part of handshake 
	memset(serverbuf,0,sizeof(serverbuf));
	if(read(rtmpfd,serverbuf,RTMP_SIG_SIZE)!= RTMP_SIG_SIZE){//接收s2
		//return FALSE;
		printf("	recvive s2 error:%s\n",strerror(errno));
		return -1;
	}
	if(memcmp(serverbuf, clientsig, RTMP_SIG_SIZE)!= 0)//对比s2 和c1，如果一样则握手成功
	{
		printf("	s2 and c1 is not same\n");
		return -1;
	}
	else{
		printf("	hand shake Success\n");
		return 1;
	}
}

int my_RTMP_connect(int rtmpfd,struct ConnectObj* con_obj){
	my_handshake(rtmpfd);
	my_rtmpconnect(rtmpfd,con_obj);
}

int TCP_connect(char rtmpurl[]){

	char IPDest[20]="10.99.1.140";
	int rtmpfd;
	int count=0;
	struct sockaddr_in tcpsaddr;
	rtmpfd=socket(AF_INET,SOCK_STREAM,0);
	bzero(&tcpsaddr,sizeof(tcpsaddr));
	tcpsaddr.sin_family=AF_INET;
	tcpsaddr.sin_port=htons(1935);
	inet_pton(AF_INET,IPDest,&tcpsaddr.sin_addr);
	if(connect(rtmpfd,(SA *)&tcpsaddr,sizeof(tcpsaddr))<0){
		printf("	TCP connect error: %s\n",strerror(errno));
		return -1;
	}
	printf("TCP success\n");
	return rtmpfd;
}

int my_ConnectStream(int rtmpfd,struct ConnectObj* con_obj){
	char data[200];
	char name[50];
	int con_len;
	unsigned char* co;
	
	//send release stream
	//no has stream id
	memset(data,0,sizeof(data));
	data[0]=0x43;
	data[7]=0x14;
	con_len=8;
	memset(name,0,sizeof(name));
	strcpy(name,"releaseStream");
	con_len=addstring(data,con_len,name,0,NULL);
	con_len=addDouble(data,con_len,NULL,2);
	data[con_len++]=0x05;
	memset(name,0,sizeof(name));
	strcpy(name,"test");//此时写死了
	con_len=addstring(data,con_len,name,0,NULL);
	con_len-=8;
	co=(unsigned char*)&con_len;
	data[4]=co[2];
	data[5]=co[1];
	data[6]=co[0];
	con_len+=8;
	if(write(rtmpfd,data,con_len)!=con_len){
		printf("send \"releaseStream\" error:%s\n",strerror(errno));
		return -1;
	}
	//send FCpublish
	memset(data,0,sizeof(data));
	data[0]=0x43;
	data[7]=0x14;
	con_len=8;
	memset(name,0,sizeof(name));
	strcpy(name,"FCPublish");
	con_len=addstring(data,con_len,name,0,NULL);
	con_len=addDouble(data,con_len,NULL,3);
	data[con_len++]=0x05;
	memset(name,0,sizeof(name));
	strcpy(name,"test");//此时写死了
	con_len=addstring(data,con_len,name,0,NULL);
	con_len-=8;
	co=(unsigned char*)&con_len;
	data[4]=co[2];
	data[5]=co[1];
	data[6]=co[0];
	con_len+=8;
	if(write(rtmpfd,data,con_len)!=con_len){
		printf("send \"releaseStream\" error:%s\n",strerror(errno));
		return -1;
	}
	//create stream
	memset(data,0,sizeof(data));
	data[0]=0x43;
	data[7]=0x14;
	con_len=8;
	memset(name,0,sizeof(name));
	strcpy(name,"createStream");
	con_len=addstring(data,con_len,name,0,NULL);
	con_len=addDouble(data,con_len,NULL,4);
	data[con_len++]=0x05;
	con_len-=8;
	co=(unsigned char*)&con_len;
	data[4]=co[2];
	data[5]=co[1];
	data[6]=co[0];
	con_len+=8;
	if(write(rtmpfd,data,con_len)!=con_len){
		printf("send \"releaseStream\" error:%s\n",strerror(errno));
		return -1;
	}
	//recvive _result
	memset(data,0,sizeof(data));
	if(getresult(rtmpfd,data)<0)
		return -1;
	return 1;
	
}

int parseObject2(char data[],int readk){
	int name_len=0;
	unsigned char* co;
	int i=0;
	while(1){
		if(data[readk++]==0x00&&data[readk++]==0x00){
			if(data[readk++]==0x09){
				printf("End of the Object\n");
				break;
			}
		}
		else{
			name_len=0;
			co=(unsigned char*)&name_len;
			co[1]=data[readk++];
			c0[0]=data[readk++];
			i=0;
			printf("Property \"");
			for(;i<name_len;i++)
				printf("%c",data[reak++]);
			printf("\"\n");
			if(data[readk++]==0x00){
				double value;
				co=(unsigned char*)&value;
				co[7]=data[readk++];
				co[6]=data[readk++];
				co[5]=data[readk++];
				co[4]=data[readk++];
				co[3]=data[readk++];
				co[2]=data[readk++];
				co[1]=data[readk++];
				co[0]=data[readk++];
				printf("double value %lf\n",value);
			}
			else if(data[readk++]==0x02){
				int len=0;
				co=(unsigned char*)&len;
				co[1]=data[readk++];
				co[0]=data[readk++];
				i=0;
				printf("string ");
				for(;i<len;i++)
					printf("%c",data[reak++]);
				printf("\n");
			}
			else if(data[readk++]==0x03){
				readk=parseObject(data[],readk);
			}
			else{
				printf("parseObject has new type\n");
			}
		}
	}
	return readk;
}

int parsedata2(char data[],int datasize){
	int readk=0;
	int flag=0;
	while(readk<=datasize-1){
		if(data[readk++]==0x00){
			double value=0.0;
			unsigned char* co=(unsigned char*)&value;
			co[7]=data[readk++];
			co[6]=data[readk++];
			co[5]=data[readk++];
			co[4]=data[readk++];
			co[3]=data[readk++];
			co[2]=data[readk++];
			co[1]=data[readk++];
			co[0]=data[readk++];
			streamid=(int)value;
			printf("	the double data of status is: %lf\n",value);
		}
		else if(data[readk++]==0x01){
			printf("the boolean data of status is: %d\n",data[readk++]);
		}
		else if(data[readk++]==0x02){
			int data_len;
			int i=0;
			char name[30];
			data_len=data_len<<8|data[readk++];
			data_len=data_len<<8|data[readk++];
			printf("	the string data of status length: %d\n",data_len);
			//for(;i<data_len;i++)
				//printf("	the string data of _result content: %c\n",data[readk++]);
			memset(name,0,sizeof(name));
			for(;i<data_len;i++)
				name[i]=data[readk++];
			name[data_len]='\0';
			printf("	the string data of status content: %s\n",name);
			
		}
		else if(data[readk++]==0x03){
			readk=parseObject2(data,readk);
			
		}
		else if(data[readk++]==0x05){
			printf("	the data of status is NULL\n");
		}
		else{
			printf("	the data type of status is not know\n");
			break;
		}
	}
	return flag;
}

int getstatus(int rtmpfd,char data[]){
	
	memset(data,0,sizeof(data));
	if(read(rtmpfd,data,12)!=12){
		printf("	write \"Window Acknowledgement Size\" error:%S\n",strerror(errno));
		return -1;
	}
	int format;
	int CSID;
	int type;
	unsigned char* co;
	co=(unsigned char*)&format;
	co[0]=data[0]>>6&0x01;
	printf("RTMP format :%d\n",format);
	
	co=(unsigned char*)&CSID;
	co[0]=data[0]&0x3f;
	printf("RTMP CSID :%d\n",CSID);
	
	co=(unsigned char*)&type;
	co[0]=data[7];
	printf("RTMP type :%d\n",type);
	
	datasize=0;
	co=(unsigned char*)&datasize;
	co[2]=data[4];
	co[1]=data[5];
	co[0]=data[6];
	printf("datasize %d\n",datasize);
	// if(type==5){
		// printf("Window Acknowledgement Size\n");
		// if(read(rtmpfd,data,datasize)!=datasize){
			// printf("	write \"Window Acknowledgement Size\" error:%S\n",strerror(errno));
			// return -1;
		// }
		// int windowsize=0;
		// co=(unsigned char*)&windowsize;
		// co[3]=data[0];
		// co[2]=data[1];
		// co[1]=data[2];
		// co[0]=data[3];
		// printf("Window Size is :%d\n",windowsize);
	// }
	// else if(type==6){
		// printf("Set Peer Bandwidth\n");
		// if(read(rtmpfd,data,datasize)!=datasize){
			// printf("	write \"Set Peer Bandwidth\" error:%S\n",strerror(errno));
			// return -1;
		// }
		// int windowsize=0;
		// co=(unsigned char*)&windowsize;
		// co[3]=data[0];
		// co[2]=data[1];
		// co[1]=data[2];
		// co[0]=data[3];
		// printf("Window Size is :%d\n",windowsize);
		// if(data[4]==0x02)
			// printf("Limit type: Dynamic(2)\n");
	// }
	// else if(type==1){
		// printf("Set Chunk Size\n");
		// if(read(rtmpfd,data,datasize)!=datasize){
			// printf("	write \"Set Chunk Size\" error:%S\n",strerror(errno));
			// return -1;
		// }
		// int windowsize=0;
		// co=(unsigned char*)&windowsize;
		// co[3]=data[0];
		// co[2]=data[1];
		// co[1]=data[2];
		// co[0]=data[3];
		// chunksize=windowsize;
		// printf("Window Size is :%d\n",windowsize);
	// }
	if(type==20){
		memset(data,0,sizeof(data));
		if(read(rtmpfd,data,datasize)!=datasize){
			printf("	write \"Set Chunk Size\" error:%S\n",strerror(errno));
			return -1;
		}
		if(parsedata2(data,datasize)<0)
			return -1;
	}
	return 1;
}

int sendpacket(int rtmpfd,FILE* fd){
	char data[200];
	unsigned char* co;
	int con_len=0;
	char name[50];
	//send publish
	memset(data,0,sizeof(data));
	data[0]=0x04;
	data[7]=0x14;
	co=(unsigned char*)&streamid;
	data[8]=co[3];
	data[9]=co[2];
	data[10]=co[1];
	data[11]=co[0];
	con_len=12;
	memset(name,0,sizeof(data));
	strcpy(name,"publish");
	con_len=addstring(data,con_len,name,0,NULL);
	con_len=addDouble(data,con_len,NULL,5);
	data[con_len++]=0x05;
	memset(name,0,sizeof(data));
	strcpy(name,"test");
	con_len=addstring(data,con_len,name,0,NULL);
	memset(name,0,sizeof(data));
	strcpy(name,"live");
	con_len=addstring(data,con_len,name,0,NULL);
	con_len-=12;
	co=(unsigned char*)&con_len;
	data[4]=co[2];
	data[5]=co[1];
	data[6]=co[0];
	con_len+=12;
	if(write(rtmpfd,data,con_len)!=con_len){
		printf("send \"releaseStream\" error:%s\n",strerror(errno));
		return -1;
	}
	//recv Netstream.Publish.Start
	
	if(getstatus(rtmpfd,data)<0){
		printf("Publish Start faile\n");
		return -1;
	}
	
	//send media tag
}

int main(int argc,char* argv[]){

	//TCP 握手
	char rtmpurl[20]="rtmp://10.99.1.140:1935/live/test";
	char filepath[50]="text.flv";
	FILE* fd;
	if((fd=fopen(filepath,"rb"))==NULL){
		printf("fopen error:%s\n",strerror(errno));
		exit(1);
	}
	struct ConnectObj con_obj;
	memset(con_obj.app,0,sizeof(con_obj.app));
	memset(con_obj.flashver,0,sizeof(con_obj.flashver));
	memset(con_obj.swfUrl,0,sizeof(con_obj.swfUrl));
	memset(con_obj.tcUrl,0,sizeof(con_obj.tcUrl));
	memset(con_obj.pageUrl,0,sizeof(con_obj.pageUrl));
	con_obj.fpad=0;
	con_obj.audioCodecs=0;
	con_obj.videoCodecs=0;
	con_obj.videoFunction=0;
	con_obj.objectEncoding=0;
	//解析rtmpurl  ??
	con_obj.app="live";
	con_obj.tcUrl="rtmp://10.99.1.140:1935/live";
	
	
	int rtmpfd=TCP_connect(rtmpurl);
	if(rtmpfd<0){
		printf("TCP connect faile\n");
		exit(1);
	}

	//rtmp connect
	my_RTMP_connect(rtmpfd,&con_obj);
	
	my_ConnectStream(rtmpfd,&con_obj);
	
	sendpacket(rtmpfd,fd);
	
	close(rtmpfd);
	exit(0);
}