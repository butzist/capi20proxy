/*
 *   capi20proxy linux client module (Provides a remote CAPI port over TCP/IP)
 *   Copyright (C) 2002  Adam Szalkowski <adam@szalkowski.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * $Log$
 * Revision 1.3  2002/03/03 20:58:13  butzist
 * formatted
 *
 * Revision 1.2  2002/03/03 19:55:19  butzist
 * changed some stuff.
 * changed memcpy to copy_to/from_user
 *
 */

/*
    Main source file for the module
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

EXPORT_NO_SYMBOLS;



#define CAPIPROXY_MAJOR	69

#define MAX_CONTROLLERS	16

#define MSG_QUEUE_LENGTH	16

#define TYPE_MESSAGE		0x0001
#define TYPE_REGISTER_APP	0x0002
#define TYPE_RELEASE_APP	0x0003
#define TYPE_SHUTDOWN		0x0004


#define IOCTL_SET_DATA		0x0001	//set some driver data (use the driver_data struct) 
#define IOCTL_USED			0x0002	//is the current minor already used





static char *main_revision = "$Revision$";
static char *DRIVERNAME = "capi20proxy";
static char *DRIVERLNAME = "Capi 2.0 Proxy Client (http://capi20proxy.sourceforge.net)";
static char *MANUFACTURER_NAME = "http://capi20proxy.sourceforge.net";

void capiproxy_reset_ctr(struct capi_ctr *_ctrl);
void capiproxy_remove_ctr(struct capi_ctr *_ctrl);
void capiproxy_register_appl(struct capi_ctr *_ctrl, __u16 _applId, capi_register_params *_params); 
void capiproxy_release_appl(struct capi_ctr *_ctrl, __u16 _applId); 
void capiproxy_send_message(struct capi_ctr *_ctrl, struct sk_buff *_skb);
static char *capiproxy_procinfo(struct capi_ctr *_ctrl);
int capiproxy_ctl_read_proc(char *_buffer, char **_start, off_t _off,int _count, int *_end, struct capi_ctr *_crtl);
int capiproxy_driver_read_proc(char *_buffer, char **_start, off_t _off,int _count, int *_end, struct capi_driver *_drv);

static int capiproxy_ioctl(struct inode *_i, struct file *_f, unsigned int _cmd, unsigned long _b);
static int capiproxy_open(struct inode *_i, struct file *_f);
static int capiproxy_release(struct inode *_i, struct file *_f);
static ssize_t capiproxy_read(struct file *_f,char *_buffer, size_t _size, loff_t *_off);
static ssize_t capiproxy_write(struct file *_f,const char *_buffer, size_t _size, loff_t *_off);

MODULE_DESCRIPTION(             "CAPI Proxy Client driver");
MODULE_AUTHOR(                  "[Butzisten] The Red Guy (adam(at)szalkowski.de) && ...");
MODULE_SUPPORTED_DEVICE(        "kernelcapi");


static spinlock_t kernelcapi_lock;

static struct capi_driver_interface *di;

static struct capi_driver capiproxy_driver = {
    "",
    "",
    capiproxy_reset_ctr,
    capiproxy_remove_ctr,
    capiproxy_register_appl,
    capiproxy_release_appl,
    capiproxy_send_message,
    capiproxy_procinfo,
    capiproxy_ctl_read_proc,
    capiproxy_driver_read_proc,
    0,
    0,
};

static struct file_operations capiproxy_fops = {
	owner:	THIS_MODULE,
	read:	capiproxy_read,
	write:	capiproxy_write,
	poll:	NULL,
	ioctl:	capiproxy_ioctl,
	open:	capiproxy_open,
	flush:	NULL,
	release:	capiproxy_release,
	lock:	NULL
};

struct capi20proxy_card
{
	unsigned int count;
	struct capi_ctr* ctrl;
	struct queue msg_queue;
};

static capi20proxy_card *card;



void capiproxy_reset_ctr(struct capi_ctr *_ctrl)
{
	// Perhaps I can do something like empty the message queue

	byte controller = (byte)_ctrl->driverdata;

	queue_free(&(card[controller].msg_queue));
}

void capiproxy_remove_ctr(struct capi_ctr *_ctrl);
{
	// Is this called when kernelcapi wants a controller to shut down?
	// If so, here's my implementation:

	byte controller = (byte)_ctrl->driverdata;

	skb = alloc_skb(4, GFP_ATOMIC);
	buffer = (byte *) skb_put(skb, 4);
       
	WRITE_DWORD((byte*)buffer,TYPE_SHUTDOWN);

	if(!queue_append(&(card[controller].msg_queue),skb))
	{
		printk("%s: Queue full, message dropped", DRIVERNAME);
		kfree_skb(skb);
	}
}

void capiproxy_register_appl(struct capi_ctr *_ctrl, __u16 _applId, capi_register_params *_params)
{
	struct sk_buff *skb;
	byte* buffer;


// I'm not sure whether CAPI_REGISTER is called for all controllers or only one for each driver...

	byte i = (byte)_ctrl->driverdata;
	printk("%s (debug): CAPI_REGISTER called for driver #%i", DRIVERNAME, i);
//	for(int i=0; i<MAX_CONTROLLERS; i++)
//	{
		if(card[i].ctrl!=NULL)
		{
			/* !!! not sure about this! */
			skb = alloc_skb(6+sizeof(struct capi_register_params), GFP_ATOMIC);
			buffer = (byte *) skb_put(skb, 6+sizeof(struct capi_register_params));
			
			WRITE_DWORD((byte*)buffer,TYPE_REGISTER_APP);
			WRITE_WORD((byte*)buffer+4,_applId);
			memcpy(buffer+6, (byte*)_params, sizeof(struct capi_register_params));

			if(!queue_append(&(card[i].msg_queue),skb))
			{
				printk("%s: Queue full, message dropped", DRIVERNAME);
				kfree_skb(skb);
			}
