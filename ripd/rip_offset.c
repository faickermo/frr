/* RIP offset-list
 * Copyright (C) 2000 Kunihiro Ishiguro <kunihiro@zebra.org>
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; see the file COPYING; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <zebra.h>

#include "if.h"
#include "prefix.h"
#include "filter.h"
#include "command.h"
#include "linklist.h"
#include "memory.h"

#include "ripd/ripd.h"

static struct list *rip_offset_list_master;

#define OFFSET_LIST_IN_NAME(O)  ((O)->direct[RIP_OFFSET_LIST_IN].alist_name)
#define OFFSET_LIST_IN_METRIC(O)  ((O)->direct[RIP_OFFSET_LIST_IN].metric)

#define OFFSET_LIST_OUT_NAME(O)  ((O)->direct[RIP_OFFSET_LIST_OUT].alist_name)
#define OFFSET_LIST_OUT_METRIC(O)  ((O)->direct[RIP_OFFSET_LIST_OUT].metric)

struct rip_offset_list *rip_offset_list_new(const char *ifname)
{
	struct rip_offset_list *offset;

	offset = XCALLOC(MTYPE_RIP_OFFSET_LIST, sizeof(struct rip_offset_list));
	offset->ifname = strdup(ifname);
	listnode_add_sort(rip_offset_list_master, offset);

	return offset;
}

void offset_list_del(struct rip_offset_list *offset)
{
	listnode_delete(rip_offset_list_master, offset);
	if (OFFSET_LIST_IN_NAME(offset))
		free(OFFSET_LIST_IN_NAME(offset));
	if (OFFSET_LIST_OUT_NAME(offset))
		free(OFFSET_LIST_OUT_NAME(offset));
	free(offset->ifname);
	XFREE(MTYPE_RIP_OFFSET_LIST, offset);
}

struct rip_offset_list *rip_offset_list_lookup(const char *ifname)
{
	struct rip_offset_list *offset;
	struct listnode *node, *nnode;

	for (ALL_LIST_ELEMENTS(rip_offset_list_master, node, nnode, offset)) {
		if (strcmp(offset->ifname, ifname) == 0)
			return offset;
	}
	return NULL;
}

/* If metric is modifed return 1. */
int rip_offset_list_apply_in(struct prefix_ipv4 *p, struct interface *ifp,
			     uint32_t *metric)
{
	struct rip_offset_list *offset;
	struct access_list *alist;

	/* Look up offset-list with interface name. */
	offset = rip_offset_list_lookup(ifp->name);
	if (offset && OFFSET_LIST_IN_NAME(offset)) {
		alist = access_list_lookup(AFI_IP, OFFSET_LIST_IN_NAME(offset));

		if (alist
		    && access_list_apply(alist, (struct prefix *)p)
			       == FILTER_PERMIT) {
			*metric += OFFSET_LIST_IN_METRIC(offset);
			return 1;
		}
		return 0;
	}
	/* Look up offset-list without interface name. */
	offset = rip_offset_list_lookup("*");
	if (offset && OFFSET_LIST_IN_NAME(offset)) {
		alist = access_list_lookup(AFI_IP, OFFSET_LIST_IN_NAME(offset));

		if (alist
		    && access_list_apply(alist, (struct prefix *)p)
			       == FILTER_PERMIT) {
			*metric += OFFSET_LIST_IN_METRIC(offset);
			return 1;
		}
		return 0;
	}
	return 0;
}

/* If metric is modifed return 1. */
int rip_offset_list_apply_out(struct prefix_ipv4 *p, struct interface *ifp,
			      uint32_t *metric)
{
	struct rip_offset_list *offset;
	struct access_list *alist;

	/* Look up offset-list with interface name. */
	offset = rip_offset_list_lookup(ifp->name);
	if (offset && OFFSET_LIST_OUT_NAME(offset)) {
		alist = access_list_lookup(AFI_IP,
					   OFFSET_LIST_OUT_NAME(offset));

		if (alist
		    && access_list_apply(alist, (struct prefix *)p)
			       == FILTER_PERMIT) {
			*metric += OFFSET_LIST_OUT_METRIC(offset);
			return 1;
		}
		return 0;
	}

	/* Look up offset-list without interface name. */
	offset = rip_offset_list_lookup("*");
	if (offset && OFFSET_LIST_OUT_NAME(offset)) {
		alist = access_list_lookup(AFI_IP,
					   OFFSET_LIST_OUT_NAME(offset));

		if (alist
		    && access_list_apply(alist, (struct prefix *)p)
			       == FILTER_PERMIT) {
			*metric += OFFSET_LIST_OUT_METRIC(offset);
			return 1;
		}
		return 0;
	}
	return 0;
}

static int offset_list_cmp(struct rip_offset_list *o1,
			   struct rip_offset_list *o2)
{
	return strcmp(o1->ifname, o2->ifname);
}

void rip_offset_init(void)
{
	rip_offset_list_master = list_new();
	rip_offset_list_master->cmp = (int (*)(void *, void *))offset_list_cmp;
	rip_offset_list_master->del = (void (*)(void *))offset_list_del;
}

void rip_offset_clean(void)
{
	list_delete(&rip_offset_list_master);

	rip_offset_list_master = list_new();
	rip_offset_list_master->cmp = (int (*)(void *, void *))offset_list_cmp;
	rip_offset_list_master->del = (void (*)(void *))offset_list_del;
}
