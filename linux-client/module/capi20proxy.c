/*
 *   capi20proxy linux client module (Provides a virtual CAPI controller)
 *   Copyright (c) 2002. Begumisa Gerald & Adam Szalkowski. All rights reserved
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

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/skbuff.h>
#include <linux/config.h>
#include <linux/smp_lock.h>
#include <linux/vmalloc.h>
#include <linux/capi.h>
#include <linux/kernelcapi.h>

#include <linux/sched.h>
#include <linux/tqueue.h>
#include <linux/interrupt.h>

#include "capilli.h"
#include "capiutil.h"
#include "capicmd.h"
#include "capi20proxy.h"

#define APPL(card,a)			(card->applications[(a)-1])
#define MAX_SK_PENDING		32

EXPORT_NO_SYMBOLS;

MODULE_DESCRIPTION("CAPI Proxy Client driver");
MODULE_AUTHOR("Begumisa Gerald (beg_g@eahd.or.ug) & Adam Szalkowski (adam@szalkowski.de)");
MODULE_SUPPORTED_DEVICE("kernelcapi");
MODULE_LICENSE("GPL");

typedef struct __capi20proxy_card
{
	__u8 status; 

	struct capi_ctr *ctrl;
	struct sk_buff_head outgoing_queue;
	wait_queue_head_t wait_queue_out;

	char cardname[32];
	spinlock_t ctrl_lock;
	int versionlen; 	/* Length of the version in versionbuf*/
	char version[CAPIPROXY_VERSIONLEN];
	char infobuf[128];      /* for function procinfo */
	int in_sk, out_sk;      /* buffer count */
	int sk_pending;         /* number of out-going buffers currently in queue */
	
	__u8 applications[CAPI_MAXAPPL];	/* application stats are per-controller! */
} capi20proxy_card;

static capi20proxy_card cards[CAPIPROXY_MAXCONTR];
static spinlock_t kernelcapi_lock;
static struct capi_driver_interface *di;

static char *main_revision	= "$Revision$";
static char *capiproxy_version	= "0.6.1";
static char *DRIVERNAME		= "capi20proxy";
//static char *DRIVERLNAME	= "Capi 2.0 Proxy Client (http://capi20proxy.sourceforge.net)";
static char *CARDNAME		= "CAPI Virtual Card";
//static char *MANUFACTURER_NAME	= "http://capi20proxy.sourceforge.net";

/* some prototypes */
void capiproxy_register_appl(struct capi_ctr *ctrl,__u16 appl, struct capi_register_params *rp);
void capiproxy_release_appl(struct capi_ctr *ctrl, __u16 appl);
void capiproxy_remove_ctr(struct capi_ctr *ctrl);
void capiproxy_send_message(struct capi_ctr *ctrl,struct sk_buff *skb);
void capiproxy_reset_ctr(struct capi_ctr *ctrl);
int capiproxy_load_firmware(struct capi_ctr *ctrl, capiloaddata *data);
char *capiproxy_procinfo(struct capi_ctr *ctrl);
int capiproxy_read_proc(char *page,char **start,off_t off,int count,int *end,struct capi_ctr *ctrl);

int capiproxy_ioctl(struct inode *inode, struct file *file, unsigned int ioctl_num,unsigned long ioctl_param);
ssize_t capiproxy_read(struct file *file, char *buffer,size_t length, loff_t *offset);
ssize_t capiproxy_write(struct file *file, const char *buffer, size_t length, loff_t *offset);
int capiproxy_open(struct inode *inode, struct file *file);
int capiproxy_release(struct inode *inode, struct file *file);

static void handle_send_msg(void *dummy);

static void capiproxy_init_appls(capi20proxy_card *card);
static void capiproxy_release_internal(struct capi_ctr *ctrl, __u16 appl);
static int capiproxy_find_free_id(void);

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

static struct tq_struct tq_send_notify = {
	{NULL},
	0,
	handle_send_msg,
	NULL
};

