// dunno what we really need from this
#include <linux/config.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/smp_lock.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>

#include <linux/isdn.h>
#include <linux/isdnif.h>

#include <linux/isdn_compat.h>

#include <net/capi/capi.h>
#include <net/capi/driver.h>
#include <net/capi/util.h>

EXPORT_NO_SYMBOLS;

static char *main_revision = "$Revision$";
static char *DRIVERNAME = "Capi 2.0 Proxy Client (http://capi20proxy.sourceforge.net/)";
static char *DRIVERLNAME = "capi20client";


void reset_ctr(struct capi_ctr *);
void remove_ctr(struct capi_ctr *);
void register_appl(struct capi_ctr *, __u16 , capi_register_params *); 
void release_appl(struct capi_ctr *, __u16); 
void send_message(struct capi_ctr *, struct sk_buff *);
static char *procinfo(struct capi_ctr *);
int ctl_read_proc(char *, char **, off_t ,int , int *, struct capi_ctr *);
int driver_read_proc(char *, char **, off_t ,int , int *, struct capi_driver *);

MODULE_DESCRIPTION(             "CAPI Proxy Client driver");
MODULE_AUTHOR(                  "[Butzisten] The Red Guy (adam(at)szalkowski.de) && ...");
MODULE_SUPPORTED_DEVICE(        "none");

static struct capi_driver_interface *di;

static struct capi_driver driver = {
    "",
    "",
    reset_ctr,            /* reset_ctr */ 								// Not needed
    remove_ctr,           /* remove_ctr */								// Not needed           
    register_appl,        /* register_appl */		          // forward to clients
    release_appl,         /* release_appl */							// forward to clients
    send_message,         /* send_message */							// receive message and forward to clients
    procinfo,             /* procinfo */	 								// dunno
    ctl_read_proc,        /* read_proc */				          // dunno2
    driver_read_proc,     /* driver_read_proc */					// dunno
    0,                    /* conf_driver */
    0,                    /* conf_controller */
};

/************** not needed ***************/

void reset_ctr(struct capi_ctr *ctrl)
{
       /* not needed */
}

void remove_ctr(struct capi_ctr *ctrl)
{
       /* not needed */
}

static spinlock_t api_lock;
static spinlock_t ll_lock;


/************* interface functions *************/

/* sending Messages to the CAPI (Application) 
We must write it by ourselves
  if (command == _DATA_B3_I)
    dlength = READ_WORD(((byte*)&msg.info.data_b3_ind.Data_Length));

  if (!(skb = alloc_skb(length + dlength, GFP_ATOMIC))) {
    printk(KERN_ERR "%s: alloc_skb failed, incoming msg dropped.\n", DRIVERLNAME);
    return;
  } else {
    write = (byte *) skb_put(skb, length + dlength);

    // copy msg header to sk_buff 
    memcpy(write, (byte *)&msg, length);

    // if DATA_B3_IND, copy data too
    if (command == _DATA_B3_I) {
      dword data = READ_DWORD(((byte*)&msg.info.data_b3_ind.Data));
      memcpy (write + length, (void *)data, dlength);
    }

    // find the card structure for this controller
		// We will take the controller itself!!!!! Card structure not needed
		
    if(!(card = find_card_by_ctrl(skb->data[8] & 0x7f))) {
      printk(KERN_ERR "%s: controller %d not found, incoming msg dropped.\n",
              DRIVERLNAME, skb->data[8] & 0x7f);
      DBG_ERR(("sendf - controller not found, incoming msg dropped"))
      kfree_skb(skb);
      return;
    }
    
    // tell capi that we have a new ncci assigned
		// Dunno wheter we have to do this
		
    if ((command == (_CONNECT_B3_R | CONFIRM)) ||
        (command == _CONNECT_B3_I)) {
          card->capi_ctrl->new_ncci(card->capi_ctrl, appl->Id,
                                    CAPIMSG_NCCI(skb->data), MAX_DATA_B3);
    }

    // tell capi that we have released the ncci
		// Also dunno
		
    if (command == _DISCONNECT_B3_I) {
      card->capi_ctrl->free_ncci(card->capi_ctrl, appl->Id, CAPIMSG_NCCI(skb->data));
    }

    // send capi msg to capi
		// !!!!!!!
    card->capi_ctrl->handle_capimsg(card->capi_ctrl, appl->Id, skb);
  }
}
*/

/************* proc functions *************/

