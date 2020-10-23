// SPDX-License-Identifier: GPL-2.0+
/*
 * vsp1_dl.c  --  R-Car VSP1 Display List
 *
 * Copyright (C) 2020 GlobalLogic
 * Copyright (C) 2015-2018 Renesas Corporation
 *
 * Contact: Laurent Pinchart (laurent.pinchart@ideasonboard.com)
 */
#include <common.h>
#include <linux/compat.h>
#include "vsp1.h"
#include "vsp1_drm.h"
#include "vsp1_pipe.h"
#include "vsp1_rwpf.h"
#include "vsp1_dl.h"

#define VSP1_DL_NUM_ENTRIES			256
#define VSP1_DL_EXT_NUM_ENTRIES		160

#define VSP1_DLH_INT_ENABLE			(1 << 1)
#define VSP1_DLH_AUTO_START			(1 << 0)

struct vsp1_dl_header_list {
	u32 num_bytes;
	u32 addr;
} __attribute__((__packed__));

struct vsp1_dl_header {
	u32 num_lists;
	struct vsp1_dl_header_list lists[8];
	u32 next_header;
	u32 flags;
	/* if (VI6_DL_EXT_CTRL.EXT) */
	u32 zero_bits;
	/* zero_bits:6 + pre_ext_dl_exec:1 + */
	/* post_ext_dl_exec:1 + zero_bits:8 + pre_ext_dl_num_cmd:16 */
	u32 pre_post_num;
	u32 pre_ext_dl_plist;
	/* zero_bits:16 + post_ext_dl_num_cmd:16 */
	u32 post_ext_dl_num_cmd;
	u32 post_ext_dl_p_list;
} __attribute__((__packed__));

struct vsp1_ext_dl_body {
	u32 ext_dl_cmd[2];
	u32 ext_dl_data[2];
} __attribute__((__packed__));

struct vsp1_ext_addr {
	u32 addr;
} __attribute__((__packed__));

struct vsp1_dl_entry {
	u32 addr;
	u32 data;
} __attribute__((__packed__));

struct vsp1_dl_body {
	struct list_head list;
	struct list_head free;

	size_t refcnt;

	struct vsp1_dl_body_pool *pool;
	struct vsp1_device *vsp1;

	struct vsp1_ext_dl_body *ext_body;
	dma_addr_t ext_dma;

	struct vsp1_ext_addr *src_dst_addr;
	dma_addr_t ext_addr_dma;

	struct vsp1_dl_entry *entries;
	dma_addr_t dma;
	size_t size;

	unsigned int num_entries;
	unsigned int max_entries;
};

struct vsp1_dl_body_pool {
	/* DMA allocation */
	dma_addr_t dma;
	size_t size;
	void *mem;
	/* Body management */
	struct vsp1_dl_body *bodies;
	struct list_head free;
	struct vsp1_device *vsp1;
};

struct vsp1_dl_list {
	struct list_head list;
	struct vsp1_dl_manager *dlm;

	struct vsp1_dl_header *header;
	dma_addr_t dma;

	struct vsp1_dl_body *body0;
	struct list_head bodies;
};

struct vsp1_dl_manager {
	struct vsp1_device *vsp1;

	struct list_head free;
	struct vsp1_dl_list *queued;

	struct vsp1_dl_body_pool *pool;
};

/* -----------------------------------------------------------------------------
 * Display List Body Management
 */