static struct capi_driver capiproxy_driver = {
	"capi20proxy",
	"1.4",
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



inline int APPL_IS_FREE(capi20proxy_card *card, __u16 a)
{ 
	return APPL(card,a) == APPL_FREE;
}

inline void APPL_MARK_FREE(capi20proxy_card *card, __u16 a)
{
	APPL(card,a)= APPL_FREE;
}

inline void APPL_MARK_WAITING(capi20proxy_card *card, __u16 a)
{
	APPL(card,a)= APPL_WAITING;
}

inline void APPL_MARK_CONFIRMED(capi20proxy_card *card, __u16 a)
{
	APPL(card,a)= APPL_CONFIRMED;
}

inline void APPL_MARK_REVOKED(capi20proxy_card *card, __u16 a)
{
	APPL(card,a)= APPL_REVOKED;
}

inline int VALID_APPLID(capi20proxy_card *card, __u16 a)
{
	return (a && (a <= CAPI_MAXAPPL) && APPL(card,a));
}



/* -------------------------------------------------------------------------- */


static void capiproxy_flush_queue(capi20proxy_card *card)
{
	struct sk_buff *skb;

	printk(KERN_NOTICE "%s: flushing queue\n", DRIVERNAME);
	
	/*while (card->outgoing_queue.next!=NULL){*/
	while(card->outgoing_queue.qlen) {
		skb = skb_dequeue(&(card->outgoing_queue));
		kfree_skb(skb);
	}
	
	spin_lock(&(card->ctrl_lock));
	card->sk_pending = 0;
	spin_unlock(&(card->ctrl_lock));
}

void capiproxy_remove_ctr(struct capi_ctr *ctrl)
{
	struct sk_buff *skb;
	capi20proxy_card *card = (capi20proxy_card*)ctrl->driverdata;
	__u16 len;
	__u16 appl;
	__u8 command, subcommand;	

	appl = 0;

	/*
	 * Tell the daemon listening on this controller
	 * that we're shutting down the controller
	 *
	 * This looks like a "normal" CAPI message,
	 * but the command TYPE_PROXY tells the deamon
	 * that this message is only a control message
	 * that must not be forwarded to the server
	 */
	subcommand = TYPE_SHUTDOWN;
	command = TYPE_PROXY;
	len = sizeof(__u16)*2 + sizeof(__u8)*2;
	if(!(skb = alloc_skb(len + 1, GFP_ATOMIC))) {
		printk(KERN_ERR "%s card %d: Could not allocate memory\n", DRIVERNAME, ctrl->cnr);
		return;
	}
	skb->len=len;
	
	CAPIMSG_SETLEN(skb->data, len);
	CAPIMSG_SETAPPID(skb->data, appl);
	CAPIMSG_SETCOMMAND(skb->data, command);
	CAPIMSG_SETSUBCOMMAND(skb->data, subcommand);
	
	(*ctrl->suspend_output)(ctrl);
	capiproxy_flush_queue(card);
	card->status=CARD_REMOVING;
	
	capiproxy_send_message(ctrl, skb);

	/* do not detach the controller yet, wait for the daemon to detach first */
}

void capiproxy_reset_ctr(struct capi_ctr *ctrl)
{
	capi20proxy_card *card = (capi20proxy_card*)ctrl->driverdata;

	capiproxy_flush_queue(card);
	(*ctrl->reseted)(ctrl);
}

static void capiproxy_register_internal(struct capi_ctr *ctrl, __u16 appl,
                                        struct capi_register_params *rp)
{
	struct sk_buff *skb;
	__u16 len;
	__u8 subcommand = TYPE_REGISTER_APP;
	/* Command to daemon to throw together a register
	 * kind of packet to send to the server
	 */
	
	__u8 command = TYPE_PROXY;
	/* Tells the daemon this is not an ordinary message */

	len = sizeof(__u16)*2 + sizeof(__u8)*2 + sizeof(__u32)*3;
	if(!(skb = alloc_skb(len + 1, GFP_ATOMIC))) {
		printk(KERN_ERR "%s card %d: Could not allocate memory\n", DRIVERNAME, ctrl->cnr);
		return;
	}
	skb->len=len;
	
