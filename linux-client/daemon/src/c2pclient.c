#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/capi.h>
#include <signal.h>
#include <stdlib.h>

#include "capi20proxy.h"
#include "protocol.h"

static void sighandler(int);

struct _c2p_init_data
{
	int length;
	char manufacturer[CAPI_MANUFACTURER_LEN];
	struct capi_version version;
	char serial[CAPI_SERIAL_LEN];
	struct capi_profile profile;
};

static char sendbuffer[10000];
static char recvbuffer[10000];
static int c2p_shutdown=0;
static int sc;
static unsigned int msgid=1;
static unsigned session_id;
static int node;
static __u16 applmap_rtl[CAPI_MAXAPPL+1], applmap_ltr[CAPI_MAXAPPL+1]; // ich bin faul!

void wait_for_answer();

int remote_get_manufacturer(char* manu)
{
	// get manufacturer from server

	struct REQUEST_HEADER *rheader;
	struct REQUEST_CAPI_MANUFACTURER *rbody;
	struct ANSWER_HEADER *aheader;
	struct ANSWER_CAPI_MANUFACTURER *abody;
	int ret;
	
	rheader=(struct REQUEST_HEADER*)sendbuffer;
	rbody=(struct REQUEST_CAPI_MANUFACTURER*)(sendbuffer+sizeof(struct REQUEST_HEADER));

	rheader->header_len=sizeof(struct REQUEST_HEADER);
	rheader->body_len=0;
	rheader->data_len=0;
	rheader->message_len=rheader->header_len+rheader->body_len+rheader->data_len;

	rheader->message_id=++msgid;
	rheader->message_type=TYPE_CAPI_MANUFACTURER;
	rheader->app_id=0;
	rheader->controller_id=1;
	rheader->session_id=session_id;

	ret=send(sc,sendbuffer,rheader->message_len,0);
	if(ret<1) {
		perror("sending message");
		return 0;
	}

	wait_for_answer(TYPE_CAPI_MANUFACTURER);

	aheader=(struct ANSWER_HEADER*)recvbuffer;
	abody=(struct ANSWER_CAPI_MANUFACTURER*)(recvbuffer+aheader->header_len);

	if(aheader->proxy_error!=PERROR_NONE) {
		printf("server error\n");
		return 0;
	}
	
	memcpy(manu,abody->manufacturer,CAPI_MANUFACTURER_LEN);
	
	printf("manufacturer is %s\n",manu);
	return 1;
}
	
int remote_get_version(char* version)
{
	// get version from server
	
	struct REQUEST_HEADER *rheader;
	struct REQUEST_CAPI_VERSION *rbody;
	struct ANSWER_HEADER *aheader;
	struct ANSWER_CAPI_VERSION *abody;
	int ret;
	
	rheader=(struct REQUEST_HEADER*)sendbuffer;
	rbody=(struct REQUEST_CAPI_VERSION*)(sendbuffer+sizeof(struct REQUEST_HEADER));

	rheader->header_len=sizeof(struct REQUEST_HEADER);
	rheader->body_len=0;
	rheader->data_len=0;
	rheader->message_len=rheader->header_len+rheader->body_len+rheader->data_len;

	rheader->message_id=++msgid;
	rheader->message_type=TYPE_CAPI_VERSION;
	rheader->app_id=0;
	rheader->controller_id=1;
	rheader->session_id=session_id;

	ret=send(sc,sendbuffer,rheader->message_len,0);
	if(ret<1) {
		perror("sending message");
		return 0;
	}

	wait_for_answer(TYPE_CAPI_VERSION);

	aheader=(struct ANSWER_HEADER*)recvbuffer;
	abody=(struct ANSWER_CAPI_VERSION*)(recvbuffer+aheader->header_len);

	if(aheader->proxy_error!=PERROR_NONE) {
		printf("server error\n");
		return 0;
	}
	
	memcpy(version,abody,sizeof(struct capi_version));
	return 1;
}

