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
 */

/*
    Main source file for the message queue
*/

#include "queue.h"
#include <linux/smp_lock.h>
#include <linux/vmalloc.h>

inline bool _queue_full(struct queue *q)
{
	return (q->skb[q->write_pos]!=NULL);
}

inline bool _queue_empty(struct queue *q)
{
	return (q->skb[q->read_pos]==NULL);
}

inline int _queue_length(struct queue *q)
// retrun the length of the queue (index starts with 0)
{
	
	if(q->write_pos==q->read_pos)
	{
		if(_queue_full(q))
		{
			return q->length;
		} else {
			return 0;
		}
	} else {
		if(q->write_pos>q->read_pos)
		{
			return q->write_pos-q->read_pos;
		} else {
			return (q->length-1)+(q->write_pos-q->read_pos);
		}
	}
}

inline void _queue_inc_read_pos(struct queue *q)
{
	if(q->read_pos+1==q->length)
	{
		q->read_pos=0;
	} else {
		q->read_pos++;
	}
}

inline void _queue_inc_write_pos(struct queue *q)
{
	if(q->write_pos+1==q->length)
	{
		q->write_pos=0;
	} else {
		q->write_pos++;
	}
}

inline bool _queue_append(struct queue *q, struct sk_buff* skb)
{
    if(_queue_full(q))
    {
    	return false;
    }

    q->skb[q->write_pos]=skb;
    _queue_inc_write_pos(q);

    return true;
}

inline sk_buff* _queue_top(struct queue *q)
{
    if(_queue_empty(q))
    {
    	return NULL;
    }

    int pos=q->read_pos;

    _queue_inc_read_pos(q);

    return q->skb[pos];
}

inline void _queue_init(struct queue *q)
{
    q->write_pos=0;
    q->read_pos=0;
    spin_init(&(q->lock));
}

inline void _queue_free(struct queue *q)
{
    len=_queue_length(&(card[i].msg_queue));
	    
    for(int j=0; j<len; j++)
    {
	skb=_queue_top(&(card[i].msg_queue));
	
	if(skb!=NULL)
	{
	    kfree_skb(skb);
	}
    }
    _queue_init(q);    
}

int queue_length(struct queue *q)
{
    spin_lock(&(q->lock));
    int ret=_queue_length(q);
    spin_lock(&(q->lock));
    
    return ret;
}
    
void queue_inc_read_pos(struct queue *q)
{
    spin_lock(&(q->lock));
    _queue_inc_read_pos(q);
    spin_unlock(&(q->lock));
}

void queue_inc_write_pos(struct queue *q)
{
    spin_lock(&(q->lock));
    _queue_inc_write_pos(p);
    spin_unlock(&(q->lock));
}

bool queue_full(struct queue *q)
{
    spin_lock(&(q->lock));
    bool ret=_queue_full(q);
    spin_unlock(&(q->lock));
    
    return ret;
}    

bool queue_empty(struct queue *q)
{
    spin_lock(&(q->lock));
    bool ret=_queue_empty(q);
    spin_unlock(&(q->lock));
    
    return ret;
}    

bool queue_append(struct queue *q, struct sk_buff* skb)
{
    spin_lock(&(q->lock));
    bool ret=_queue_append(q,skb);
    spin_unlock(&(q->lock));
    
    return ret;
}

sk_buff* queue_top(struct queue *q)
{
    spin_lock(&(q->lock));
    sk_buff* ret=_queue_top(q);
    spin_unlock(&(q->lock));
    
    return ret;
}

void queue_init(struct queue *q)
{
    spin_lock(&(q->lock));
    _queue_init(q);
    spin_unlock(&(q->lock));
}

void queue_free(struct queue *q)
{
    spin_lock(&(q->lock));
    _queue_free(q);
    spin_unlock(&(q->lock));
}