//		}
//	}

	//CapiRegister(_applId); // What is this for?
	
	/* tell kcapi that we actually registerd the appl */
	ctrl->appl_registered(_ctrl, _applId);

	MOD_INC_USE_COUNT
}

void capiproxy_release_appl(struct capi_ctr *_ctrl, __u16 _applId)
{
	MOD_DEC_USE_COUNT

	struct sk_buff *skb;
	byte* buffer;

// I'm not sure whether CAPI_REGISTER is called for all controllers or only one for each driver...

	byte i = (byte)_ctrl->driverdata;
//	for(int i=0; i<MAX_CONTROLLERS; i++)
//	{
		if(card[i].ctrl!=NULL)
		{
			/* !!! not sure about this! */
			skb = alloc_skb(6, GFP_ATOMIC);
			buffer = (byte *) skb_put(skb, 6);
			
			WRITE_DWORD((byte*)buffer,TYPE_RELEASE_APP);
			WRITE_WORD((byte*)buffer+4,_applId);

			if(!queue_append(&(card[i].msg_queue),skb))
			{
				printk("%s: Queue full, message dropped", DRIVERNAME);
				kfree_skb(skb);
			}
//		}
//	}

	//CapiRelease(_applId); // What is this for?
	
	/* tell kcapi that we actually released the appl */
	ctrl->appl_released(_ctrl, _applId);

}

void capiproxy_send_message(struct capi_ctr *_ctrl, struct sk_buff *_skb)
{
	byte controller=(byte)(_ctrl->driverdata);  // we saved the mapped controller id in the driverdata field

	if(!queue_append(&(card[controller].msg_queue),skb))
	{
		printk("%s: Queue full, message dropped", DRIVERNAME);
		kfree_skb(skb);
	}
}


/***************** CAPI proc functions ***********************/

int capiproxy_driver_read_proc(char *_buffer, char **_start, off_t _off,int _count, int *_end, struct capi_driver *_drv);
{
	int len = sprintf(_buffer,"capi20proxy: Sorry, procinfo not implemented\n");

	if (_off + _count >= len)
		*_eof = 1;
	if (len < _off)
		return 0;
	*_start = _buffer + _off;
	return((_count < len-_off) ? _count : len-_off);
}

static char *capiproxy_procinfo(struct capi_ctr *_ctrl);
{
	return "capi20proxy";
}

int capiproxy_ctl_read_proc(char *_buffer, char **_start, off_t _off,int _count, int *_end, struct capi_ctr *_ctrl);
{
	int len = sprintf(_buffer,"capi20proxy: Sorry, procinfo not implemented\n");

	if (_off + _count >= len)
		*_eof = 1;
	if (len < _off)
		return 0;
	*_start = _buffer + _off;
	return((_count < len-_off) ? _count : len-_off);
}



/********** The functions for the character device interface ***********/

static ssize_t capiproxy_read(struct file *_f, char *_buffer, size_t _size, loff_t *_off)
{
	int minor=MINOR(_f->f_dentry->d_inode->i_rdev);
	int major=MAJOR(_f->f_dentry->d_inode->i_rdev);


	while((queue_empty(&(card[minor].msg_queue))) && (_f->f_flags & F_BLOCK)) {} //wait until message arrives if file opened in blocking mode

	struct sk_buff *skb=queue_top(&(card[minor].msg_queue));
        
	int len=0;
	if(skb!=NULL)
	{
		len=skb->length;
		if(len>size)
		{
			printk("%s: Buffer too small, data lost");  // You won't be notified directly if data is lost!!!!
			len=_size;
		}

		char* writepos = skb_put(skb);

		copy_to_user(buffer, writepos, len); //queue -> buffer
		{
			printk("%s: Failed copying data to userspace", DRIVERNAME);
			kfree_skb(skb);
			return ERESTARTSYS;
		}

		kfree_skb(skb);
	}

	return len;		//len will be 0 if queue is empty
}

static ssize_t capiproxy_write(struct file *_f, const char *_buffer, size_t _size, loff_t *_off)
{
	int minor=MINOR(_f->f_dentry->d_inode->i_rdev);
	int major=MAJOR(_f->f_dentry->d_inode->i_rdev);

	if(_size<4) return 0; //the first DWORD (unsigned long) should indicate the type of the message

	__u32 type;

	if(!get_user(type,(__u32*)_buffer))
	{
		printk("%s: Failed getting value from userspace", DRIVERNAME);
		return ERESTARTSYS;
	}


	// set the size and the pointer to the data to the "real" values
	size_t size = _size - 4;
	char* buffer = _buffer + 4;
	
	switch(type)
	{

	case TYPE_MESSAGE:
		spin_lock(&kernalcapi_lock);

		if(card[minor].ctrl!=NULL)
		{
			struct sk_buff skb;
			skb = alloc_skb(size), GFP_ATOMIC);
			byte *writepos = (byte *) skb_put(skb, size);
			
			if(copy_from_user(writepos, buffer, size)!=0)
			{
				printk("%s: Failed getting data from userspace", DRIVERNAME);
				return ERESTARTSYS;
			}


			if(CAPI_MESSAGE(skb->data)==CAPI_CONNECT_B3_IND)  //not too shure, whether this is the only possible message
			{
				ctrl->new_ncci(APPL_ID(skb->data,CAPI_NCCI(skb->data)));   //only symbolic!
			}
                        
			if(CAPI_MESSAGE(skb->data)==CAPI_DISCONNECT_B3_IND)  //not too shure, whether this is the only possible message
			{
				ctrl->release_ncci(APPL_ID(skb->data,CAPI_NCCI(skb->data)));   //only symbolic!
			}

			card[minor].ctrl->handle_capi_msg(buffer,ApplId,skb,...);

			//kfree_skb(skb);         //have I to do this?
		}
		else
		{
			printk("%s: Controller %i seems not to be registered\n Skipping message\n", DRIVERNAME, minor);
		}

		spin_unlock(&kernelcapi_lock);
		break;

	default:
		//ignore
	}

	return _size;
}