	CAPIMSG_SETLEN(skb->data, len);
	CAPIMSG_SETAPPID(skb->data, appl);
	CAPIMSG_SETCOMMAND(skb->data, command);
	CAPIMSG_SETSUBCOMMAND(skb->data, subcommand);
	CAPIMSG_SETL3C(skb->data, rp->level3cnt);
	CAPIMSG_SETDBC(skb->data, rp->datablkcnt);
	CAPIMSG_SETDBL(skb->data, rp->datablklen);
	capiproxy_send_message(ctrl, skb);
}

void capiproxy_register_appl(struct capi_ctr *ctrl, 
				__u16 appl, 
				struct capi_register_params *rp)
{
	capi20proxy_card *card = (capi20proxy_card*)ctrl->driverdata;
	capi_register_params arp;

	if(card->status==CARD_FREE) {
		printk(KERN_ERR "%s card %d: Tried to register application on free controller\n", DRIVERNAME, ctrl->cnr);
		(*ctrl->appl_released)(ctrl,appl);
		return;
	}

	if (!APPL_IS_FREE(card,appl)) {
		printk(KERN_ERR "%s card %d: Application %d already registered\n", DRIVERNAME, ctrl->cnr, appl);
		return;
	}
	
	arp.datablkcnt = (rp->datablkcnt > CAPI_MAXDATAWINDOW) ? CAPI_MAXDATAWINDOW : rp->datablkcnt;	
	arp.datablklen = (rp->datablklen > 1024) ? 1024 : rp->datablklen;
	arp.level3cnt  = rp->level3cnt;

	capiproxy_register_internal(ctrl, appl, &arp);
	APPL_MARK_WAITING(card,appl);
	printk(KERN_NOTICE "%s card %d: Application %d registering\n", DRIVERNAME, ctrl->cnr, appl);
}

/*
 * Gerald:
 * I think if there is an error registering the application
 * then the daemon should send a message to this module
 * reporting the error because as far as kcapi is concerned,
 * our module told it that the application had been registered
 *
 * One problem is:- kcapi may send messages from that application
 * before it is confirmed.  The go-around I thought of was to
 * include the variable "status" in the applications structure.
 *
 * Adam:
 * My idea is to not call ctrl->appl_registered before the application is
 * registered.
 * The deamon will call an ioctl that makes this module call appl_registered!
 */

void capiproxy_release_appl(struct capi_ctr *ctrl, 
			    __u16 appl)
{
	capi20proxy_card *card = (capi20proxy_card*)ctrl->driverdata;
	
	if((!VALID_APPLID(card,appl)) || APPL_IS_FREE(card,appl)) {
		(*ctrl->appl_released)(ctrl, appl);
		printk(KERN_ERR "%s card %d: Releasing free application or invalid applid -- %d\n", DRIVERNAME, ctrl->cnr, appl);
		return;
	}
	capiproxy_release_internal(ctrl, appl);
	APPL_MARK_FREE(card,appl);
	//DEC_MOD_USE_COUNT;	
	(*ctrl->appl_released)(ctrl, appl);
	printk(KERN_NOTICE "%s card %d: Application released %d\n", DRIVERNAME, ctrl->cnr, appl);
}

static void capiproxy_release_internal(struct capi_ctr *ctrl, 
				       __u16 appl)
{
	struct sk_buff *skb;
	__u16 len;
	__u8 command;
	__u8 subcommand;

	/*
	 * A command of zero should tell the daemon that this
	 * is not an ordinary capi message.
	 */

	command 	= TYPE_PROXY;
	subcommand 	= TYPE_RELEASE_APP;

	len = sizeof(__u16)*2 + sizeof(__u8)*2;
	if(!(skb = alloc_skb(len+1, GFP_ATOMIC))) {
		printk(KERN_ERR "%s card %d: Could not allocate memory\n", DRIVERNAME, ctrl->cnr);
		return;
	}
	skb->len=len;
	
        CAPIMSG_SETLEN(skb->data, len);
        CAPIMSG_SETAPPID(skb->data, appl);
        CAPIMSG_SETCOMMAND(skb->data, command);
        CAPIMSG_SETSUBCOMMAND(skb->data, subcommand);
	capiproxy_send_message(ctrl, skb);
}

/* messages coming from kernelcapi */
void capiproxy_send_message(struct capi_ctr *ctrl,
				struct sk_buff *skb)
{
	capi20proxy_card *card = (capi20proxy_card*)ctrl->driverdata;
	__u16 applid;

