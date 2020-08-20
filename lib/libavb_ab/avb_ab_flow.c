/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <avb_verify.h>

#include "avb_ab/avb_ab_flow.h"

bool avb_ab_data_verify_and_byteswap(const AvbABData* src, AvbABData* dest) {
	/* Ensure magic is correct. */
	if (avb_safe_memcmp(src->magic, AVB_AB_MAGIC, AVB_AB_MAGIC_LEN) != 0) {
		avb_error("Magic is incorrect.\n");
		return false;
	}

	avb_memcpy(dest, src, sizeof(AvbABData));
	dest->crc32 = avb_be32toh(dest->crc32);

	/* Ensure we don't attempt to access any fields if the major version
	 * is not supported.
	 */
	if (dest->version_major > AVB_AB_MAJOR_VERSION) {
		avb_error("No support for given major version.\n");
		return false;
	}

	/* Bail if CRC32 doesn't match. */
	if (dest->crc32 !=
			avb_crc32((const uint8_t*)dest, sizeof(AvbABData) - sizeof(uint32_t))) {
		avb_error("CRC32 does not match.\n");
		return false;
	}

	return true;
}

void avb_ab_data_update_crc_and_byteswap(const AvbABData* src,
							AvbABData* dest) {
	avb_memcpy(dest, src, sizeof(AvbABData));
	dest->crc32 = avb_htobe32(
		avb_crc32((const uint8_t*)dest, sizeof(AvbABData) - sizeof(uint32_t)));
}

void avb_ab_data_init(AvbABData* data) {
	avb_memset(data, '\0', sizeof(AvbABData));
	avb_memcpy(data->magic, AVB_AB_MAGIC, AVB_AB_MAGIC_LEN);
	data->version_major = AVB_AB_MAJOR_VERSION;
	data->version_minor = AVB_AB_MINOR_VERSION;
	data->slots[0].priority = AVB_AB_MAX_PRIORITY;
	data->slots[0].tries_remaining = AVB_AB_MAX_TRIES_REMAINING;
	data->slots[0].successful_boot = 0;
	data->slots[1].priority = AVB_AB_MAX_PRIORITY - 1;
	data->slots[1].tries_remaining = AVB_AB_MAX_TRIES_REMAINING;
	data->slots[1].successful_boot = 0;
}

/* The AvbABData struct is stored 2048 bytes into the 'misc' partition
 * following the 'struct bootloader_message' field. The struct is
 * compatible with the guidelines in bootable/recovery/bootloader.h -
 * e.g. it is stored in the |slot_suffix| field, starts with a
 * NUL-byte, and is 32 bytes long.
 */
#define AB_METADATA_MISC_PARTITION_OFFSET 2048

AvbIOResult avb_ab_data_read(AvbABOps* ab_ops, AvbABData* data) {
	AvbOps* ops = ab_ops->ops;
	AvbABData serialized;
	AvbIOResult io_ret;
	size_t num_bytes_read;

	io_ret = ops->read_from_partition(ops,
					"misc",
					AB_METADATA_MISC_PARTITION_OFFSET,
					sizeof(AvbABData),
					&serialized,
					&num_bytes_read);
	if (io_ret == AVB_IO_RESULT_ERROR_OOM) {
		return AVB_IO_RESULT_ERROR_OOM;
	} else if (io_ret != AVB_IO_RESULT_OK ||
						 num_bytes_read != sizeof(AvbABData)) {
		avb_error("Error reading A/B metadata.\n");
		return AVB_IO_RESULT_ERROR_IO;
	}

	if (!avb_ab_data_verify_and_byteswap(&serialized, data)) {
		avb_error(
				"Error validating A/B metadata from disk. "
				"Resetting and writing new A/B metadata to disk.\n");
		avb_ab_data_init(data);
		return avb_ab_data_write(ab_ops, data);
	}

	return AVB_IO_RESULT_OK;
}