/* try to find out, what it does and chnage it a little bit...
int diva_driver_read_proc(char *page, char **start, off_t off,int count, int *eof, struct capi_driver *driver)
{
  int len = 0;
  char tmprev[32];

  strcpy(tmprev, main_revision);
  len += sprintf(page+len, "%-16s divas\n", "name");
  len += sprintf(page+len, "%-16s %s/%s %s(%s)\n", "release",
                 DRIVERRELEASE, getrev(tmprev), diva_capi_common_code_build, DIVA_BUILD);
  len += sprintf(page+len, "%-16s %s\n", "author", "Cytronics & Melware / Eicon Networks");

  if (off + count >= len)
    *eof = 1;
  if (len < off)
    return 0;
  *start = page + off;
  return((count < len-off) ? count : len-off);
}

int diva_ctl_read_proc(char *page, char **start, off_t off,int count, int *eof, struct capi_ctr *ctrl)
{
  diva_card *card = ctrl->driverdata;
  int len = 0;

  len += sprintf(page+len, "%s\n", ctrl->name);
  len += sprintf(page+len, "Serial No. : %s\n", ctrl->serial);
  len += sprintf(page+len, "Id         : %d\n", card->Id);
  len += sprintf(page+len, "Channels   : %d\n", card->d.channels);

  if (off + count >= len)
    *eof = 1;
  if (len < off)
    return 0;
  *start = page + off;
  return((count < len-off) ? count : len-off);
}

static char *diva_procinfo(struct capi_ctr *ctrl)
{
  return(ctrl->serial);
}
*/

/************** capi functions ***********/

/*
**  register appl
*/

void register_appl(struct capi_ctr *ctrl, __u16 appl, capi_register_params *rp)
{
  MOD_INC_USE_COUNT;

 // We need to allocate some mem for Data (perhaps not needed if we are allowed to transmit the data directly to the server))
  bnum = rp->level3cnt * rp->datablkcnt; // bytes for DataNCCI
  xnum = rp->level3cnt * MAX_DATA_B3;    // bytes for 

	// but we need;
	
	void** ReceiveBufferForMessages // from CAPI  // bnum * rp->datablklen
	void** SendBufferForMessages    //to CAPI       // 
	// use kmalloc/kfree
	// what is vmalloc?
	
  // tell capi that we have done our work
	// Problem (!!!): Registering the App at the Server assigns us a new Apllid. But this Id is different from the ApplId
	// CAPI already assigned to the App! So, we must make up a table for translating ApplId's !!!!!!!!!
  ctrl->appl_registered(ctrl, appl);

  /*
  DBG_LOG(("CAPI_REGISTER - Id = %d", appl))
  DBG_LOG(("  MaxLogicalConnections = %d", rp->level3cnt))
  DBG_LOG(("  MaxBDataBuffers       = %d", rp->datablkcnt))
  DBG_LOG(("  MaxBDataLength        = %d", rp->datablklen))
  */

  // dunno whatr this does
	CapiRegister(appl);
	
  /* tell kcapi that we actually registerd the appl */
  ctrl->appl_registered(ctrl, appl);
} 


/*
**  release appl
*/

void release_appl(struct capi_ctr *ctrl, __u16 appl)
{
  spin_lock(&api_lock);
  
	// dunno where it comes from
	CapiRelease(appl);
	// free the mem

  spin_unlock(&api_lock);

  /* tell kcapi that we actually released the appl */
  ctrl->appl_released(ctrl, appl);
  MOD_DEC_USE_COUNT;
} 


/*
** Receives Message from Capi     
*/
void send_message(struct capi_ctr *ctrl, struct sk_buff *skb)
{
  char *msg = skb->data;
  __u32 length = skb->len;

  //put the message on some kind of queue and notify threads waiting for messages 
	//(see getMessage()) 
	
  kfree_skb(skb);
}

/************ module functions ***********/

/* I think this is no CAPI funtion but some self-made one.
   so it should be changed a little bit and detach all deamons instead */
static void __exit remove_cards(void)
// Unregister all
{
  spin_lock(&ll_lock);

  // for all cards (daemons)
	// YOU SHOULD TAKE A CLOSE LOOK AT THE di=DIRVER_INTERFACE FUNCTIONS!
	// I DON'T KNOW HOW THEY WORK AND WHAT THEY ARE EXACTLY USED FOR!
	  di->detach_ctr(card->capi_ctrl);
    card = card->next;

    kfree(card);
  }
  spin_unlock(&ll_lock);
}