struct vsp1_dl_body_pool *
vsp1_dl_body_pool_create(struct vsp1_device *vsp1, unsigned int num_bodies,
			 unsigned int num_entries, size_t extra_size)
{
	struct vsp1_dl_body_pool *pool;
	size_t dlb_size;
	unsigned int i;

	pool = kzalloc(sizeof(*pool), GFP_KERNEL);
	if (!pool)
		return NULL;

	pool->vsp1 = vsp1;

	dlb_size = num_entries * sizeof(struct vsp1_dl_entry) + extra_size;
	pool->size = dlb_size * num_bodies;

	pool->bodies = kcalloc(num_bodies, sizeof(*pool->bodies), GFP_KERNEL);

	if (!pool->bodies) {
		kfree(pool);
		return NULL;
	}

	pool->mem = kzalloc(pool->size, GFP_KERNEL); //0x58080000
	pool->dma = (dma_addr_t)(pool->mem);
	if (!pool->mem) {
		kfree(pool->bodies);
		kfree(pool);
		return NULL;
	}

	INIT_LIST_HEAD(&pool->free);

	for (i = 0; i < num_bodies; ++i) {
		struct vsp1_dl_body *dlb = &pool->bodies[i];
		size_t header_offset;
		size_t ex_body_offset;
		size_t ex_addr_offset;

		dlb->pool = pool;
		dlb->max_entries = num_entries;

		dlb->dma = pool->dma + i * dlb_size;
		dlb->entries = pool->mem + i * dlb_size;

		header_offset = dlb->max_entries *
				sizeof(*dlb->entries);
		ex_body_offset = sizeof(struct vsp1_dl_header);
		ex_addr_offset = sizeof(struct vsp1_ext_dl_body);

		dlb->ext_dma = pool->dma + (i * dlb_size) +
				header_offset + ex_body_offset;
		dlb->ext_body = pool->mem + (i * dlb_size) +
				header_offset + ex_body_offset;
		dlb->ext_addr_dma = pool->dma + (i * dlb_size) +
				header_offset + ex_body_offset +
				ex_addr_offset;
		dlb->src_dst_addr = pool->mem + (i * dlb_size) +
				header_offset + ex_body_offset +
				ex_addr_offset;

		list_add_tail(&dlb->free, &pool->free);
	}

	return pool;
}

void vsp1_dl_body_pool_destroy(struct vsp1_dl_body_pool *pool)
{
	if (!pool)
		return;
	if (pool->mem) {
		kfree(pool->mem);
	}
	kfree(pool->bodies);
	kfree(pool);
}

struct vsp1_dl_body *vsp1_dl_body_get(struct vsp1_dl_body_pool *pool)
{
	struct vsp1_dl_body *dlb = NULL;

	if (!list_empty(&pool->free)) {
		dlb = list_first_entry(&pool->free, struct vsp1_dl_body, free);
		list_del(&dlb->free);
		dlb->refcnt = 1;
	}

	return dlb;
}

void vsp1_dl_body_put(struct vsp1_dl_body *dlb)
{
	if (!dlb) {
		return;
	}

	dlb->refcnt--;
	if (!dlb->refcnt) {
		return;
	}

	dlb->num_entries = 0;

	list_add_tail(&dlb->free, &dlb->pool->free);
}

void vsp1_dl_body_write(struct vsp1_dl_body *dlb, u32 reg, u32 data)
{
	if (dlb->num_entries >= dlb->max_entries) {
		printf("DLB size exceeded (max %u)", dlb->max_entries);
		return;
	}
	dlb->entries[dlb->num_entries].addr = reg;
	dlb->entries[dlb->num_entries].data = data;
	dlb->num_entries++;
}

/* -----------------------------------------------------------------------------
 * Display List Transaction Management
 */
void vsp1_dl_set_addr_auto_fld(struct vsp1_dl_body *dlb)
{
	u32 width, stride;

	width = ALIGN(CONFIG_VIDEO_RENESAS_DISPLAY_WIDTH, 16);
	stride = width * 32 / 8;

	//Y top, Y bot, U, V
	dlb->src_dst_addr[0].addr = CONFIG_VIDEO_RENESAS_BUF_ADDR;
	dlb->src_dst_addr[1].addr = CONFIG_VIDEO_RENESAS_BUF_ADDR + stride;
	dlb->src_dst_addr[2].addr = 0;
	dlb->src_dst_addr[3].addr = 0 + stride;
	dlb->src_dst_addr[4].addr = 0;
	dlb->src_dst_addr[5].addr = 0 + stride;
}

static struct vsp1_dl_list *vsp1_dl_list_alloc(struct vsp1_dl_manager *dlm)
{
	struct vsp1_dl_list *dl;

	dl = kzalloc(sizeof(*dl), GFP_KERNEL);
	if (!dl)
		return NULL;

	INIT_LIST_HEAD(&dl->bodies);
	dl->dlm = dlm;

	/* Get a default body for our list. */
	dl->body0 = vsp1_dl_body_get(dlm->pool);
	if (!dl->body0)
		return NULL;