AvbIOResult avb_ab_data_write(AvbABOps* ab_ops, const AvbABData* data) {
	AvbOps* ops = ab_ops->ops;
	AvbABData serialized;
	AvbIOResult io_ret;

	avb_ab_data_update_crc_and_byteswap(data, &serialized);
	io_ret = ops->write_to_partition(ops,
				"misc",
				AB_METADATA_MISC_PARTITION_OFFSET,
				sizeof(AvbABData),
				&serialized);
	if (io_ret == AVB_IO_RESULT_ERROR_OOM) {
		return AVB_IO_RESULT_ERROR_OOM;
	} else if (io_ret != AVB_IO_RESULT_OK) {
		avb_error("Error writing A/B metadata.\n");
		return AVB_IO_RESULT_ERROR_IO;
	}
	return AVB_IO_RESULT_OK;
}

bool slot_is_bootable(AvbABSlotData* slot) {
	return slot->priority > 0 &&
		(slot->successful_boot || (slot->tries_remaining > 0));
}

static void slot_set_unbootable(AvbABSlotData* slot) {
	slot->priority = 0;
	slot->tries_remaining = 0;
	slot->successful_boot = 0;
}

/* Ensure all unbootable and/or illegal states are marked as the
 * canonical 'unbootable' state, e.g. priority=0, tries_remaining=0,
 * and successful_boot=0.
 */
static void slot_normalize(AvbABSlotData* slot, uint8_t slot_idx) {
#if CONFIG_IS_ENABLED(ANDROID_VIRTUAL_AB_UPDATE)
	struct misc_virtual_ab_message virtual_ab_msg;
#endif

	if (slot->priority > 0) {
		if (slot->tries_remaining == 0 && !slot->successful_boot) {
			/* We've exhausted all tries -> unbootable. */
			slot_set_unbootable(slot);

#if CONFIG_IS_ENABLED(ANDROID_VIRTUAL_AB_UPDATE)
			/*
			 * For Virtual A/B we need to check probable slot switching
			 * in case of unsuccessful kernel boot from new slot after
			 * Virtual update. In such case we need to cancel virtual
			 * update and record this to misc
			 */
			if (!load_virtual_ab_msg(&virtual_ab_msg)) {
				printf("Failed to load /misc from MMC\n");
				return;
			}

			/* Check if we previously applied update */
			if ((virtual_ab_msg.merge_status == VIRTUAL_AB_SNAPSHOTED)
				&& (virtual_ab_msg.source_slot != slot_idx))
			{
				/*
				 * Okay, updated kernel on slot_idx failed, need to cancel
				 * update in misc_virtual_ab_message to inform Android.
				 */
				virtual_ab_msg.merge_status = VIRTUAL_AB_CANCELLED;

				if (save_virtual_ab_msg(&virtual_ab_msg))
					printf("Virtual update for slot %u cancelled\n", slot_idx);
				else
					printf("Failed to save Virual A/B message to /misc\n");
			}
#endif
		}
		if (slot->tries_remaining > 0 && slot->successful_boot) {
			/* Illegal state - avb_ab_mark_slot_successful() will clear
			 * tries_remaining when setting successful_boot.
			 */
			slot_set_unbootable(slot);
		}
	} else {
		slot_set_unbootable(slot);
	}
}

static const char* slot_suffixes[2] = {"_a", "_b"};

/* Helper function to load metadata - returns AVB_IO_RESULT_OK on
 * success, error code otherwise.
 */
static AvbIOResult load_metadata(AvbABOps* ab_ops,
						AvbABData* ab_data,
						AvbABData* ab_data_orig) {
	AvbIOResult io_ret;

	io_ret = ab_ops->read_ab_metadata(ab_ops, ab_data);
	if (io_ret != AVB_IO_RESULT_OK) {
		avb_error("I/O error while loading A/B metadata.\n");
		return io_ret;
	}
	*ab_data_orig = *ab_data;

	/* Ensure data is normalized, e.g. illegal states will be marked as
	 * unbootable and all unbootable states are represented with
	 * (priority=0, tries_remaining=0, successful_boot=0).
	 */
	slot_normalize(&ab_data->slots[0], 0);
	slot_normalize(&ab_data->slots[1], 1);
	return AVB_IO_RESULT_OK;
}

/* Writes A/B metadata to disk only if it has changed - returns
 * AVB_IO_RESULT_OK on success, error code otherwise.
 */
