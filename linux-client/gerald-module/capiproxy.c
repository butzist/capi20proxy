/*
 *   capi20proxy linux client module (Provides a remote CAPI port over TCP/IP)
 *   Copyright (c) 2002. Begumisa Gerald. All rights reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 *   IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 *   OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *   IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 *   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *   NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *   THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *   See the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/config.h>
#include <linux/smp_lock.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <kernelcapi.h>
#include <capilli.h>

#include "capiutils.h"
#include "capicmd.h"

EXPORT_NO_SYMBOLS;

#define CAPIPROXY_VERSIONLEN	1 /* One pointer so far */
#define CAPIPROXY_VER_DRIVER	0 /* the offset of the pointer to the driver version */

#define CAPIPROXY_MAJOR		69
#define MAX_CONTROLLERS		16
#define MSG_QUEUE_LENGTH	16

#define TYPE_MESSAGE		0x01
#define TYPE_REGISTER_APP	0x02
#define TYPE_RELEASE_APP	0x03
#define TYPE_SHUTDOWN		0x04

#define IOCTL_SET_DATA		0x01
/*
 * An error message sent by the daemon telling
 * us that the application's registration failed
 * and hence we should call ctrl->appl_released()
 */
#define IOCTL_ERROR_REGFAIL     0x02

#define APPL(a)			(&applications[(a)-1])
#define VALID_APPLID(a)		((a) && (a) <= CAPI_MAXAPPL && APPL(a)->applid == a)
#define APPL_IS_FREE(a)		(APPL(a)->applid == 0)
#define APPL_MARK_FREE(a)	do{ APPL(a)->applid=0; MOD_DEC_USE_COUNT; }while(0);
#define APPL_MARK_USED(a)	do{ APPL(a)->applid=(a); MOD_INC_USE_COUNT; }while(0);

/*
 * Before we send any messages to the server, we
 * determine whether it has been registered
 */

#define APPL_CONFIRMED		0 /* Server registered the application */
#define APPL_REVOKED		1 /* Server failed to register the application */
#define APPL_WAITING		2 /* Server has not yet answered */

#define CARD(c)			(&cards[(c)-1])
#define CARD_FREE		0
#define CARD_RUNNING		1
#define VALID_CARD(c)		(CARD(c)->status)

/*
 * Error codes for possible ioctl() errors
 * to be sent to the daemon
 */

#define ENOTREG			1 /* Controller not registered */

MODULE_DESCRIPTION("CAPI Proxy Client driver");
MODULE_AUTHOR("Begumisa Gerald (beg_g@eahd.or.ug)");
MODULE_SUPPORTED_DEVICE("kernelcapi");

typedef struct __capi20proxy_card
{
	__u8 status; 
	__u16 ctrl_id;	/* This corresponds exactly to ctrl->cnr */

	struct capi_ctr *ctrl;
	struct sk_buff_head outgoing_queue;
	wait_queue_head_t wait_queue_out;

	struct capiproxyctrl_info {
		char cardname[32];
		spinlock_t ctrl_lock;
		int versionlen; 	/* Length of the version in versionbuf*/
		char versionbuf[8];
		char *version[CAPIPROXY_VERSIONLEN];
		char infobuf[128];      /* for function procinfo */
		struct capi20proxy_card *card;
		int in_sk, out_sk;      /* buffer count */
		int sk_pending;         /* number of out-going buffers currently in queue */
	} *pcapiproxyctrl_info;
} capi20proxy_card;

typedef struct __capiproxy_appl {
	capi_register_params 	rp;
	__u16 			applid;		/* kcapi application identifier */
	__u8			status;		/* tells our module whether this appl is up */
} capiproxy_appl;

static capi20proxy_card cards[CAPI_MAXCONTR];
static capiproxy_appl applications[CAPI_MAXAPPL];
static spinlock_t kernelcapi_lock;
static struct capi_driver_interface *di;

/*
 * Wait queue for any call to rmmod while there
 * are tasks in the queue
 */

wait_queue_head_t rmmod_queue;

/*
 * This helps the above cause
 */

static __u16 system_shutting_down = 0;

static char *main_revision	= "$Revision$";
static char *capiproxy_version	= "1.2";
static char *DRIVERNAME		= "capi20proxy";
static char *DRIVERLNAME	= "Capi 2.0 Proxy Client (http://capi20proxy.sourceforge.net)";
static char *CARDNAME		= "CAPI Virtual Card";
static char *MANUFACTURER_NAME	= "http://capi20proxy.sourceforge.net";