int remote_get_serial(char* serial)
{
	// get serial from server
	
	struct REQUEST_HEADER *rheader;
	struct REQUEST_CAPI_SERIAL *rbody;
	struct ANSWER_HEADER *aheader;
	struct ANSWER_CAPI_SERIAL *abody;
	int ret;
	
	rheader=(struct REQUEST_HEADER*)sendbuffer;
	rbody=(struct REQUEST_CAPI_SERIAL*)(sendbuffer+sizeof(struct REQUEST_HEADER));

	rheader->header_len=sizeof(struct REQUEST_HEADER);
	rheader->body_len=0;
	rheader->data_len=0;
	rheader->message_len=rheader->header_len+rheader->body_len+rheader->data_len;

	rheader->message_id=++msgid;
	rheader->message_type=TYPE_CAPI_SERIAL;
	rheader->app_id=0;
	rheader->controller_id=1;
	rheader->session_id=session_id;

	ret=send(sc,sendbuffer,rheader->message_len,0);
	if(ret<1) {
		perror("sending message");
		return 0;
	}

	wait_for_answer(TYPE_CAPI_SERIAL);	

	aheader=(struct ANSWER_HEADER*)recvbuffer;
	abody=(struct ANSWER_CAPI_SERIAL*)(recvbuffer+aheader->header_len);

	if(aheader->proxy_error!=PERROR_NONE) {
		printf("server error\n");
		return 0;
	}
	
	memcpy(serial,abody->serial,CAPI_SERIAL_LEN);
	
	printf("serial is %s\n",serial);
	return 1;
}

int remote_get_profile(char* profile)
{
	// get profile from server
	
	struct REQUEST_HEADER *rheader;
	struct REQUEST_CAPI_PROFILE *rbody;
	struct ANSWER_HEADER *aheader;
	struct ANSWER_CAPI_PROFILE *abody;
	int ret;
	
	rheader=(struct REQUEST_HEADER*)sendbuffer;
	rbody=(struct REQUEST_CAPI_PROFILE*)(sendbuffer+sizeof(struct REQUEST_HEADER));

	rheader->header_len=sizeof(struct REQUEST_HEADER);
	rheader->body_len=0;
	rheader->data_len=0;
	rheader->message_len=rheader->header_len+rheader->body_len+rheader->data_len;

	rheader->message_id=++msgid;
	rheader->message_type=TYPE_CAPI_PROFILE;
	rheader->app_id=0;
	rheader->controller_id=1;
	rheader->session_id=session_id;

	ret=send(sc,sendbuffer,rheader->message_len,0);
	if(ret<1) {
		perror("sending message");
		return 0;
	}

	wait_for_answer(TYPE_CAPI_PROFILE);	

	aheader=(struct ANSWER_HEADER*)recvbuffer;
	abody=(struct ANSWER_CAPI_PROFILE*)(recvbuffer+aheader->header_len);

	if(aheader->proxy_error!=PERROR_NONE) {
		printf("server error\n");
		return 0;
	}
	
	memcpy(profile,abody->profile,sizeof(struct capi_profile));
	
	return 1;
}

int remote_waitforsignal(__u16 appl)
{
	// wait for message from server

	struct REQUEST_HEADER *rheader;
	struct REQUEST_CAPI_WAITFORSIGNAL *rbody;
	int ret;
	
	rheader=(struct REQUEST_HEADER*)sendbuffer;
	rbody=(struct REQUEST_CAPI_WAITFORSIGNAL*)(sendbuffer+sizeof(struct REQUEST_HEADER));

	rheader->header_len=sizeof(struct REQUEST_HEADER);
	rheader->body_len=sizeof(struct REQUEST_CAPI_WAITFORSIGNAL);
	rheader->data_len=0;
	rheader->message_len=rheader->header_len+rheader->body_len+rheader->data_len;

	rheader->message_id=++msgid;
	rheader->message_type=TYPE_CAPI_WAITFORSIGNAL;
	rheader->app_id=appl;
	rheader->controller_id=1;
	rheader->session_id=session_id;

	ret=send(sc,sendbuffer,rheader->message_len,0);
	if(ret<1) {
		perror("sending message");
		return 0;
	}

	return 1;
}