static AvbIOResult save_metadata_if_changed(AvbABOps* ab_ops,
										AvbABData* ab_data,
										AvbABData* ab_data_orig) {
	if (avb_safe_memcmp(ab_data, ab_data_orig, sizeof(AvbABData)) != 0) {
		avb_debug("Writing A/B metadata to disk.\n");
		return ab_ops->write_ab_metadata(ab_ops, ab_data);
	}
	return AVB_IO_RESULT_OK;
}

AvbABFlowResult avb_get_bootable_slot(AvbABData *ab_data, size_t *slot_index_to_boot)
{
	if ((!ab_data) ||(!slot_index_to_boot))
		return AVB_AB_FLOW_RESULT_ERROR_IO;

	if (slot_is_bootable(&ab_data->slots[0]) &&
		slot_is_bootable(&ab_data->slots[1])) {
	if (ab_data->slots[1].priority > ab_data->slots[0].priority) {
		*slot_index_to_boot = 1;
	} else {
		*slot_index_to_boot = 0;
	}
	} else if (slot_is_bootable(&ab_data->slots[0])) {
		*slot_index_to_boot = 0;
	} else if (slot_is_bootable(&ab_data->slots[1])) {
		*slot_index_to_boot = 1;
	} else {
		/* No bootable slots! */
		avb_error("No bootable slots found.\n");
		return AVB_AB_FLOW_RESULT_ERROR_NO_BOOTABLE_SLOTS;
	}
	return AVB_AB_FLOW_RESULT_OK;
}

AvbABFlowResult avb_ab_get_metadata(AvbABOps* ab_ops, AvbABData *ab_data)
{
	AvbABData ab_data_orig;
	AvbIOResult io_ret;

	if ((!ab_ops) || (!ab_data))
		return AVB_IO_RESULT_ERROR_OOM;

	io_ret = load_metadata(ab_ops, ab_data, &ab_data_orig);
	if (io_ret != AVB_IO_RESULT_OK)
		return io_ret;

	if (memcmp(ab_data, &ab_data_orig, sizeof(AvbABData)))
		avb_print("AB data was normalized to valid state\n");

	return io_ret;
}