static void capiproxy_flush_queue(capi20proxy *this_card)
{
	struct sk_buff *skb;
	struct capiproxyctrl_info *cinfo = this_card->pcapiproxyctrl_info;

	while ((skb = skb_dequeue(&this_card->outgoing_queue)) != 0)
		kfree_skb(skb);

	spin_lock(cinfo->ctrl_lock);
	cinfo->sk_pending = 0;
	spin_unlock(cinfo->ctrl_lock);
}

void capiproxy_remove_ctr(struct capi_ctr *ctrl);
{
	struct sk_buff *skb;
	__u16 len;
	__u16 applid;
	__u8 command, subcommand;	

	applid = 0;

	/*
	 * Tell the daemon listening on this controller
	 * that we're shutting down the controller
	 */
	command = TYPE_SHUTDOWN;
	subcommand = 0;
	len = sizeof(__u16)*2 + sizeof(__u8)*2;
	skb = alloc_skb(len+1, GFP_ATOMIC);

	CAPIMSG_SETLEN(skb->data, len);
	CAPIMSG_SETAPPID(skb->data, appl);
	CAPIMSG_SETCOMMAND(skb->data, command);
	CAPIMSG_SETSUBCOMMAND(skb->data, subcommand);
	
	/*
	 * Do we really need to lock kernel capi at this
	 * point?  My hunch was that once we suspend the
	 * output to the card, nothing shady should 
	 * happen
	 */

	ctrl->suspend_output(ctrl);
	capiproxy_flush_queue(CARD(ctrl->cnr));
	capiproxy_send_message(ctrl, skb);
	di->detach_ctr(ctrl);
	CARD(ctrl->cnr)->status = CARD_FREE;
}

void capiproxy_reset_ctr(struct capi_ctr *ctrl)
{
	capiproxy_card *card = CARD(ctrl->cnr);
	struct capiproxyctrl_info *cinfo = card->pcapiproxyctrl_info;

	capiproxy_flush_queue(card);
	capiproxy_remove_ctr(ctrl);
	ctrl->reseted(ctrl);
}

static void capiproxy_register_internal(struct capi_ctr *ctrl, __u16 appl,
                                        capi_register_params *rp)
{
	struct sk_buff *skb;
	__u16 len;
	__u8 command = TYPE_REGISTER_APP; /* Command to daemon to throw together a register
				    	   * kind of packet to send to the server
				    	   */
	__u8 subcommand = 0; /* Tells the daemon this is not an ordinary message */

	len = sizeof(__u16)*2 + sizeof(__u8)*2) + sizeof(__u32)*3;
	if(!(skb = alloc_skb(len + 1, GFP_ATOMIC))) {
		printk(KERN_ERR "capi20proxy card %d: Could not allocate memory", ctrl->cnr);
		return;
	}
	CAPIMSG_SETLEN(skb->data, len);
	CAPIMSG_SETAPPID(skb->data, appl);
	CAPIMSG_SETCOMMAND(skb->data, command);
	CAPIMSG_SETSUBCOMMAND(skb->data, subcommand);
	CAPIMSG_SETL3C(skb->data, rp->level3cnt);
	CAPIMSG_SETDBC(skb->data, rp->datablkcnt);
	CAPIMSG_SETDBC(skb->data, rp->datablklen);
	capiproxy_send_message(ctrl, skb);
}

void capiproxy_register_appl(struct capi_ctr *ctrl, 
				__u16 appl, 
				capi_register_params *rp)
{
	int	max_logical_connections = 0,
		max_b_data_blocks	= 0,
		max_b_data_len		= 0,
		chk			= 0;
	struct sk_buff *skb;
	capi_register_params *arp;

	if (!APPL_IS_FREE(appl)) {
		printk(KERN_ERR "capi20proxy: Application %d already registered\n", appl);
		return;
	}
	APPL(appl)->applid = appl;
	arp = &(APPL(appl)->rp);

	max_b_data_blocks = rp->datablkcnt > CAPI_MAXDATAWINDOW ? CAPI_MAXDATAWINDOW : rp->datablkcnt;	
	rp->datablkcnt = max_b_data_blocks;
	max_b_data_len = rp->datablklen < 1024 ? 1024 : rp->datablklen;
	rp->datablklen = max_b_data_len;

	arp->level3cnt  = rp->level3cnt;
	arp->datablkcnt = rp->datablkcnt;
	arp->datablklen = rp->datablklen;

	capiproxy_register_internal(ctrl, appl, arp);
	APPL_MARK_USED(appl);
	ctrl->appl_registered(ctrl, appl);
}