int remote_register_appl(__u16 *appl, __u32 maxLogicalConnection, __u32 maxBDataBlocks, __u32 maxBDataLen)
{
	// register an application on the server

	struct REQUEST_HEADER *rheader;
	struct REQUEST_CAPI_REGISTER *rbody;
	struct ANSWER_HEADER *aheader;
	struct ANSWER_CAPI_REGISTER *abody;
	int ret;
	
	rheader=(struct REQUEST_HEADER*)sendbuffer;
	rbody=(struct REQUEST_CAPI_REGISTER*)(sendbuffer+sizeof(struct REQUEST_HEADER));

	rheader->header_len=sizeof(struct REQUEST_HEADER);
	rheader->body_len=sizeof(struct REQUEST_CAPI_REGISTER);
	rheader->data_len=0;
	rheader->message_len=rheader->header_len+rheader->body_len+rheader->data_len;

	rheader->message_id=++msgid;
	rheader->message_type=TYPE_CAPI_REGISTER;
	rheader->app_id=0;
	rheader->controller_id=1;
	rheader->session_id=session_id;

	rbody->messageBufferSize=1024*maxLogicalConnection;
	rbody->maxLogicalConnection=maxLogicalConnection;
	rbody->maxBDataBlocks=maxBDataBlocks;
	rbody->maxBDataLen=maxBDataLen;

	ret=send(sc,sendbuffer,rheader->message_len,0);
	if(ret<1) {
		perror("sending message");
		return 0;
	}

	wait_for_answer(TYPE_CAPI_REGISTER);	

	aheader=(struct ANSWER_HEADER*)recvbuffer;
	abody=(struct ANSWER_CAPI_REGISTER*)(recvbuffer+aheader->header_len);

	if(aheader->proxy_error!=PERROR_NONE) {
		printf("server error\n");
		return 0;
	}

	*appl=aheader->app_id;
	
	remote_waitforsignal(*appl);
	
	return 1;
}

int remote_release_appl(__u16 appl)
{
	// release an application on the server

	struct REQUEST_HEADER *rheader;
	struct REQUEST_CAPI_RELEASE *rbody;
	struct ANSWER_HEADER *aheader;
	struct ANSWER_CAPI_RELEASE *abody;
	int ret;
	
	rheader=(struct REQUEST_HEADER*)sendbuffer;
	rbody=(struct REQUEST_CAPI_RELEASE*)(sendbuffer+sizeof(struct REQUEST_HEADER));

	rheader->header_len=sizeof(struct REQUEST_HEADER);
	rheader->body_len=sizeof(struct REQUEST_CAPI_RELEASE);
	rheader->data_len=0;
	rheader->message_len=rheader->header_len+rheader->body_len+rheader->data_len;

	rheader->message_id=++msgid;
	rheader->message_type=TYPE_CAPI_RELEASE;
	rheader->app_id=appl;
	rheader->controller_id=1;
	rheader->session_id=session_id;

	ret=send(sc,sendbuffer,rheader->message_len,0);
	if(ret<1) {
		perror("sending message");
		return 0;
	}

	wait_for_answer(TYPE_CAPI_RELEASE);	

	aheader=(struct ANSWER_HEADER*)recvbuffer;
	abody=(struct ANSWER_CAPI_RELEASE*)(recvbuffer+aheader->header_len);

	if(aheader->proxy_error!=PERROR_NONE) {
		printf("server error\n");
		return 0;
	}

	return 1;
}

int remote_get_message(__u16 appl)
{
	// get message from server

	struct REQUEST_HEADER *rheader;
	struct REQUEST_CAPI_GETMESSAGE *rbody;
	int ret;
	
	rheader=(struct REQUEST_HEADER*)sendbuffer;
	rbody=(struct REQUEST_CAPI_GETMESSAGE*)(sendbuffer+sizeof(struct REQUEST_HEADER));

	rheader->header_len=sizeof(struct REQUEST_HEADER);
	rheader->body_len=sizeof(struct REQUEST_CAPI_GETMESSAGE);
	rheader->data_len=0;
	rheader->message_len=rheader->header_len+rheader->body_len+rheader->data_len;

	rheader->message_id=++msgid;
	rheader->message_type=TYPE_CAPI_GETMESSAGE;
	rheader->app_id=appl;
	rheader->controller_id=1;
	rheader->session_id=session_id;

	ret=send(sc,sendbuffer,rheader->message_len,0);
	if(ret<1) {
		perror("sending message");
		return 0;
	}

	return 1;
}