static int capiproxy_ioctl(struct inode *_i, struct file *_f, unsigned int _cmd, unsigned long _b)
{
    char* buffer = (char*)(void*) _b;
    int major = MAJOR(_i->i_rdev);
    int minor = MINOR(_i->i_rdev);
    
    switch(_cmd)
    {
	case IOCTL_SET_DATA:
		ctrl=card[minor].ctrl;

		if(ctrl!=NULL)
		{
			// Copy driver data to Kernelspace (dunno if it's really called copy_to_system())
			
			copy_to_system(ctrl->manu, buffer, CAPI_MANUFACTURER_LEN);
			buffer += CAPI_MANUFACTURER_LEN;

			copy_to_system(ctrl->version, buffer, CAPI_VERSION_LEN);
			buffer += CAPI_VERSION_LEN;

			copy_to_system(ctrl->serial, buffer, CAPI_SERIAL_LEN);
			buffer[CAPI_SERIAL_LEN-1] = 0;
			buffer += CAPI_SERIAL_LEN;

			copy_to_system(ctrl->profile, buffer, CAPI_PROFILE_LEN);
			
			ctrl->ready(ctrl);
		}
		break;

	case IOCTL_USED:
		
		if(card[minor].count>1) return 1;
		else return 0;
		break;

	default:
	    copy_to_user(buffer,"Hallo\n",6);   //Daten von Kernel- in den Userspace kopieren
    }
    
    printk("%s: ioctl called with major(%i) and minor(%i)\n", DRIVERNAME, major, minor);

    return (0);
}