	size_t header_offset = dl->body0->max_entries
				 * sizeof(*dl->body0->entries);

	dl->header = ((void *)dl->body0->entries) + header_offset;
	dl->dma = dl->body0->dma + header_offset;

	memset(dl->header, 0, sizeof(*dl->header));
	dl->header->lists[0].addr = dl->body0->dma;

	return dl;
}

static void vsp1_dl_list_bodies_put(struct vsp1_dl_list *dl)
{
	struct vsp1_dl_body *dlb, *tmp;

	list_for_each_entry_safe(dlb, tmp, &dl->bodies, list) {
		list_del(&dlb->list);
		vsp1_dl_body_put(dlb);
	}
}

static void vsp1_dl_list_free(struct vsp1_dl_list *dl)
{
	vsp1_dl_body_put(dl->body0);
	vsp1_dl_list_bodies_put(dl);

	kfree(dl);
}

struct vsp1_dl_list *vsp1_dl_list_get(struct vsp1_dl_manager *dlm)
{
	struct vsp1_dl_list *dl = NULL;

	if (!list_empty(&dlm->free)) {
		dl = list_first_entry(&dlm->free, struct vsp1_dl_list, list);
		list_del(&dl->list);
	}

	return dl;
}

static void __vsp1_dl_list_put(struct vsp1_dl_list *dl)
{
	if (!dl) {
		return;
	}

	vsp1_dl_list_bodies_put(dl);
	/*
	 * body0 is reused as as an optimisation as presently every display list
	 * has at least one body, thus we reinitialise the entries list.
	 */
	dl->body0->num_entries = 0;

	list_add_tail(&dl->list, &dl->dlm->free);
}

struct vsp1_dl_body *vsp1_dl_list_get_body0(struct vsp1_dl_list *dl)
{
	return dl->body0;
}

#define list_next_entry(pos, member) \
	list_entry((pos)->member.next, typeof(*(pos)), member)

static void vsp1_dl_list_fill_header(struct vsp1_dl_list *dl, bool is_last)
{
	struct vsp1_dl_manager *dlm = dl->dlm;
	struct vsp1_dl_header_list *hdr = dl->header->lists;
	struct vsp1_dl_body *dlb;
	unsigned int num_lists = 0;
	struct vsp1_device *vsp1 = dlm->vsp1;
	struct vsp1_drm_pipeline *drm_pipe = &vsp1->drm->pipe;
	struct vsp1_pipeline *pipe = &drm_pipe->pipe;
	unsigned int i, rpf_update = 0;

	/*
	 * Fill the header with the display list bodies addresses and sizes. The
	 * address of the first body has already been filled when the display
	 * list was allocated.
	 */
	hdr->num_bytes = dl->body0->num_entries
		       * sizeof(*dl->header->lists);

	list_for_each_entry(dlb, &dl->bodies, list) {
		num_lists++;
		hdr++;

		hdr->addr = dlb->dma;
		hdr->num_bytes = dlb->num_entries
			       * sizeof(*dl->header->lists);
	}

	dl->header->num_lists = num_lists;

	/*
	 * If the display list manager works in continuous mode, the VSP
	 * should loop over the display list continuously until
	 * instructed to do otherwise.
	 */
	dl->header->next_header = dl->dma;
	dl->header->flags = VSP1_DLH_INT_ENABLE | VSP1_DLH_AUTO_START;

	for (i = 0; i < 1; ++i) { //TODO
		if (!pipe->inputs)
			continue;
		rpf_update |= 0x01 << (16 + i);
	}

	/* Set extended display list header */
	/* pre_ext_dl_exec = 1, pre_ext_dl_num_cmd = 1 */
	dl->header->pre_post_num = (1 << 25) | (0x01);
	dl->header->pre_ext_dl_plist = dl->body0->ext_dma;
	dl->header->post_ext_dl_num_cmd = 0;
	dl->header->post_ext_dl_p_list = 0;

	/* Set extended display list (Auto-FLD) */
	/* Set opecode */
	dl->body0->ext_body->ext_dl_cmd[0] = 0x00000003;
	/* RPF[0]-[4] address is updated */
	dl->body0->ext_body->ext_dl_cmd[1] =
				0x00000001 | rpf_update;

	/* Set pointer of source/destination address */
	dl->body0->ext_body->ext_dl_data[0] =
				dl->body0->ext_addr_dma;
	/* Should be set to 0 */
	dl->body0->ext_body->ext_dl_data[1] = 0;
}

