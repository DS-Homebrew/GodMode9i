
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
//#include "utils.h"
#include "sector0.h"
//#include "../term256/term256ext.h"

// return 0 for valid NCSD header
int parse_ncsd(const uint8_t sector0[SECTOR_SIZE], int verbose) {
	const ncsd_header_t * h = (ncsd_header_t *)sector0;
	if (h->magic == 0x4453434e) {
		if (verbose) {
			//prt("NCSD magic found\n");
		}
	} else {
		if (verbose) {
			//prt("NCSD magic not found\n");
		}
		return -1;
	}
	if (verbose) {
		//iprtf("size: %" PRIu32 " sectors, %s MB\n", h->size, to_mebi(h->size * SECTOR_SIZE));
		//iprtf("media ID: %08" PRIx32 "%08" PRIx32 "\n", h->media_id_h, h->media_id_l);
	}

	for (unsigned i = 0; i < NCSD_PARTITIONS; ++i) {
		unsigned fs_type = h->fs_types[i];
		if (fs_type == 0) {
			break;
		}
		const char *s_fs_type;
		switch (fs_type) {
			case 1:
				s_fs_type = "Normal";
				break;
			case 3:
				s_fs_type = "FIRM";
				break;
			case 4:
				s_fs_type = "AGB_FIRM save";
				break;
			default:
				if (verbose) {
					//iprtf("invalid partition type %d\n", fs_type);
				}
				return -2;
		}
		if (verbose) {
			// yes I use MB for "MiB", bite me
			//iprtf("partition %u, %s, crypt: %" PRIu8 ", offset: 0x%08" PRIx32 ", length: 0x%08" PRIx32 "(%s MB)\n",
				//i, s_fs_type, h->crypt_types[i],
				//h->partitions[i].offset, h->partitions[i].length, to_mebi(h->partitions[i].length * SECTOR_SIZE));
		}
	}
	return 0;
}

const mbr_partition_t ptable_DSi[MBR_PARTITIONS] = {
	{0, {3, 24, 4}, 6, {15, 224, 59}, 0x00000877, 0x00066f89},
	{0, {2, 206, 60}, 6, {15, 224, 190}, 0x0006784d, 0x000105b3},
	{0, {2, 222, 191}, 1, {15, 224, 191}, 0x00077e5d, 0x000001a3},
	{0, {0, 0, 0}, 0, {0, 0, 0}, 0, 0}
};

const mbr_partition_t ptable_3DS[MBR_PARTITIONS] = {
	{0, {4, 24, 0}, 6, {1, 160, 63}, 0x00000097, 0x00047da9},
	{0, {4, 142, 64}, 6, {1, 160, 195}, 0x0004808d, 0x000105b3},
	{0, {0, 0, 0}, 0, {0, 0, 0}, 0, 0},
	{0, {0, 0, 0}, 0, {0, 0, 0}, 0, 0}
};

// return 0 for valid MBR
int parse_mbr(const uint8_t sector0[SECTOR_SIZE], int is3DS, int verbose) {
	const mbr_t *m = (mbr_t*)sector0;
	const mbr_partition_t *ref_ptable; // reference partition table
	int ret = 0;
	if (m->boot_signature_0 != 0x55 || m->boot_signature_1 != 0xaa) {
		//prt("invalid boot signature(0x55, 0xaa)\n");
		ret = -1;
	}
	if (!is3DS) {
		for (unsigned i = 0; i < sizeof(m->bootstrap); ++i) {
			if (m->bootstrap[i]) {
				//prt("bootstrap on DSi should be all zero\n");
				ret = 0;
				break;
			}
		}
		ref_ptable = ptable_DSi;
	} else {
		ref_ptable = ptable_3DS;
	}
	// only test the 1st partition now, we've seen variations on the 3rd partition
	// and after all we only care about the 1st partition
	if (memcmp(ref_ptable, m->partitions, sizeof(mbr_partition_t))) {
		//prt("invalid partition table\n");
		ret = -2;
	}
	if (!verbose) {
		return ret;
	}
	for (unsigned i = 0; i < MBR_PARTITIONS; ++i) {
		const mbr_partition_t *rp = &ref_ptable[i]; // reference
		const mbr_partition_t *p = &m->partitions[i];
		if (p->status != rp->status) {
			//iprtf("invalid partition %u status: %02" PRIx8 ", should be %02" PRIx8 "\n",
			//	i, p->status, rp->status);
		}
		if (p->type != rp->type) {
			//iprtf("invalid partition %u type: %02" PRIx8 ", should be %02" PRIx8 "\n",
			//	i, p->type, rp->type);
		}
		if (memcmp(&p->chs_first, &rp->chs_first, sizeof(chs_t))) {
			//iprtf("invalid partition %u first C/H/S: %u/%u/%u, should be %u/%u/%u\n",
			//	i, p->chs_first.cylinder, p->chs_first.head, p->chs_first.sector,
			//	rp->chs_first.cylinder, rp->chs_first.head, rp->chs_first.sector);
		}
		if (memcmp(&p->chs_last, &rp->chs_last, sizeof(chs_t))) {
			//iprtf("invalid partition %u last C/H/S: %u/%u/%u, should be %u/%u/%u\n",
			//	i, p->chs_last.cylinder, p->chs_last.head, p->chs_last.sector,
			//	rp->chs_last.cylinder, rp->chs_last.head, rp->chs_last.sector);
		}
		if (p->offset != rp->offset) {
			//iprtf("invalid partition %u LBA offset: 0x%08" PRIx32 ", should be 0x%08" PRIx32 "\n",
			//	i, p->offset, rp->offset);
		}
		if (p->length != rp->length) {
			//iprtf("invalid partition %u LBA length: 0x%08" PRIx32 ", should be 0x%08" PRIx32 "\n",
			//	i, p->length, rp->length);
		}
		//iprtf("status: %02x, type: %02" PRIx8 ", offset: 0x%08" PRIx32 ", length: 0x%08" PRIx32 "(%s MB)\n"
		//	"\t C/H/S: %u/%u/%u - %u/%u/%u\n",
		//	p->status, p->type, p->offset, p->length, to_mebi(p->length * SECTOR_SIZE),
		//	p->chs_first.cylinder, p->chs_first.head, p->chs_first.sector,
		//	p->chs_last.cylinder, p->chs_last.head, p->chs_last.sector);
	}
	return ret;
}