/* look fct. above. It should be accessible for the daemon somehow.
   Use it to detach a deamon from the driver (this will look fer the kernelcapi 
	 like you removing a card from the driver) */
static void remove_card(DESCRIPTOR *d)
// UnregisterClient (deamon)
{
  spin_lock(&ll_lock);

    if (card->d.request == d->request)
    {
      // this seems to be the only interesting thing here
			di->detach_ctr(card->capi_ctrl);
      kfree(card);
		}


  spin_unlock(&ll_lock);
}


static int add_card(DESCRIPTOR *d)
// RegisterClient
{
  struct capi_ctr *ctrl = NULL;

  card->capi_ctrl = di->attach_ctr(&capi_driver, card->name, card))) {
  kfree(card);

  card->Id = find_free_id();
  ctrl = card->capi_ctrl;

  // Give the kernelcapi some info about our card/driver
	strncpy(ctrl->manu, M_COMPANY, CAPI_MANUFACTURER_LEN);
  ctrl->version.majorversion = 2;
  ctrl->version.minorversion = 0;
  ctrl->version.majormanuversion = DRRELMAJOR;
  ctrl->version.minormanuversion = DRRELMINOR;

  serial[CAPI_SERIAL_LEN-1] = 0;
  strncpy(ctrl->serial, serial, CAPI_SERIAL_LEN);

  // dunno whatfor this is used
	// perhaps not that interesting
	ControllerMap[card->Id] = (byte)(ctrl->cnr);

  /* profile information */
  ctrl->profile.nbchannel = card->d.channels;
  ctrl->profile.goptions = a->profile.Global_Options;
  ctrl->profile.support1 = a->profile.B1_Protocols;
  ctrl->profile.support2 = a->profile.B2_Protocols;
  ctrl->profile.support3 = a->profile.B3_Protocols;
  /* manufacturer profile information */
  ctrl->profile.manu[0] = a->man_profile.private_options;
  ctrl->profile.manu[1] = a->man_profile.rtp_primary_payloads;
  ctrl->profile.manu[2] = a->man_profile.rtp_additional_payloads;
  ctrl->profile.manu[3] = 0;
  ctrl->profile.manu[4] = 0;

  ctrl->ready(ctrl);
  
	// I'm not too sure whether there are some more values we should set!!!!
	// Take a look at the CAPI docs
	
  max_adapter++;
  return(1);
}

static void clean_adapter(int id)
// dunno whether we need something like this
{
  if(adapter[id].plci)
    vfree(adapter[id].plci);

  max_adapter--;
}

static int __init driver_init(void)
{
  MOD_INC_USE_COUNT;

  api_lock = SPIN_LOCK_UNLOCKED;
  ll_lock = SPIN_LOCK_UNLOCKED;

  // THIS di STUFF SEEMS TO BE QUITE IMPORTANT, SO SAVE IT SOMEWHERE IN GLOBAL CONTEXT!!!
	di = attach_capi_driver(&capi_driver);

  // on error (and on exit perhaps)
    detach_capi_driver(&capi_driver);
    MOD_DEC_USE_COUNT;

  return ret;
}


static void __exit diver_exit(void)
{
  int count = 100;
  word ret = 1;

  // This is up to you to know whether and how this must be done
	// at least remove all cards
	while(ret && count--)
  {
    ret = api_remove_start();
    set_current_state(TASK_INTERRUPTIBLE);
    schedule_timeout(10);
	}
	
  detach_capi_driver(&capi_driver);
  printk(KERN_INFO "%s: module unloaded.\n", DRIVERLNAME);
}

void getMessage(char* msg/*, int len*/) // perhaps we can define a size for all msgs
// must be able to be called from the deamon 
{
  /* look wheter there are already messages queued for this device (i think we will need 
	several different message queues, one for every daemon (or we allow only one daemon))*/
	
	
	/* if so, get the message from the queue and return it to the caller */
	
	/* if not wait, until you get some signal from send_message() */
	
	/* get the message from the queue and return it to the sender */
}

int putMessage(char* msg, int len) //or skb
{
  /* get some capi_ctr and use its handle_msg() funtion to transmit the message */
	
	/*eventually we have to parse it to see whether we have to add a NCCI or stuff
	and translate some values (we have to find out what has to be translated and where)*/

	/* return */
}

module_init(driver_init);
module_exit(driver_exit);