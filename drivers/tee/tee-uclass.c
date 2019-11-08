// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2018 Linaro Limited
 */

#include <common.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <dm/uclass-internal.h>
#include <tee.h>
#include <linux/kernel.h>

static const struct tee_driver_ops *tee_get_ops(struct udevice *dev)
{
	return device_get_ops(dev);
}

void tee_get_version(struct udevice *dev, struct tee_version_data *vers)
{
	tee_get_ops(dev)->get_version(dev, vers);
}

int tee_open_session(struct udevice *dev, struct tee_open_session_arg *arg,
		     uint num_param, struct tee_param *param)
{
	return tee_get_ops(dev)->open_session(dev, arg, num_param, param);
}

int tee_close_session(struct udevice *dev, u32 session)
{
	return tee_get_ops(dev)->close_session(dev, session);
}

int tee_invoke_func(struct udevice *dev, struct tee_invoke_arg *arg,
		    uint num_param, struct tee_param *param)
{
	return tee_get_ops(dev)->invoke_func(dev, arg, num_param, param);
}

void tee_shm_config_map(struct udevice *dev,
		    struct tee_shm_pool_mem_info *shm_pool)
{
	tee_get_ops(dev)->shm_config_map(dev, shm_pool);
}

static void free_tee_shmem_from_pool(struct tee_shm *shm);
static struct tee_shm *alloc_tee_shmem_from_pool(struct udevice *dev,
	ulong size, ulong align, ulong flags)
{
	struct tee_uclass_priv *priv = dev_get_uclass_priv(dev);
	struct tee_shm_pool_mem_info shm_pool;
	struct list_head *pos;
	struct tee_shm *curr;
	struct tee_shm *new_elem;
	ulong aligned_offset;
	uint8_t *end_ptr;

	struct tee_shm *first_elem = NULL;
	struct list_head *mem_pools = &priv->list_shm;

	if (!size)
		return NULL;

	tee_shm_config_map(dev, &shm_pool);

	if (list_empty(mem_pools)) {
		new_elem = calloc(1, sizeof(*new_elem));
		if (!new_elem)
			return NULL;

		new_elem->dev = dev;
		new_elem->addr = (void *)shm_pool.vaddr;
		new_elem->size = shm_pool.size;
		list_add_tail(&new_elem->link, mem_pools);
		first_elem = new_elem;
	}

	list_for_each(pos, mem_pools) {
		curr = list_entry(pos, struct tee_shm, link);
		if (curr->flags & TEE_SHM_ALLOC)
			continue;

		aligned_offset = (ulong)ALIGN((ulong)curr->addr, align) - (ulong)curr->addr;

		if (curr->size < aligned_offset + size)
			continue;

		new_elem = calloc(1, sizeof(*new_elem));
		if (!new_elem) {
			free_tee_shmem_from_pool(first_elem);
			return NULL;
		}

		end_ptr = (uint8_t *)curr->addr + curr->size;

		curr->flags = flags;
		curr->size = size;
		curr->addr = (void *)(curr->addr + aligned_offset);

		new_elem->dev = dev;
		new_elem->addr = (void *)(ulong)ALIGN((ulong)curr->addr + curr->size, 8);
		new_elem->size = end_ptr - (uint8_t *)new_elem->addr;

		list_add(&new_elem->link, &curr->link);

		return curr;
	}

	return NULL;
}

static void free_tee_shmem_from_pool(struct tee_shm *shm)
{
	struct tee_shm *prev_entry;
	struct tee_shm *next_entry;
	struct list_head *prev;
	struct list_head *curr;
	struct list_head *next;
	struct tee_uclass_priv *priv;

	if (!shm)
		return;

	shm->flags = 0;
	curr = &shm->link;
	priv = dev_get_uclass_priv(shm->dev);

	prev = curr->prev;
	next = curr->next;

	if (next != &priv->list_shm) {
		next_entry = list_entry(next, struct tee_shm, link);
		if (!next_entry->flags) {
			shm->size += next_entry->size;
			list_del(next);
			free(next_entry);
		}
	}

	if (prev != &priv->list_shm) {
		prev_entry = list_entry(prev, struct tee_shm, link);
		if (!prev_entry->flags) {
			prev_entry->size += shm->size;
			list_del(curr);
			free(shm);
		}
	}

	if (list_empty(&priv->list_shm))
		return;

	/* Delete the last free block */
	if (priv->list_shm.next->next == priv->list_shm.next->prev) {
		shm = list_entry(priv->list_shm.next, struct tee_shm, link);
		list_del(priv->list_shm.next);
		free(shm);
	}
}

int __tee_shm_add(struct udevice *dev, ulong align, void *addr, ulong size,
		  u32 flags, struct tee_shm **shmp)
{
	struct tee_version_data v;
	struct tee_shm *shm;
	void *p = addr;
	int rc;

	tee_get_ops(dev)->get_version(dev, &v);

	if (flags & TEE_SHM_ALLOC) {
		if (!(v.gen_caps & TEE_GEN_CAP_REG_MEM)) {
			*shmp = alloc_tee_shmem_from_pool(dev, size, align, flags);
			if (!*shmp)
				return -ENOMEM;
			return 0;
		}

		if (align)
			p = memalign(align, size);
		else
			p = malloc(size);
	}
	if (!p)
		return -ENOMEM;

	shm = calloc(1, sizeof(*shm));
	if (!shm) {
		rc = -ENOMEM;
		goto err;
	}

	shm->dev = dev;
	shm->addr = p;
	shm->size = size;
	shm->flags = flags;

	if (flags & TEE_SHM_SEC_REGISTER) {
		rc = tee_get_ops(dev)->shm_register(dev, shm);
		if (rc)
			goto err;
	}

	if (flags & TEE_SHM_REGISTER) {
		struct tee_uclass_priv *priv = dev_get_uclass_priv(dev);

		list_add(&shm->link, &priv->list_shm);
	}

	*shmp = shm;

	return 0;
err:
	free(shm);
	if (flags & TEE_SHM_ALLOC)
		free(p);

	return rc;
}

