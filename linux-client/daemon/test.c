#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/capi.h>
#include <signal.h>

#include "../module/capi20proxy.h"
#include "../module/capiutil.h"

static void sighandler(int);

static const int BUFFER_LEN=CAPI_MANUFACTURER_LEN+CAPI_SERIAL_LEN+sizeof(struct capi_version)+sizeof(struct capi_profile);
static int shutdown=0;

int main(void)
{
	int node, file;
	char *infobuffer, *msgbuffer;
	int ret,i=0;
	
	infobuffer=(char*)malloc(BUFFER_LEN+sizeof(int));
	msgbuffer=(char*)malloc(10000);
	
	signal(SIGINT, sighandler);
	
	file=open("capiinfo.buf",O_RDONLY);
	if(file==-1) {
		perror("opening file");
		return 1;
	}
	
	memcpy(infobuffer,&BUFFER_LEN,sizeof(int));
	ret=read(file,infobuffer+sizeof(int),BUFFER_LEN);

	node=open("/dev/capiproxy", O_NONBLOCK );

	ioctl(node,IOCTL_SET_DATA,infobuffer);

	while(i<5 && !shutdown) {
		__u16 len;
		__u8 cmd,subcmd;
		__u16 appl;
		
		sleep(1);
		i++;
		
		if(read(node,msgbuffer,10000)<=0)
			continue;
		
		appl=CAPIMSG_APPID(msgbuffer);
		printf("received message from appl %d\n",appl);

		if(CAPIMSG_COMMAND(msgbuffer)==TYPE_PROXY) {
			switch(CAPIMSG_SUBCOMMAND(msgbuffer))
			{
				case TYPE_REGISTER_APP:
					printf("TYPE: REGISTER_APP\n");
					ioctl(node,IOCTL_APPL_REGISTERED,(void*)(__u32)appl);
					break;
				case TYPE_RELEASE_APP:
					printf("TYPE: RELEASE_APP\n");
					break;
				
				case TYPE_SHUTDOWN:
					printf("TYPE: SHUTDOWN\n");
					shutdown=1;
					break;
				default:
					break;
			}
		}
			
	}
	
	printf("going down\n");
		
	free(infobuffer);
	free(msgbuffer);
	close(node);
	return 0;
}

static void sighandler(int signal)
{
	switch(signal)
	{
	case SIGINT:
		printf("received SIGINT, quitting\n");
		shutdown=1;
		break;
	default:
		break;
	}
}