static int capiproxy_open(struct inode *_i, struct file *_f)
{
    int major = MAJOR(_i->i_rdev);
    int minor = MINOR(_i->i_rdev);
	int error=0;
    
    printk("%s: open called with major(%i) and minor(%i)\n", DRIVERNAME, major, minor);
    
	// first set the name and revision in the struct
	spin_lock(&kernelcapi_lock);

	if(card[minor].count==0)
	{

		sprintf(capiproxy_driver.name, "capi20proxy%i",minor);
		sprintf(capiproxy_driver.revision, "0");

		card[minor].ctrl = di->attach_ctrl(capiproxy_driver, DRIVERNAME, (void*)minor);	// we save the mapped controller id in the driverdata field

		card[minor].msg_queue.length=MSG_QUEUE_LENGTH;
	    queue_init(&(card[minor].msg_queue));
	} else {
		error=1;
	}

	card[minor].count++;

	spin_lock(&kernelcapi_lock);
	
	MOD_INC_USE_COUNT;
    
    return (error);
}   

static int capiproxy_release(struct inode *_i, struct file *_f)
{

    MOD_DEC_USE_COUNT;

    int major = MAJOR(_i->i_rdev);
    int minor = MINOR(_i->i_rdev);
    
    printk("%s: release called with major(%i) and minor(%i)\n",DRIVERNAME, major, minor);

	spin_lock(&kernelcapi_lock);
	
	if(card[minor].count==1)
	{
		di->detach_ctrl(card[minor].ctrl);

		card[minor].count--;
		card[minor].ctrl=NULL;
    
		queue_free(&(card[minor].msg_queue));
	}

	spin_unlock(&kernelcapi_lock);

    return (0);
}



/******************** Module initialization **********************/

static int __init capiproxy_init()
{
    printk("%s: module loaded\n", DRIVERNAME);
    register_chrdev(CAPIPROXY_MAJOR,"capi20proxy",&capiproxy_fops);

	card = kmalloc(MAX_CONTROLLERS*sizeof(struct capiproxy_card));

	// first set the name and revision in the struct
	spin_lock(&kernelcapi_lock);

	sprintf(capiproxy_driver.name, "capi20proxy");
	sprintf(capiproxy_driver.revision, "%s", main_revision);

    di=attach_capi_driver(&capiproxy_driver);

	spin_unlock(&kernelcapi_lock)


    return (0);
}

static void __exit capiproxy_exit()
{
	for(int i=0; i<MAX_CONTROLLERS; i++)
	{
		if(card[i].ctrl!=NULL)
		{
			/* !!! not sure about this! */
			skb = alloc_skb(4, GFP_ATOMIC);
			buffer = (byte *) skb_put(skb, 4);
               
			WRITE_DWORD((byte*)buffer,TYPE_SHUTDOWN);

			if(!queue_append(&(card[i].msg_queue),skb))
			{
				printk("%s: Queue full, message dropped", DRIVERNAME);
				kfree_skb(skb);
			}
		}
	}
    
	mdelay(5000); //5 seconds (perhaps more)

	unregister_chrdev(CAPIPROXY_MAJOR,"capi20proxy");

	spin_lock(&kernelcapi_lock);
	for(int i=0; i<MAX_CONTROLLERS; i++)
	{
		if(card[i].ctrl!=NULL)
		{
			di->release_ctrl(ctrl,...)
			queue_free(&(card[i].msg_queue));    
		}
	}

	detach_capi_driver();
	spin_unlock(&kernelcapi_lock);

	kfree(card);
    
	printk("%s: module released\n", DRIVERNAME);
}


module_init(capiproxy_init);
module_exit(capiproxy_exit);