int tee_shm_alloc(struct udevice *dev, ulong size, u32 flags,
		  struct tee_shm **shmp)
{
	u32 f = flags;

	f |= TEE_SHM_SEC_REGISTER | TEE_SHM_REGISTER | TEE_SHM_ALLOC;

	return __tee_shm_add(dev, 0, NULL, size, f, shmp);
}

int tee_shm_register(struct udevice *dev, void *addr, ulong size, u32 flags,
		     struct tee_shm **shmp)
{
	u32 f = flags & ~TEE_SHM_ALLOC;

	f |= TEE_SHM_SEC_REGISTER | TEE_SHM_REGISTER;

	return __tee_shm_add(dev, 0, addr, size, f, shmp);
}

void tee_shm_free(struct tee_shm *shm)
{
	struct tee_version_data v;

	if (!shm)
		return;

	tee_get_ops(shm->dev)->get_version(shm->dev, &v);

	if (shm->flags & TEE_SHM_ALLOC && !(v.gen_caps & TEE_GEN_CAP_REG_MEM)) {
		free_tee_shmem_from_pool(shm);
		return;
	}

	if (shm->flags & TEE_SHM_SEC_REGISTER)
		tee_get_ops(shm->dev)->shm_unregister(shm->dev, shm);

	if (shm->flags & TEE_SHM_REGISTER)
		list_del(&shm->link);

	if (shm->flags & TEE_SHM_ALLOC)
		free(shm->addr);

	free(shm);
}

bool tee_shm_is_registered(struct tee_shm *shm, struct udevice *dev)
{
	struct tee_uclass_priv *priv = dev_get_uclass_priv(dev);
	struct tee_shm *s;

	list_for_each_entry(s, &priv->list_shm, link)
		if (s == shm)
			return true;

	return false;
}

struct udevice *tee_find_device(struct udevice *start,
				int (*match)(struct tee_version_data *vers,
					     const void *data),
				const void *data,
				struct tee_version_data *vers)
{
	struct udevice *dev = start;
	struct tee_version_data lv;
	struct tee_version_data *v = vers ? vers : &lv;

	if (!dev)
		uclass_find_first_device(UCLASS_TEE, &dev);
	else
		uclass_find_next_device(&dev);

	for (; dev; uclass_find_next_device(&dev)) {
		if (device_probe(dev))
			continue;
		tee_get_ops(dev)->get_version(dev, v);
		if (!match || match(v, data))
			return dev;
	}

	return NULL;
}

static int tee_pre_probe(struct udevice *dev)
{
	struct tee_uclass_priv *priv = dev_get_uclass_priv(dev);

	INIT_LIST_HEAD(&priv->list_shm);

	return 0;
}

static int tee_pre_remove(struct udevice *dev)
{
	struct tee_uclass_priv *priv = dev_get_uclass_priv(dev);
	struct tee_shm *shm;

	/*
	 * Any remaining shared memory must be unregistered now as U-Boot
	 * is about to hand over to the next stage and that memory will be
	 * reused.
	 */
	while (!list_empty(&priv->list_shm)) {
		shm = list_first_entry(&priv->list_shm, struct tee_shm, link);
		debug("%s: freeing leftover shm %p (size %lu, flags %#x)\n",
		      __func__, (void *)shm, shm->size, shm->flags);
		tee_shm_free(shm);
	}

	return 0;
}

UCLASS_DRIVER(tee) = {
	.id = UCLASS_TEE,
	.name = "tee",
	.per_device_auto_alloc_size = sizeof(struct tee_uclass_priv),
	.pre_probe = tee_pre_probe,
	.pre_remove = tee_pre_remove,
};

void tee_optee_ta_uuid_from_octets(struct tee_optee_ta_uuid *d,
				   const u8 s[TEE_UUID_LEN])
{
	d->time_low = ((u32)s[0] << 24) | ((u32)s[1] << 16) |
		      ((u32)s[2] << 8) | s[3],
	d->time_mid = ((u32)s[4] << 8) | s[5];
	d->time_hi_and_version = ((u32)s[6] << 8) | s[7];
	memcpy(d->clock_seq_and_node, s + 8, sizeof(d->clock_seq_and_node));
}

void tee_optee_ta_uuid_to_octets(u8 d[TEE_UUID_LEN],
				 const struct tee_optee_ta_uuid *s)
{
	d[0] = s->time_low >> 24;
	d[1] = s->time_low >> 16;
	d[2] = s->time_low >> 8;
	d[3] = s->time_low;
	d[4] = s->time_mid >> 8;
	d[5] = s->time_mid;
	d[6] = s->time_hi_and_version >> 8;
	d[7] = s->time_hi_and_version;
	memcpy(d + 8, s->clock_seq_and_node, sizeof(s->clock_seq_and_node));
}