static void vsp1_dl_list_hw_enqueue(struct vsp1_dl_list *dl)
{
	vsp1_write(VI6_DL_HDR_ADDR(0), dl->dma);
}

static void vsp1_dl_list_commit_continuous(struct vsp1_dl_list *dl)
{
	struct vsp1_dl_manager *dlm = dl->dlm;
	/*
	 * Pass the new display list to the hardware and mark it as queued. It
	 * will become active when the hardware starts processing it.
	 */
	vsp1_dl_list_hw_enqueue(dl);

	__vsp1_dl_list_put(dlm->queued);
	dlm->queued = dl;
}

void vsp1_dl_list_commit(struct vsp1_dl_list *dl)
{
	/* Fill the header for the head */
	vsp1_dl_list_fill_header(dl, 0);
	vsp1_dl_list_commit_continuous(dl);
}

/* Hardware Setup */
void vsp1_dlm_setup(void)
{
	u32 ctrl = (256 << VI6_DL_CTRL_AR_WAIT_SHIFT)
		 | VI6_DL_CTRL_DC2 | VI6_DL_CTRL_DC1 | VI6_DL_CTRL_DC0
		 | VI6_DL_CTRL_DLE;

	vsp1_write(VI6_DL_EXT_CTRL(0),
		   (0x02 << VI6_DL_EXT_CTRL_POLINT_SHIFT) |
		   VI6_DL_EXT_CTRL_DLPRI | VI6_DL_EXT_CTRL_EXT);
	/*
	 * Enable VSP2 display list function.
	 * The WPF0 operates in Continuous Frame
	 * Mode, use Display List Header (Normal DL Mode)
	 */
	vsp1_write(VI6_DL_CTRL, ctrl);//VI6_DL_CTRL=1001111
	/* Swapping in long word (32-bit) units is enabled */
	vsp1_write(VI6_DL_SWAP(0), VI6_DL_SWAP_LWS);
}

void vsp1_dlm_reset(struct vsp1_dl_manager *dlm)
{
	__vsp1_dl_list_put(dlm->queued);
	dlm->queued = NULL;
}

struct vsp1_dl_manager *vsp1_dlm_create(struct vsp1_device *vsp1,
					unsigned int prealloc)
{
	static struct vsp1_dl_manager dl_manager;
	struct vsp1_dl_manager *dlm;
	size_t header_size;
	unsigned int i;

	memset(&dl_manager, 0, sizeof(struct vsp1_dl_manager));
	dlm = &dl_manager;
	if (!dlm)
		return NULL;

	dlm->vsp1 = vsp1;

	INIT_LIST_HEAD(&dlm->free);
	header_size = ALIGN(sizeof(struct vsp1_dl_header), 8);

	size_t ex_addr_offset;
	size_t ex_addr_size;

	ex_addr_offset = sizeof(struct vsp1_ext_dl_body);
	ex_addr_size = sizeof(struct vsp1_ext_dl_body)
			* VSP1_DL_EXT_NUM_ENTRIES;
	header_size += (ex_addr_offset + ex_addr_size);

	dlm->pool = vsp1_dl_body_pool_create(vsp1, prealloc + 1,
					     VSP1_DL_NUM_ENTRIES, header_size);

	if (!dlm->pool)
		return NULL;

	for (i = 0; i < prealloc; ++i) {
		struct vsp1_dl_list *dl;

		dl = vsp1_dl_list_alloc(dlm);
		if (!dl) {
			vsp1_dlm_destroy(dlm);
			return NULL;
		}

		list_add_tail(&dl->list, &dlm->free);
	}

	return dlm;
}

void vsp1_dlm_destroy(struct vsp1_dl_manager *dlm)
{
	struct vsp1_dl_list *dl, *next;

	if (!dlm)
		return;

	list_for_each_entry_safe(dl, next, &dlm->free, list) {
		list_del(&dl->list);
		vsp1_dl_list_free(dl);
	}

	vsp1_dl_body_pool_destroy(dlm->pool);
}