AvbABFlowResult avb_ab_flow(AvbABOps* ab_ops,
							const char* const* requested_partitions,
							AvbSlotVerifyFlags flags,
							AvbHashtreeErrorMode hashtree_error_mode,
							AvbSlotVerifyData** out_data) {
	AvbOps* ops = ab_ops->ops;
	AvbSlotVerifyData* slot_data[2] = {NULL, NULL};
	AvbSlotVerifyData* data = NULL;
	AvbABFlowResult ret;
	AvbABData ab_data, ab_data_orig;
	size_t slot_index_to_boot, n;
	AvbIOResult io_ret;
	bool saw_and_allowed_verification_error = false;

	io_ret = load_metadata(ab_ops, &ab_data, &ab_data_orig);
	if (io_ret == AVB_IO_RESULT_ERROR_OOM) {
		ret = AVB_AB_FLOW_RESULT_ERROR_OOM;
		goto out;
	} else if (io_ret != AVB_IO_RESULT_OK) {
		ret = AVB_AB_FLOW_RESULT_ERROR_IO;
		goto out;
	}

	/* Validate all bootable slots. */
	for (n = 0; n < 2; n++) {
		if (slot_is_bootable(&ab_data.slots[n])) {
			AvbSlotVerifyResult verify_result;
			bool set_slot_unbootable = false;

			verify_result = avb_slot_verify(ops,
							requested_partitions,
							slot_suffixes[n],
							flags,
							hashtree_error_mode,
							&slot_data[n]);

			switch (verify_result) {
				case AVB_SLOT_VERIFY_RESULT_ERROR_OOM:
					ret = AVB_AB_FLOW_RESULT_ERROR_OOM;
					goto out;

				case AVB_SLOT_VERIFY_RESULT_ERROR_IO:
					ret = AVB_AB_FLOW_RESULT_ERROR_IO;
					goto out;

				case AVB_SLOT_VERIFY_RESULT_OK:
					break;

				case AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA:
				case AVB_SLOT_VERIFY_RESULT_ERROR_UNSUPPORTED_VERSION:
					/* Even with AVB_SLOT_VERIFY_FLAGS_ALLOW_VERIFICATION_ERROR
					 * these mean game over.
					 */
					set_slot_unbootable = true;
					break;

				/* explicit fallthrough. */
				case AVB_SLOT_VERIFY_RESULT_ERROR_VERIFICATION:
				case AVB_SLOT_VERIFY_RESULT_ERROR_ROLLBACK_INDEX:
				case AVB_SLOT_VERIFY_RESULT_ERROR_PUBLIC_KEY_REJECTED:
					if (flags & AVB_SLOT_VERIFY_FLAGS_ALLOW_VERIFICATION_ERROR) {
						/* Do nothing since we allow this. */
						avb_debugv("Allowing slot ",
								slot_suffixes[n],
								" which verified "
								"with result ",
								avb_slot_verify_result_to_string(verify_result),
								" because "
								"AVB_SLOT_VERIFY_FLAGS_ALLOW_VERIFICATION_ERROR "
								"is set.\n",
								NULL);
						saw_and_allowed_verification_error = true;
					} else {
						set_slot_unbootable = true;
					}
					break;

				case AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_ARGUMENT:
					ret = AVB_AB_FLOW_RESULT_ERROR_INVALID_ARGUMENT;
					goto out;
					/* Do not add a 'default:' case here because of -Wswitch. */
			}

			if (set_slot_unbootable) {
				avb_errorv("Error verifying slot ",
					slot_suffixes[n],
					" with result ",
					avb_slot_verify_result_to_string(verify_result),
					" - setting unbootable.\n",
					NULL);
				slot_set_unbootable(&ab_data.slots[n]);
			}
		}
	}
	ret = avb_get_bootable_slot(&ab_data, &slot_index_to_boot);
	if (ret != AVB_AB_FLOW_RESULT_OK) {
		goto out;
	}
	/* Update stored rollback index such that the stored rollback index
	 * is the largest value supporting all currently bootable slots. Do
	 * this for every rollback index location.
	 */
	for (n = 0; n < AVB_MAX_NUMBER_OF_ROLLBACK_INDEX_LOCATIONS; n++) {
		uint64_t rollback_index_value = 0;

		if (slot_data[0] != NULL && slot_data[1] != NULL) {
			uint64_t a_rollback_index = slot_data[0]->rollback_indexes[n];
			uint64_t b_rollback_index = slot_data[1]->rollback_indexes[n];
			rollback_index_value =
					(a_rollback_index < b_rollback_index ? a_rollback_index
												 : b_rollback_index);
		} else if (slot_data[0] != NULL) {
			rollback_index_value = slot_data[0]->rollback_indexes[n];
		} else if (slot_data[1] != NULL) {
			rollback_index_value = slot_data[1]->rollback_indexes[n];
		}

		if (rollback_index_value != 0) {
			uint64_t current_rollback_index_value;
			io_ret = ops->read_rollback_index(ops, n, &current_rollback_index_value);
			if (io_ret == AVB_IO_RESULT_ERROR_OOM) {
				ret = AVB_AB_FLOW_RESULT_ERROR_OOM;
				goto out;
			} else if (io_ret != AVB_IO_RESULT_OK) {
				avb_error("Error getting rollback index for slot.\n");
				ret = AVB_AB_FLOW_RESULT_ERROR_IO;
				goto out;
			}
			if (current_rollback_index_value != rollback_index_value) {
				io_ret = ops->write_rollback_index(ops, n, rollback_index_value);
				if (io_ret == AVB_IO_RESULT_ERROR_OOM) {
					ret = AVB_AB_FLOW_RESULT_ERROR_OOM;
					goto out;
				} else if (io_ret != AVB_IO_RESULT_OK) {
					avb_error("Error setting stored rollback index.\n");
					ret = AVB_AB_FLOW_RESULT_ERROR_IO;
					goto out;
				}
			}
		}
	}

	/* Finally, select this slot. */
	avb_assert(slot_data[slot_index_to_boot] != NULL);
	data = slot_data[slot_index_to_boot];
	slot_data[slot_index_to_boot] = NULL;
	if (saw_and_allowed_verification_error) {
		avb_assert(flags & AVB_SLOT_VERIFY_FLAGS_ALLOW_VERIFICATION_ERROR);
		ret = AVB_AB_FLOW_RESULT_OK_WITH_VERIFICATION_ERROR;
	} else {
		ret = AVB_AB_FLOW_RESULT_OK;
	}

	/* ... and decrement tries remaining, if applicable. */
	if (!ab_data.slots[slot_index_to_boot].successful_boot &&
			ab_data.slots[slot_index_to_boot].tries_remaining > 0) {
		ab_data.slots[slot_index_to_boot].tries_remaining -= 1;
	}

out:
	io_ret = save_metadata_if_changed(ab_ops, &ab_data, &ab_data_orig);
	if (io_ret != AVB_IO_RESULT_OK) {
		if (io_ret == AVB_IO_RESULT_ERROR_OOM) {
			ret = AVB_AB_FLOW_RESULT_ERROR_OOM;
		} else {
			ret = AVB_AB_FLOW_RESULT_ERROR_IO;
		}
		if (data != NULL) {
			avb_slot_verify_data_free(data);
			data = NULL;
		}
	}

	for (n = 0; n < 2; n++) {
		if (slot_data[n] != NULL) {
			avb_slot_verify_data_free(slot_data[n]);
		}
	}

	if (out_data != NULL) {
		*out_data = data;
	} else {
		if (data != NULL) {
			avb_slot_verify_data_free(data);
		}
	}

	return ret;
}