	printk(KERN_NOTICE "%s card %d: message received from kcapi\n", DRIVERNAME, ctrl->cnr);

	if(card->status==CARD_FREE) {
		printk(KERN_ERR "%s card %d: message for free card dropped\n", DRIVERNAME, ctrl->cnr);
		kfree_skb(skb);
		return;
	}
	
	applid = CAPIMSG_APPID(skb->data);

	switch(CAPIMSG_COMMAND(skb->data)) {
		case CAPI_DISCONNECT_B3_RESP:
			(*ctrl->free_ncci)(ctrl, applid, CAPIMSG_NCCI(skb->data));
			break;
		default:
			break;
	}	
	
	if(card->sk_pending>MAX_SK_PENDING) {
		printk(KERN_ERR "%s card %d: message buffer overflow. message dropped.\n", DRIVERNAME, ctrl->cnr);
		kfree_skb(skb);
		return;
	}
	
	spin_lock(&(card->ctrl_lock));	
	skb_queue_tail(&(card->outgoing_queue), skb);
	card->sk_pending += 1;
	spin_unlock(&(card->ctrl_lock));

	
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

char *capiproxy_procinfo(struct capi_ctr *ctrl)
{
	capi20proxy_card *card = (capi20proxy_card*)ctrl->driverdata;

	if (!card)
		return "";

	sprintf(card->infobuf, "%s %s 0x%x",
		card->cardname[0] ? card->cardname : "-",
		card->version[CAPIPROXY_VER_DRIVER] ? card->version : "-",
		card ? (__u32)(void*)card : 0x0
		);
		
	return card->infobuf;
}

int capiproxy_read_proc(char *page,
			char **start,
			off_t off,
			int count,
			int *end,
			struct capi_ctr *ctrl)
{
	capi20proxy_card *card = (capi20proxy_card*)ctrl->driverdata;
	int len = 0;
	
	len += sprintf(page+len, "%-16s %s\n", "name", card->cardname);
	len += sprintf(page+len, "%-16s 0x0\n", "io");
	len += sprintf(page+len, "%-16s -\n", "irq");
	len += sprintf(page+len, "%-16s %s\n", "type", "Virtual Card :-)");

	if (card->version[CAPIPROXY_VER_DRIVER] != 0)
		len += sprintf(page+len, "%-16s %s\n", "ver_driver", card->version);

	len += sprintf(page+len, "%-16s %s\n", "cardname", card->cardname);

	if (off+count >= len)
		*end = 1; /* eof? */
	if (len < off)
		return 0;