int send_to_module(int len)
{
	__u16 appl,data_offset;
	struct ANSWER_HEADER* aheader;
	
	aheader=(struct ANSWER_HEADER*)recvbuffer;
	
	// strip protocol header
	data_offset=aheader->header_len+aheader->body_len;
	
	memcpy(sendbuffer,recvbuffer+data_offset,aheader->data_len);
	
	//map applid
	CAPIMSG_SETAPPID(sendbuffer,applmap_rtl[CAPIMSG_APPID(sendbuffer)]);
	appl=CAPIMSG_APPID(sendbuffer);
	printf("message to appl %d\n",appl);

	if(write(node,sendbuffer,aheader->data_len)<1) {
		perror("sending message to module");
		return 0;
	}

	return 1;
}

int local_register_appl(__u16 local_appid)
{
	__u16 remote_appid;
		
	if(!remote_register_appl(&remote_appid,
				CAPIMSG_L3C(recvbuffer),
				CAPIMSG_DBC(recvbuffer),
				CAPIMSG_DBL(recvbuffer)
				)) {
		return 0;
	}

	printf("local appl %d is remote appl %d\n",local_appid,remote_appid);
	
	applmap_rtl[remote_appid]=local_appid;
	applmap_ltr[local_appid]=remote_appid;
	
	return 1;
}

int local_release_appl(__u16 appl)
{
	return remote_release_appl(appl);
}

int init_module()
{
	struct _c2p_init_data infobuffer;
	if(	!remote_get_manufacturer(infobuffer.manufacturer) ||
		!remote_get_version((char*)&infobuffer.version) ||
		!remote_get_serial(infobuffer.serial) ||
		!remote_get_profile((char*)&infobuffer.profile)
	) {
		printf("error initializing\n");
		return 0;
	}
	infobuffer.length=sizeof(struct _c2p_init_data)-sizeof(int);
	
	node=open("/dev/capiproxy", O_NONBLOCK);
	ioctl(node,IOCTL_SET_DATA,&infobuffer);

	return 1;
}

int init_socket()
{
	int ret;
	struct sockaddr_in addr;
	struct REQUEST_HEADER *rheader;
	struct REQUEST_PROXY_HELO *rbody;
	struct ANSWER_HEADER *aheader;
	struct ANSWER_PROXY_HELO *abody;
	
	sc = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sc<1) {
		perror("creating socket");
		return 0;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("10.0.0.1");
	addr.sin_port = htons(6674);
	
	ret = connect(sc, (struct sockaddr*) &addr, sizeof(struct sockaddr_in));
	if(ret<0) {
		perror("connecting to server");
		return 0;
	}
	
	// send helo to server
	
	rheader=(struct REQUEST_HEADER*)sendbuffer;
	rbody=(struct REQUEST_PROXY_HELO*)(sendbuffer+sizeof(struct REQUEST_HEADER));

	rheader->header_len=sizeof(struct REQUEST_HEADER);
	rheader->body_len=sizeof(struct REQUEST_PROXY_HELO);
	rheader->data_len=0;
	rheader->message_len=rheader->header_len+rheader->body_len+rheader->data_len;

	rheader->message_id=++msgid;
	rheader->message_type=TYPE_PROXY_HELO;
	rheader->app_id=0;
	rheader->controller_id=0;
	rheader->session_id=0;

	strcpy(rbody->name,"Freddy");
	rbody->os=OS_TYPE_LINUX;
	rbody->version.major=1;
	rbody->version.minor=1;

	ret=send(sc,sendbuffer,rheader->message_len,0);
	if(ret<1) {
		perror("sending message");
		return 0;
	}

	ret=recv(sc,recvbuffer,10000,0);
	if(ret<1) {
		perror("receiving message");
		return 0;
	}

	aheader=(struct ANSWER_HEADER*)recvbuffer;
	abody=(struct ANSWER_PROXY_HELO*)(recvbuffer+aheader->header_len);

	if(aheader->proxy_error!=PERROR_NONE) {
		printf("server error\n");
		return 0;
	}
	
	session_id=aheader->session_id;
	printf("session id is %x\n",session_id);
	return 1;
}

int handle_serverpacket(int len)
{
	struct ANSWER_HEADER* aheader;
	
	aheader=(struct ANSWER_HEADER*)recvbuffer;
		
	switch(aheader->message_type)
	{
		case TYPE_CAPI_WAITFORSIGNAL:
			return remote_get_message(aheader->app_id);

		case TYPE_CAPI_GETMESSAGE:
			send_to_module(len);
			return remote_waitforsignal(aheader->app_id);

		default:
			printf("wrong message type from server: %d\n",aheader->message_type);
			return 0;
	}

	return 0;
}