AvbIOResult avb_ab_mark_slot_active(AvbABOps* ab_ops,
					unsigned int slot_number) {

	AvbABData ab_data, ab_data_orig;
	unsigned int other_slot_number;
	AvbIOResult ret;

	avb_assert(slot_number < 2);

	ret = load_metadata(ab_ops, &ab_data, &ab_data_orig);
	if (ret != AVB_IO_RESULT_OK) {
		goto out;
	}

	/* Make requested slot top priority, unsuccessful, and with max tries. */
	ab_data.slots[slot_number].priority = AVB_AB_MAX_PRIORITY;
	ab_data.slots[slot_number].tries_remaining = AVB_AB_MAX_TRIES_REMAINING;
	ab_data.slots[slot_number].successful_boot = 0;

	/* Ensure other slot doesn't have as high a priority. */
	other_slot_number = 1 - slot_number;
	if (ab_data.slots[other_slot_number].priority == AVB_AB_MAX_PRIORITY) {
		ab_data.slots[other_slot_number].priority = AVB_AB_MAX_PRIORITY - 1;
	}

	ret = AVB_IO_RESULT_OK;

out:
	if (ret == AVB_IO_RESULT_OK) {
		ret = save_metadata_if_changed(ab_ops, &ab_data, &ab_data_orig);
	}
	return ret;
}

AvbIOResult avb_ab_mark_slot_unbootable(AvbABOps* ab_ops,
					unsigned int slot_number) {

	AvbABData ab_data, ab_data_orig;
	AvbIOResult ret;

	avb_assert(slot_number < 2);

	ret = load_metadata(ab_ops, &ab_data, &ab_data_orig);
	if (ret != AVB_IO_RESULT_OK) {
		goto out;
	}

	slot_set_unbootable(&ab_data.slots[slot_number]);

	ret = AVB_IO_RESULT_OK;

out:
	if (ret == AVB_IO_RESULT_OK) {
		ret = save_metadata_if_changed(ab_ops, &ab_data, &ab_data_orig);
	}
	return ret;
}

AvbIOResult avb_ab_mark_slot_successful(AvbABOps* ab_ops,
					unsigned int slot_number) {

	AvbABData ab_data, ab_data_orig;
	AvbIOResult ret;

	avb_assert(slot_number < 2);

	ret = load_metadata(ab_ops, &ab_data, &ab_data_orig);
	if (ret != AVB_IO_RESULT_OK) {
		goto out;
	}

	if (!slot_is_bootable(&ab_data.slots[slot_number])) {
		avb_error("Cannot mark unbootable slot as successful.\n");
		ret = AVB_IO_RESULT_OK;
		goto out;
	}

	ab_data.slots[slot_number].tries_remaining = 0;
	ab_data.slots[slot_number].successful_boot = 1;

	ret = AVB_IO_RESULT_OK;

out:
	if (ret == AVB_IO_RESULT_OK) {
		ret = save_metadata_if_changed(ab_ops, &ab_data, &ab_data_orig);
	}
	return ret;
}