/*
 * I think if there is an error registering the application
 * then the daemon should send a message to this module
 * reporting the error because as far as kcapi is concerned,
 * our module told it that the application had been registered
 *
 * One problem is:- kcapi may send messages from that application
 * before it is confirmed.  The go-around I thought of was to
 * include the variable "status" in the applications structure.
 */

void capiproxy_release_appl(struct capi_ctr *ctrl, 
			    __u16 appl)
{
	if(!VALID_APPLID(appl) || APPL_IS_FREE(appl)) {
		printf(KERN_ERR "capi20proxy card %d: Releasing free application or invalid applid -- %d\n", ctrl->cnr, appl);
		return;
	}
	capiproxy_release_internal(ctrl, appl);
	APPL_MARK_FREE(appl);	
	ctrl->appl_released(ctrl, appl);
}

static void capiproxy_release_internal(struct capi_ctr *ctrl, 
				       __u16 appl)
{
	struct sk_buff *skb;
	__u16 len;
	__u8 command;
	__u8 subcommand;

	/*
	 * A sub command of zero should tell the daemon that this
	 * is not an ordinary capi message.
	 */

	command 	= TYPE_RELEASE_APP;
	subcommand 	= 0;

	len = sizeof(__u16)*2 + sizeof(__u8)*2;
	if(!(skb = alloc_skb(len+1, GFP_ATOMIC))) {
		printk(KERN_ERR "capi20proxy card %d: Could not allocate memory", ctrl->cnr);
		return;
	}
        CAPIMSG_SETLEN(skb->data, len);
        CAPIMSG_SETAPPID(skb->data, appl);
        CAPIMSG_SETCOMMAND(skb->data, command);
        CAPIMSG_SETSUBCOMMAND(skb->data, subcommand);
	capiproxy_send_message(ctrl, skb);
}

int capiproxy_capi_release(capi20proxy *card)
{
	struct capi_ctr *ctrl;

	ctrl = card->ctrl;
	printk(KERN_NOTICE "capi20proxy: %d released", ctrl->cnr);
	capiproxy_remove_ctr(ctrl);
	return 0;
}

void capiproxy_send_message(struct capi_ctr *ctrl,
				struct sk_buff *skb)
{
	capi20proxy *this_card = CARD(ctrl->cnr);;
	capiproxyctrl_info *cinfo = this_card->pcapiproxyctrl_info;
	__u16 applid;

	applid = CAPIMSG_APPID(skb->data);

	switch(CAPIMSG_COMMAND(skb->data)) {
		case CAPI_DISCONNECT_B3_RESP:
			ctrl->free_ncci(ctrl, applid,
					CAPIMSG_NCCI(skb->data));
			break;
		default:
			break;
	}	

	skb_queue_tail(&this_card->outgoing_queue, skb);
	spin_lock(cinfo->ctrl_lock);
	cinfo->sk_pending += 1;
	spin_unlock(cinfo->ctrl_lock);
	queue_task(&tq_send_notify, &tq_immediate);
	mark_bh(IMMEDIATE_BH);
}

/*
 * This does not load any firmware but the callback is 
 * needed on capi-interface registration
 */

int capiproxy_load_firmware(struct capi_ctr *ctrl,
				capiloaddata *data)
{
	return 0;
}

char *capiproxy_procinfo(struct capi_ctr *ctrl);
{
	struct capiproxyctrl_info *cinfo = (capiproxyctrl_info *)ctrl->driverdata;

	if (!cinfo)
		return "";

	sprintf(cinfo->infobuf, "%s %s 0x%x",
		cinfo->cardname[0] ? cinfo->cardname : "-",
		cinfo->version[CAPIPROXY_VER_DRIVER] ? cinfo->version[CAPIPROXY_VER_DRIVER] : "-",
		cinfo->card ? cinfo->card : 0x0
		);
		
	return cinfo->infobuf;
}