	*start = page + off;
	return ((count < len-off) ? count : len-off);
}

static void push_user_buffer(void *dest, void **src, int len)
{
	copy_from_user(dest, *src, len);
	((char*)*src) += len; 
}

int capiproxy_ioctl(struct inode *inode, 
				struct file *file, 
				unsigned int ioctl_num,
				unsigned long ioctl_param)
{
	char *temp = (char *)ioctl_param;
	capi20proxy_card *card = (capi20proxy_card *)file->private_data;
	struct capi_ctr *ctrl  = card->ctrl;
	__u16 appl;
	int len;
	
	printk(KERN_NOTICE "%s card %d: ioctl()\n", DRIVERNAME, ctrl->cnr);
	
	switch(ioctl_num) {
		case IOCTL_SET_DATA:
		{
			if (!ctrl)
				return -ENOTREG;

			copy_from_user(&len, temp, sizeof(int));
			temp += sizeof(int);
			
			if (len < CAPI_MANUFACTURER_LEN + (sizeof(struct capi_version)) + CAPI_SERIAL_LEN + (sizeof(struct capi_profile)))
				return -EMSGSIZE;

			push_user_buffer((void *)(ctrl->manu),
					(void **)(&temp),
					CAPI_MANUFACTURER_LEN);
			push_user_buffer((void *)(&(ctrl->version)),
					(void **)(&temp),
					sizeof(struct capi_version));
			push_user_buffer((void *)(ctrl->serial),
					(void **)(&temp),
					CAPI_SERIAL_LEN);
			push_user_buffer((void *)(&(ctrl->profile)),
					(void **)(&temp),
					sizeof(struct capi_profile));
			
			(*ctrl->ready)(ctrl);
			card->status=CARD_RUNNING;
			printk(KERN_NOTICE "%s card %d: controller ready\n", DRIVERNAME, ctrl->cnr);
			return 0;
		}
		case IOCTL_ERROR_REGFAIL:
		{
			appl=(__u16)ioctl_param;			
			APPL_MARK_REVOKED(card,appl);
			(*ctrl->appl_released)(ctrl, appl);
			printk(KERN_NOTICE "%s card %d: deamon failed to register application %d\n", DRIVERNAME, ctrl->cnr, appl);
			return 0;
		}
		case IOCTL_APPL_REGISTERED:
		{
			appl=(__u16)ioctl_param;			
			(*ctrl->appl_registered)(ctrl, appl);
			APPL_MARK_CONFIRMED(card,appl);
			//INC_MOD_USE_COUNT;
			printk(KERN_NOTICE "%s card %d: daemon confirmed application %d\n", DRIVERNAME, ctrl->cnr, appl);
			return 0;
		}
		default:
			return -EIO;
	}
}

/* -------------------------------------------------------------------------- */

static void handle_send_msg(void *dummy)
{
	int i;
	capi20proxy_card *card;

	for (i = 0; i < CAPIPROXY_MAXCONTR; i++) {
		card = &cards[i];
		if(card->status == CARD_FREE) 
			continue;

		if(card->sk_pending)
			wake_up_interruptible(&card->wait_queue_out);
	}

}

/*
 * The daemon blocks at this function until data is
 * ready for transfer.  The daemon shall handle mapping
 * the application identifiers known to this (CLIENT's)
 * CAPI with the application identifiers known to the
 * SERVER's CAPI
 */

/* ----------------------------------------------------------------------------------- */

/* static ? */
ssize_t capiproxy_read(struct file *file,
				char *buffer,
				size_t length,
				loff_t *offset)
{
	capi20proxy_card *card = (capi20proxy_card *)file->private_data;
	struct sk_buff *skb;
	int retval;
	__u16 appl;
	size_t copied;

	if (!card->sk_pending) {
		if(file->f_flags & O_NONBLOCK)
			return -EAGAIN;
	
		for (;;) {
			interruptible_sleep_on(&card->wait_queue_out);
			if (card->sk_pending)
				break;
			if (signal_pending(current))
				break;
		}

		if (!card->sk_pending)
			return -EINTR;
	}
	skb = skb_dequeue(&card->outgoing_queue); 

	if (skb->len > length) {
		skb_queue_head(&card->outgoing_queue,skb);
		return -EMSGSIZE;
	}
	
	if(skb->len < 6) {
		/* message ist too short take the next one */
		printk(KERN_WARNING "%s: too short message dropped\n", DRIVERNAME);
		
		spin_lock(&(card->ctrl_lock));
        	card->sk_pending -= 1;
		spin_unlock(&(card->ctrl_lock));
		
		return capiproxy_read(file,buffer,length,offset);
	}
		

/*	Mybe we should not check whether the message is valid. Let the daemon do that!
 
	appl = CAPIMSG_APPID(skb->data);

	*
	 * If this is a valid capi message (the command wouldn't be zero)
	 * then we check on the application in question
	 *
	if (CAPIMSG_COMMAND(skb->data)!=TYPE_PROXY) {
		switch(APPL(card,appl)) {
			case APPL_WAITING:
				skb_queue_head(&card->outgoing_queue,skb);
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
*/

	retval = copy_to_user(buffer, skb->data, skb->len);
	if(retval) {
		skb_queue_head(&card->outgoing_queue,skb);
		printk(KERN_ERR "%s: error %x in copy_to_user()\n", DRIVERNAME, retval);
		return retval;
	}
	copied = skb->len;
	kfree_skb(skb);
	spin_lock(&(card->ctrl_lock));
        card->sk_pending -= 1;
	card->out_sk++;
	spin_unlock(&(card->ctrl_lock));
	return copied;
}

/*
 * Messages from the daemon
 */

ssize_t capiproxy_write(struct file *file,
			   const char *buffer,
			   size_t length,
			   loff_t *offset)
{
	capi20proxy_card *card = (capi20proxy_card *)file->private_data;
        struct capi_ctr *ctrl  = card->ctrl;
	struct sk_buff *skb;
	__u16 appl;
	__u32 ncci;
	unsigned char *writepos;

	if(card->status!=CARD_RUNNING) {
		printk(KERN_ERR "%s card %d: Card not ready to receive messages\n", DRIVERNAME, ctrl->cnr);
		return -1;
	}

	if(!(skb = alloc_skb(length+1, GFP_ATOMIC))) {
		printk(KERN_ERR "%s card %d: Could not allocate memory\n", DRIVERNAME, ctrl->cnr);
		return -1;
	}
	skb->len=length;
	
	writepos = skb_put(skb, length);
	copy_from_user(writepos, buffer, length); 

	appl = CAPIMSG_APPID(skb->data);
	ncci = CAPIMSG_NCCI(skb->data);

	/* map the controller id */
	
	ncci &=  0xFFFFFF80;
	ncci += ctrl->cnr;
	
	switch(CAPIMSG_COMMAND(skb->data)) {
		case CAPI_CONNECT_B3_CONF:
		case CAPI_CONNECT_B3_IND:
			(*ctrl->new_ncci)(ctrl,
					appl,
					ncci,
					CAPI_MAXDATAWINDOW);
			break;
		default:
			break;
	}

	spin_lock(&(card->ctrl_lock));
	card->in_sk++;
	spin_unlock(&(card->ctrl_lock));
	(*ctrl->handle_capimsg)(ctrl, appl, skb);
	return length;
}

static int capiproxy_find_free_id()
{
	int i;

	for (i = 0; i < CAPIPROXY_MAXCONTR; i++) {
		if(cards[i].status==CARD_FREE)
			return i;
	}
	return -1;
}

/*
 * The daemon will have to map controller ids in the messages
 * he gets from the module. The module cannot know which
 * controller on the server the daemon uses.
 * But messages coming from the daemon.to the module need not
 * to have mapped controller ids. They will be adjusted by the module!
 */

int capiproxy_open(struct inode *inode, struct file *file)
{
	int id;
	capi20proxy_card *card;
	struct capi_ctr *ctrl;

	ctrl = card->ctrl;
	id = capiproxy_find_free_id();
	if(id == -1) {
		printk(KERN_ERR "%s: no free controllers left\n", DRIVERNAME);
		return -1;
	}

	card=&cards[id];

	capiproxy_init_appls(card);
	capiproxy_flush_queue(card);
	
	card->status = CARD_NOT_READY;
	skb_queue_head_init(&card->outgoing_queue);
	init_waitqueue_head(&card->wait_queue_out);
	
	sprintf(card->cardname, "%s", CARDNAME);
	card->ctrl_lock = SPIN_LOCK_UNLOCKED;
	card->versionlen = CAPIPROXY_VERSIONLEN;
	sprintf(card->version, "%s", capiproxy_version);

	spin_lock(&kernelcapi_lock);
	card->ctrl = di->attach_ctr(&capiproxy_driver, DRIVERNAME, (void*)card);
	spin_unlock(&kernelcapi_lock);

	file->private_data = (void*)card;
	//MOD_INC_USE_COUNT;
	return 0;
}   

int capiproxy_release(struct inode *inode, struct file *file)
{
	capi20proxy_card *card;
	struct capi_ctr *ctrl;

	card = (capi20proxy_card *)file->private_data;
	ctrl = card->ctrl;

	printk(KERN_NOTICE "%s: removing Controller %d\n", DRIVERNAME, ctrl->cnr);
	
	(*ctrl->suspend_output)(ctrl);
	capiproxy_flush_queue(card);
	(*di->detach_ctr)(ctrl);
	
	file->private_data 	= NULL;
	card->status		= CARD_FREE;
	card->ctrl		= NULL;

	/*
	 * Should we unregister all apps?
	 */

	printk(KERN_NOTICE "%s: Controller %d released\n", DRIVERNAME, ctrl->cnr);
	//MOD_DEC_USE_COUNT;
	return 0;
}

static void capiproxy_init_cards(void)
{
	int i;
	capi20proxy_card *card;

	for (i = 0; i < CAPIPROXY_MAXCONTR; i++) {
		card = &cards[i];
		memset((void *)card, 0, sizeof(capi20proxy_card));
		card->status = CARD_FREE;
		card->ctrl = NULL;
	}
}

static void capiproxy_init_appls(capi20proxy_card *card)
{
	memset((void *)card->applications, APPL_FREE, CAPI_MAXAPPL);
}

/* ------------------------------------------------------------- */

/*EXPORT_SYMBOL(capiproxy_register_appl);
EXPORT_SYMBOL(capiproxy_release_appl);
EXPORT_SYMBOL(capiproxy_remove_ctr);
EXPORT_SYMBOL(capiproxy_send_message);
EXPORT_SYMBOL(capiproxy_reset_ctr);
EXPORT_SYMBOL(capiproxy_load_firmware);
EXPORT_SYMBOL(capiproxy_procinfo);
EXPORT_SYMBOL(capiproxy_read_proc);

EXPORT_SYMBOL(capiproxy_ioctl);
EXPORT_SYMBOL(capiproxy_read);
EXPORT_SYMBOL(capiproxy_write);
EXPORT_SYMBOL(capiproxy_open);
EXPORT_SYMBOL(capiproxy_release);*/

static int __init capiproxy_init(void)
{
	capiproxy_init_cards();

	register_chrdev(CAPIPROXY_MAJOR, DRIVERNAME, &capiproxy_fops);

	spin_lock(&kernelcapi_lock);
	sprintf(capiproxy_driver.name, DRIVERNAME);
	sprintf(capiproxy_driver.revision, "%s", main_revision);
	di = attach_capi_driver(&capiproxy_driver);
	spin_unlock(&kernelcapi_lock);

	if (!di) {
		printk(KERN_ERR "%s: Load of driver failed\n", DRIVERNAME);
		return -1;
	} else {
		printk(KERN_NOTICE "%s loaded\n", DRIVERNAME);
		return 0;
	}
}

static void __exit capiproxy_exit(void)
{
	int i;
	capi20proxy_card *card;
	struct capi_ctr *ctrl;

	for(i = 0; i < CAPIPROXY_MAXCONTR; i++) {
		if (cards[i].status==CARD_FREE)
			continue;

		card = &cards[i];
		ctrl = card->ctrl;
		if (!ctrl)
			continue;

		capiproxy_remove_ctr(ctrl);
	}

	handle_send_msg(NULL);
	
	unregister_chrdev(CAPIPROXY_MAJOR, DRIVERNAME);

	detach_capi_driver(&capiproxy_driver);
	printk(KERN_NOTICE "%s unloaded\n", DRIVERNAME);
}

module_init(capiproxy_init);
module_exit(capiproxy_exit);
