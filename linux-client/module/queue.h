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
 * Revision 1.2  2002/03/03 20:38:19  butzist
 * added Log history
 *
 */

/*
    Header file for the message queue functions
*/

struct queue
{
	int	length;
	int	write_pos;
	int	read_pos;
	spinlock_t	lock;

	struct	sk_buff **skb;
};

int queue_length(struct queue *q);
void queue_inc_read_pos(struct queue *q);
void queue_inc_write_pos(struct queue *q);
bool queue_full(struct queue *q);
bool queue_empty(struct queue *q);
bool queue_append(struct queue *q, struct sk_buff* skb);
sk_buff* queue_top(struct queue *q);
void queue_init(struct queue *q);
void queue_free(struct queue *q);