int capiproxy_read_proc(char *page,
			char **start,
			off_t off,
			int count,
			int *end,
			struct capi_ctr *ctrl)
{
	struct capiproxyctrl_info *cinfo = ctrl->driverdata;
	char *s;
	int len = 0;
	
	len += sprintf(page+len, "%-16s %s\n", "name", cinfo->cardname);
	len += sprintf(page+len, "%-16s 0x0\n", "io");
	len += sprintf(page+len, "%-16s -\n", "irq");
	len += sprintf(page+len, "%-16s %s\n", "type, "Virtual Card :-)");

	if ((s = cinfo->version[CAPIPROXY_VER_DRIVER]) != 0)
		len += sprintf(page+len, "%-16s %s\n", "ver_driver", s);

	len += sprintf(page+len, "%-16s %s\n", "cardname", cinfo->cardname);

	if (off+count >= len)
		*eof = 1;
	if (len < off)
		return 0;

	*start = page + off;
	return ((count < len-off) ? count : len-off);
}

static struct capi_driver capiproxy_driver = {
	"capi20proxy",
	"1.2",
	capiproxy_load_firmware,
	capiproxy_reset_ctr,
	capiproxy_remove_ctr,
	capiproxy_register_appl,
	capiproxy_release_appl,
	capiproxy_send_message,
	capiproxy_procinfo,
	capiproxy_read_proc,
	0,				/* Use standard driver read proc */
	0,				/* No add card function */
};

static void push_user_buffer(void **dest, void **src, int len)
{
	copy_from_user(*dest, *src, len);
	*dest += len; 
}

static int capiproxy_ioctl(struct inode *inode, 
				struct file *file, 
				unsigned int ioctl_num,
				unsigned long ioctl_param)
{
	char *temp = (char *)ioctl_param;
	capi20proxy_card *card = (capi20proxy_card *)file->private_data;
	struct capi_ctr *ctrl  = card->ctrl;
	__u16 applid, len;

	switch(ioctl_num) {
		case IOCTL_SET_DATA:
		{
			if (!ctrl)
				return -ENOTREG;

			copy_from_user(&len, temp, sizeof(int));
			temp += sizeof(int);

			if (len < CAPI_MANUFACTURER_LEN + CAPI_VERSION_LEN + CAPI_SERIAL_LEN + CAPI_PROFILE_LEN)
				return -EMSGSIZE;

			push_user_buffer((void **)(&(ctrl->manu)),
					(void **)(&temp),
					CAPI_MANUFACTURER_LEN);
			push_user_buffer((void **)(&(ctrl->version)),
					(void **)(&temp),
					CAPI_VERSION_LEN);
			push_user_buffer((void **)(&(ctrl->serial)),
					(void **)(&temp),
					CAPI_SERIAL_LEN);
			push_user_buffer((void **)(&(ctrl->profile)),
					(void **)(&temp),
					CAPI_PROFILE_LEN);
			ctrl->ready(ctrl);
			return 0;
		}
		case IOCTL_ERROR_REGFAIL:
		{
			applid = CAPIMSG_APPID(temp);			
			ctrl->appl_released(ctrl, appl);
			return 0;
		}
		default:
			return -EIO;
	}
}

static void handle_send_msg(void *dummy)
{
	int i;
	capi20proxy *card;
	capiproxyctrl_info *cinfo;

	for (i = 0; i < CAPI_MAXCONTR; i++) {
		card = &cards[i];
		if(card->status == CARD_FREE) 
			continue;

		cinfo = card->pcapiproxyctrl_info;
		if(!cinfo) 
			continue;

		if(cinfo->sk_pending)
			wake_up_interruptible(&card->wait_queue_out);
	}

	if (system_shutting_down) {
		system_shutting_down = 0;
		wake_up_interruptible(&rmmod_queue);
	}
}

/*
 * The daemon blocks at this function until data is
 * ready for transfer.  The daemon shall handle mapping
 * the application identifiers known to this (CLIENT's)
 * CAPI with the application identifiers known to the
 * SERVER's CAPI
 */