const char* avb_ab_flow_result_to_string(AvbABFlowResult result) {
	const char* ret = NULL;

	switch (result) {
		case AVB_AB_FLOW_RESULT_OK:
			ret = "OK";
			break;

		case AVB_AB_FLOW_RESULT_OK_WITH_VERIFICATION_ERROR:
			ret = "OK_WITH_VERIFICATION_ERROR";
			break;

		case AVB_AB_FLOW_RESULT_ERROR_OOM:
			ret = "ERROR_OOM";
			break;

		case AVB_AB_FLOW_RESULT_ERROR_IO:
			ret = "ERROR_IO";
			break;

		case AVB_AB_FLOW_RESULT_ERROR_NO_BOOTABLE_SLOTS:
			ret = "ERROR_NO_BOOTABLE_SLOTS";
			break;

		case AVB_AB_FLOW_RESULT_ERROR_INVALID_ARGUMENT:
			ret = "ERROR_INVALID_ARGUMENT";
			break;
			/* Do not add a 'default:' case here because of -Wswitch. */
	}

	if (ret == NULL) {
		avb_error("Unknown AvbABFlowResult value.\n");
		ret = "(unknown)";
	}

	return ret;
}

#if CONFIG_IS_ENABLED(ANDROID_VIRTUAL_AB_UPDATE)
void init_virtual_ab_msg(struct misc_virtual_ab_message *ab_msg) {
	ab_msg->magic = MISC_VIRTUAL_AB_MAGIC_HEADER;
	ab_msg->version = MAX_VIRTUAL_AB_MESSAGE_VERSION;
	ab_msg->merge_status = VIRTUAL_AB_NONE;
	ab_msg->source_slot = avb_get_slot_index();
}

bool validate_virtual_ab_msg(struct misc_virtual_ab_message *ab_msg) {
	return ((ab_msg->magic == MISC_VIRTUAL_AB_MAGIC_HEADER) &&
			(ab_msg->version <= MAX_VIRTUAL_AB_MESSAGE_VERSION) &&
			(ab_msg->merge_status <= VIRTUAL_AB_CANCELLED) &&
			(ab_msg->source_slot < AVB_AB_MAX_SLOTS));
}

bool save_virtual_ab_msg(struct misc_virtual_ab_message *ab_msg) {
	int res = 0;

	if(!validate_virtual_ab_msg(ab_msg)){
		printf("Invalid Virtual A/B message passed for saving!");
		return false;
	}

	res = write_to_part(CONFIG_FASTBOOT_FLASH_MMC_DEV, "misc",
			SYSTEM_SPACE_OFFSET_IN_MISC, sizeof(*ab_msg),
			ab_msg);
	if (res)
		printf("Failed to write Virtual A/B message to /misc!");

	return !res;
}

bool load_virtual_ab_msg(struct misc_virtual_ab_message *ab_msg) {
	int ret;
	size_t bytes_read = 0;

	ret = read_from_part(CONFIG_FASTBOOT_FLASH_MMC_DEV, "misc",
			SYSTEM_SPACE_OFFSET_IN_MISC, sizeof(*ab_msg),
			ab_msg, &bytes_read);

	if (ret || (bytes_read != sizeof(*ab_msg)))
		return false;

	if(!validate_virtual_ab_msg(ab_msg)){
		printf("Invalid Virtual A/B message in /misc, re-init this...");
		init_virtual_ab_msg(ab_msg);
		return save_virtual_ab_msg(ab_msg);
	}

	return true;

}

bool virtual_ab_is_in_progress(void) {
	struct misc_virtual_ab_message ab_data;

	if (!load_virtual_ab_msg(&ab_data)) {
		printf("Failed to load /misc from MMC in %s\n", __func__);
		return false;
	}

	/*
	 * Merge is in progress or we are now load updated and
	 * snapshoted images. Userdata or metadata erase prohibited.
	 */
	return (ab_data.merge_status == VIRTUAL_AB_MERGING) ||
		((ab_data.merge_status == VIRTUAL_AB_SNAPSHOTED) &&
		(ab_data.source_slot != avb_get_slot_index()));
}
#endif