void wait_for_answer(int type)
{
	int len;
	struct ANSWER_HEADER *aheader;
	
	do {
		len=recv(sc,recvbuffer,10000,0);
		if(len<1) {
			perror("receiving message");
			return;
		}
		
		aheader=(struct ANSWER_HEADER*)recvbuffer;
		if(aheader->message_type!=type) {
			handle_serverpacket(len);
		}
		
	} while(aheader->message_type!=type);
}

int send_to_server(int len)
{
	__u32 ncci;
	struct REQUEST_HEADER *rheader;
	struct REQUEST_CAPI_PUTMESSAGE *rbody;
	struct ANSWER_HEADER *aheader;
	struct ANSWER_CAPI_PUTMESSAGE *abody;
	int ret;
	__u16 data_offset;

	//map applid
	CAPIMSG_SETAPPID(recvbuffer,applmap_ltr[CAPIMSG_APPID(recvbuffer)]);
	
	//set controllerid
	ncci = CAPIMSG_NCCI(recvbuffer);
	ncci &= 0xFFFFFF80;
	ncci += 1;	// set controller id 1

	CAPIMSG_SETCONTROL(recvbuffer,ncci);
		
	rheader=(struct REQUEST_HEADER*)sendbuffer;
	rbody=(struct REQUEST_CAPI_PUTMESSAGE*)(sendbuffer+sizeof(struct REQUEST_HEADER));

	rheader->header_len=sizeof(struct REQUEST_HEADER);
	rheader->body_len=sizeof(struct REQUEST_CAPI_PUTMESSAGE);
	rheader->data_len=len;
	rheader->message_len=rheader->header_len+rheader->body_len+rheader->data_len;

	rheader->message_id=++msgid;
	rheader->message_type=TYPE_CAPI_PUTMESSAGE;
	rheader->app_id=CAPIMSG_APPID(recvbuffer);
	rheader->controller_id=1;
	rheader->session_id=session_id;

	data_offset=rheader->header_len+rheader->body_len;
	memcpy(sendbuffer+data_offset,recvbuffer,len);
	
	ret=send(sc,sendbuffer,rheader->message_len,0);
	if(ret<1) {
		perror("sending message");
		return 0;
	}

	wait_for_answer(TYPE_CAPI_PUTMESSAGE);
	
	aheader=(struct ANSWER_HEADER*)recvbuffer;
	abody=(struct ANSWER_CAPI_PUTMESSAGE*)(recvbuffer+aheader->header_len);

	if(aheader->proxy_error!=PERROR_NONE) {
		printf("server error\n");
		return 0;
	}

	return 1;
}

int main(void)
{
	int ret;

	signal(SIGINT, sighandler);
	
	ret=init_socket();
	ret=init_module();

	
	
	while(!c2p_shutdown) {
		int len;
		//__u8 cmd,subcmd;
		__u16 appl;
		
		
		fcntl(sc,F_SETFL,O_NONBLOCK);
		if(!((len=recv(sc,recvbuffer,10000,0))<0))
		{
			handle_serverpacket(len);
		}			
		fcntl(sc,F_SETFL,0);
			
		if((len=read(node,recvbuffer,10000))<=0)
			continue;
		
		appl=CAPIMSG_APPID(recvbuffer);

		if(CAPIMSG_COMMAND(recvbuffer)==TYPE_PROXY) {
			switch(CAPIMSG_SUBCOMMAND(recvbuffer))
			{
				case TYPE_REGISTER_APP:
					ret=local_register_appl(appl);
					if(ret) {
						ioctl(node,IOCTL_APPL_REGISTERED,(void*)(__u32)appl);
					} else {
						ioctl(node,IOCTL_ERROR_REGFAIL,(void*)(__u32)appl);
					}
					break;
				
				case TYPE_RELEASE_APP:
					ret=local_release_appl(appl);
					break;
				
				case TYPE_SHUTDOWN:
					c2p_shutdown=1;
					break;
				default:
					break;
			}
		} else {
			printf("message from appl %d\n",appl);
			send_to_server(len);
		}
	}
	
	printf("going down\n");
		
	close(node);
	close(sc);
	return 0;
}

static void sighandler(int signal)
{
	switch(signal)
	{
	case SIGINT:
		printf("received SIGINT, quitting\n");
		c2p_shutdown=1;
		break;
	default:
		break;
	}
}