static int capiproxy_read(struct file *file,
				char *buffer,
				size_t length,
				loff_t *offset)
{
	capi20proxy_card *card = (capi20proxy_card *)file->private_data;
	struct capi_ctr *ctrl = card->ctrl;
	capiproxyctrl_info *cinfo = card->pcapiproxyctrl_info;
	capiproxy_appl *this_application;
	struct sk_buff *skb;
	int retval;
	__u16 applid;
	size_t copied;

	if (!cinfo->sk_pending) {
		if(file->f_flags & O_NONBLOCK)
			return -EAGAIN;
	
		for (;;) {
			interruptible_sleep_on(&card->wait_queue_out);

			if (cinfo->sk_pending)
				break;
			if (signal_pending(current))
				break;
		}

		if (!cinfo->sk_pending)
			return -EINTR;
	}
	skb = skb_dequeue(&card->outgoing_queue); 

	if (skb->len > length) {
		skb_queue_head(&card->outgoing_queue);
		return -EMSGSIZE;
	}

	applid = CAPIMSG_APPID(skb->data);
	this_application = APPL(applid);

	/*
	 * If this is a valid capi message (the subcommand wouldn't be zero)
	 * then we check on the application in question
	 */
	if (CAPIMSG_SUBCOMMAND(skb->data)) {
		switch(this_application->status) {
			case APPL_WAITING:
				skb_queue_head(&card->outgoing_queue);
				return -EAGAIN;
			case APPL_CONFIRMED:
				break;
			case APPL_REVOKED:
				kfree_skb(skb);
				return -EIO;
			default:
				break;
		}
	}

	retval = copy_to_user(buffer, skb->data, skb->len);
	if(retval) {
		skb_queue_head(&card->outgoing_queue);
		return retval;
	}
	copied = skb->len;
	kfree_skb(skb);
	spin_lock(cinfo->ctrl_lock);
        card->sk_pending -= 1;
	cinfo->out_sk++;
	spin_unlock(cinfo->ctrl_lock);
	return copied;
}

/*
 * Messages from the daemon
 */

static int capiproxy_write(struct file *file,
			   char *buffer,
			   size_t length,
			   loff_t offset)
{
        capi20proxy_card *card = (capi20proxy_card *)file->private_data;
        struct capi_ctr *ctrl  = card->ctrl;
	struct capiproxyctrl_info *cinfo = card->pcapiproxyctrl_info;
	struct sk_buff *skb;
	capiproxy_appl *this_application;
	int len;
	__u16 applid;
	unsigned char *writepos;

	if(!ctrl)
		return -ENOTREG;

	copy_from_user(&len, buffer, sizeof(int));
	buffer += sizeof(int);

	skb = alloc_skb(len + CAPI_MSG_BASELEN, GFP_ATOMIC);
	writepos = skb_put(skb, len);
	copy_from_user(writepos, buffer, len); 

	applid = CAPIMSG_APPID(skb->data);
	this_application = APPL(applid);
	
	switch(CAPIMSG_COMMAND(skb->data)) {
		case CAPI_CONNECT_B3_CONF:
			ctrl->new_ncci(ctrl,
					applid,
					CAPIMSG_NCCI(skb->data),
					this_application->.rp.datablkcnt);
			break;
		case CAPI_CONNECT_B3_IND:
			ctrl->new_ncci(ctrl,
					applid,
					CAPIMSG_NCCI(skb->data),
					this_application->.rp.datablkcnt);
			break;
		default:
			break;
	}

	spin_lock(cinfo->ctrl_lock);
	cinfo->in_sk++;
	spin_unlock(cinfo->ctrl_lock);
	ctrl->handle_capimsg(ctrl, applid, skb);
}

/*
 * This function is there only if the question 
 * just before capiproxy_open is justified
 */

static int capiproxy_find_free_id()
{
	int i;

	for (i = 0; i < CAPI_MAXCONTR; i++) {
		if(!VALID_CARD(i))
			return i;
	}
	return -1;
}

/*
 * The daemon, once again shall handle mapping the
 * controller ids (between the CLIENT and SERVER capi
 * implementations.
 */

static int capiproxy_open(struct inode *inode, struct file *file)
{
	int id;
	capi20proxy_card *card;
	capiproxyctrl_info *cinfo;
	struct capi_ctr *ctrl;

	id = capiproxy_find_free_id();
	if(id == -1) {
		printk(KERN_ERR "%s: no free controllers left", DRIVERNAME);
		return -1;
	}

	card = CARD(id);
	cinfo = card->pcapiproxyctrl_info;

	card->status = CARD_RUNNING;
	skb_queue_head_init(&card->outgoing_queue);
	init_waitqueue_head(&card->wait_queue_out);
	sprintf(cinfo->cardname, "%s", CARDNAME);
	cinfo->ctrl_lock = SPIN_LOCK_UNLOCKED;
	cinfo->versionlen = 3;
	sprintf(cinfo->versionbuf, "%s", proxy_version);
	cinfo->version[CAPIPROXY_VER_DRIVER] = &(cinfo->versionbuf[0]);	
	cinfo->card = card;

	spin_lock(&kernelcapi_lock);
	card->ctrl = di->attach_ctrl(capiproxy_driver, DRIVERNAME, (void *)cinfo);
	ctrl = card->ctrl;
	card->ctrl_id = ctrl->cnr; 	
	spin_unlock(&kernelcapi_lock);

	file->private_data = (capi20proxy_card *)card;
	MOD_INC_USE_COUNT;
	return 0;
}   

static int capiproxy_release(struct inode *inode, struct file *file)
{
	capi20proxy_card *card;
	struct capiproxyctrl_info *cinfo;
	struct capi_ctr *ctrl;

	card = (capi20proxy_card *)file->private_data;
	cinfo = card->pcapiproxyctrl_info;
	ctrl = card->ctrl;

	capiproxy_remove_ctr(ctrl);

	file->private_data 	= NULL;
	card->status		= CARD_FREE;
	card->ctrl		= NULL;

	printk(KERN_NOTICE "capi20proxy: Controller %d released", card->ctrl_id);
	MOD_DEC_USE_COUNT;
	return 0;
}

static struct file_operations capiproxy_fops = {
	owner:		THIS_MODULE,
	llseek:		NULL,
        read:		capiproxy_read,
	write:		capiproxy_write,
	poll:		NULL,
	ioctl:		capiproxy_ioctl,
	open:		capiproxy_open,
	flush:		NULL,
	release:	capiproxy_release,
	lock:		NULL
};

static struct tq_task tq_send_notify = {
	NULL,
	0,
	handle_send_msg,
	NULL,
};

static void capiproxy_init_cards()
{
	int i;
	capi20proxy_card *card;

	for (i = 0; i < CAPI_MAXCONTR; i++) {
		card = CARD(i);
		memset((void *)card, 0, sizeof(capi20proxy_card));
		card->ctrl_id = i;
		card->status = CARD_FREE;
		card->ctrl = NULL;
	}
}

static void capiproxy_init_appls()
{
	int i;
	capiproxy_appl *appl;
	
	for (i = 0; i < CAPI_MAXAPPL; i++) {	
 		appl = APPL(i);
		memset((void *)appl, 0, sizeof(capiproxy_appl));
		appl->appid = i;
		appl->status = APPL_WAITING;
	}	
}

static int __init capiproxy_init()
{
	capiproxy_init_cards();
	capiproxy_init_appls();
	init_waitqueue_head(&rmmod_queue);

	register_chrdev(CAPIPROXY_MAJOR, DRIVERNAME, &capiproxy_fops);

	spin_lock(&kernelcapi_lock);
	sprintf(capiproxy_driver.name, "capi20proxy");
	sprintf(capiproxy_driver.revision, "%s", main_revision);
	di = attach_capi_driver(&capiproxy_driver);
	spin_unlock(&kernelcapi_lock)

	if (!di)
		printk(KERN_ERR "capi20proxy: Load of driver failed\n");

	printk(KERN_NOTICE "%s loaded\n", DRIVERNAME);
	return (0);
}

static void __exit capiproxy_exit()
{
	int i;
	capi20proxy *card;
	struct capi_ctr *ctrl;

	for(i = 0; i < CAPI_MAXCONTR; i++) {
		if (!VALID_CARD(i))
			continue;

		card = CARD(i);
		ctrl = card->ctrl;
		if (!ctrl)
			continue;

		capiproxy_remove_ctr(ctrl);
	}

	system_shutting_down = 1;
	while(system_shutting_down)
		interruptible_sleep_on(&rmmod_queue);

	unregister_chrdev(CAPIPROXY_MAJOR, DRIVERNAME);

	detach_capi_driver(di);
	printk(KERN_NOTICE "%s unloaded\n", DRIVERNAME);
}

module_init(capiproxy_init);
module_exit(capiproxy_exit);
