/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2014 Jorgen Lundman <lundman@lundman.net>
 */

#include <sys/spa.h>
#include <sys/zio.h>
#include <sys/zio_compress.h>
#include <sys/zfs_context.h>
#include <sys/arc.h>
#include <sys/refcount.h>
#include <sys/vdev.h>
#include <sys/vdev_impl.h>
#include <sys/dsl_pool.h>
#ifdef _KERNEL
#include <sys/vmsystm.h>
#include <vm/anon.h>
#include <sys/fs/swapnode.h>
#include <sys/dnlc.h>
#endif
#include <sys/callb.h>
#include <sys/kstat.h>
#include <sys/kstat_osx.h>
#include <sys/zfs_ioctl.h>


/*
 * In Solaris the tunable are set via /etc/system. Until we have a load
 * time configuration, we add them to writable kstat tunables.
 *
 * This table is more or less populated from IllumOS mdb zfs_params sources
 * https://github.com/illumos/illumos-gate/blob/master/usr/src/cmd/mdb/common/modules/zfs/zfs.c#L336-L392
 *
 */




osx_kstat_t osx_kstat = {
	{ "active_vnodes",				KSTAT_DATA_UINT64 },
	{ "reclaim_nodes",				KSTAT_DATA_UINT64 },
	{ "pageout_nodes",				KSTAT_DATA_UINT64 },
	{ "vnop_debug",					KSTAT_DATA_UINT64 },
	{ "ignore_negatives",			KSTAT_DATA_UINT64 },
	{ "ignore_positives",			KSTAT_DATA_UINT64 },
	{ "create_negatives",			KSTAT_DATA_UINT64 },
	{ "reclaim_throttle",			KSTAT_DATA_UINT64 },
	{ "force_formd_normalized",		KSTAT_DATA_UINT64 },

	{ "zfs_arc_max",				KSTAT_DATA_UINT64 },
	{ "zfs_arc_min",				KSTAT_DATA_UINT64 },
	{ "zfs_arc_meta_limit",			KSTAT_DATA_UINT64 },
	{ "zfs_arc_grow_retry",			KSTAT_DATA_UINT64 },
	{ "zfs_arc_shrink_shift",		KSTAT_DATA_UINT64 },
	{ "zfs_arc_p_min_shift",		KSTAT_DATA_UINT64 },
	{ "zfs_disable_dup_eviction",	KSTAT_DATA_UINT64 },
	{ "zfs_arc_average_blocksize",	KSTAT_DATA_UINT64 },

	{ "max_active",					KSTAT_DATA_UINT64 },
	{ "sync_read_min_active",		KSTAT_DATA_UINT64 },
	{ "sync_read_max_active",		KSTAT_DATA_UINT64 },
	{ "sync_write_min_active",		KSTAT_DATA_UINT64 },
	{ "sync_write_max_active",		KSTAT_DATA_UINT64 },
	{ "async_read_min_active",		KSTAT_DATA_UINT64 },
	{ "async_read_max_active",		KSTAT_DATA_UINT64 },
	{ "async_write_min_active",		KSTAT_DATA_UINT64 },
	{ "async_write_max_active",		KSTAT_DATA_UINT64 },
	{ "scrub_min_active",			KSTAT_DATA_UINT64 },
	{ "scrub_max_active",			KSTAT_DATA_UINT64 },
	{ "async_write_min_dirty_pct",	KSTAT_DATA_INT64  },
	{ "async_write_max_dirty_pct",	KSTAT_DATA_INT64  },
	{ "aggregation_limit",			KSTAT_DATA_INT64  },
	{ "read_gap_limit",				KSTAT_DATA_INT64  },
	{ "write_gap_limit",			KSTAT_DATA_INT64  },

	{"arc_reduce_dnlc_percent",		KSTAT_DATA_INT64  },
	{"arc_lotsfree_percent",		KSTAT_DATA_INT64  },
	{"zfs_dirty_data_max",			KSTAT_DATA_INT64  },
	{"zfs_dirty_data_sync",			KSTAT_DATA_INT64  },
	{"zfs_delay_max_ns",			KSTAT_DATA_INT64  },
	{"zfs_delay_min_dirty_percent",	KSTAT_DATA_INT64  },
	{"zfs_delay_scale",				KSTAT_DATA_INT64  },
	{"spa_asize_inflation",			KSTAT_DATA_INT64  },
	{"zfs_mdcomp_disable",			KSTAT_DATA_INT64  },
	{"zfs_prefetch_disable",		KSTAT_DATA_INT64  },
	{"zfetch_max_streams",			KSTAT_DATA_INT64  },
	{"zfetch_min_sec_reap",			KSTAT_DATA_INT64  },
	{"zfetch_block_cap",			KSTAT_DATA_INT64  },
	{"zfetch_array_rd_sz",			KSTAT_DATA_INT64  },
	{"zfs_default_bs",				KSTAT_DATA_INT64  },
	{"zfs_default_ibs",				KSTAT_DATA_INT64  },
	{"metaslab_aliquot",			KSTAT_DATA_INT64  },
	{"reference_tracking_enable",	KSTAT_DATA_INT64  },
	{"reference_history",			KSTAT_DATA_INT64  },
	{"spa_max_replication_override",KSTAT_DATA_INT64  },
	{"spa_mode_global",				KSTAT_DATA_INT64  },
	{"zfs_flags",					KSTAT_DATA_INT64  },
	{"zfs_txg_timeout",				KSTAT_DATA_INT64  },
	{"zfs_vdev_cache_max",			KSTAT_DATA_INT64  },
	{"zfs_vdev_cache_size",			KSTAT_DATA_INT64  },
	{"zfs_vdev_cache_bshift",		KSTAT_DATA_INT64  },
	{"vdev_mirror_shift",			KSTAT_DATA_INT64  },
	{"zfs_scrub_limit",				KSTAT_DATA_INT64  },
	{"zfs_no_scrub_io",				KSTAT_DATA_INT64  },
	{"zfs_no_scrub_prefetch",		KSTAT_DATA_INT64  },
	{"fzap_default_block_shift",	KSTAT_DATA_INT64  },
	{"zfs_immediate_write_sz",		KSTAT_DATA_INT64  },
	{"zfs_read_chunk_size",			KSTAT_DATA_INT64  },
	{"zfs_nocacheflush",			KSTAT_DATA_INT64  },
	{"zil_replay_disable",			KSTAT_DATA_INT64  },
	{"metaslab_gang_bang",			KSTAT_DATA_INT64  },
	{"metaslab_df_alloc_threshold",	KSTAT_DATA_INT64  },
	{"metaslab_df_free_pct",		KSTAT_DATA_INT64  },
	{"zio_injection_enabled",		KSTAT_DATA_INT64  },
	{"zvol_immediate_write_sz",		KSTAT_DATA_INT64  },
};




static kstat_t		*osx_kstat_ksp;


static int osx_kstat_update(kstat_t *ksp, int rw)
{
	osx_kstat_t *ks = ksp->ks_data;

	if (rw == KSTAT_WRITE) {

		/* Darwin */

		debug_vnop_osx_printf = ks->darwin_debug.value.ui64;
		zfs_vnop_ignore_negatives = ks->darwin_ignore_negatives.value.ui64;
		zfs_vnop_ignore_positives = ks->darwin_ignore_positives.value.ui64;
		zfs_vnop_create_negatives = ks->darwin_create_negatives.value.ui64;
		zfs_vnop_reclaim_throttle = ks->darwin_reclaim_throttle.value.ui64;
		zfs_vnop_force_formd_normalized_output = ks->darwin_force_formd_normalized.value.ui64;

		/* ARC */
		arc_kstat_update(ksp, rw);


		/* vdev_queue */

		zfs_vdev_max_active =
			ks->zfs_vdev_max_active.value.ui64;
		zfs_vdev_sync_read_min_active =
			ks->zfs_vdev_sync_read_min_active.value.ui64;
		zfs_vdev_sync_read_max_active =
			ks->zfs_vdev_sync_read_max_active.value.ui64;
		zfs_vdev_sync_write_min_active =
			ks->zfs_vdev_sync_write_min_active.value.ui64;
		zfs_vdev_sync_write_max_active =
			ks->zfs_vdev_sync_write_max_active.value.ui64;
		zfs_vdev_async_read_min_active =
			ks->zfs_vdev_async_read_min_active.value.ui64;
		zfs_vdev_async_read_max_active =
			ks->zfs_vdev_async_read_max_active.value.ui64;
		zfs_vdev_async_write_min_active =
			ks->zfs_vdev_async_write_min_active.value.ui64;
		zfs_vdev_async_write_max_active =
			ks->zfs_vdev_async_write_max_active.value.ui64;
		zfs_vdev_scrub_min_active =
			ks->zfs_vdev_scrub_min_active.value.ui64;
		zfs_vdev_scrub_max_active =
			ks->zfs_vdev_scrub_max_active.value.ui64;
		zfs_vdev_async_write_active_min_dirty_percent =
			ks->zfs_vdev_async_write_active_min_dirty_percent.value.i64;
		zfs_vdev_async_write_active_max_dirty_percent =
			ks->zfs_vdev_async_write_active_max_dirty_percent.value.i64;
		zfs_vdev_aggregation_limit =
			ks->zfs_vdev_aggregation_limit.value.i64;
		zfs_vdev_read_gap_limit =
			ks->zfs_vdev_read_gap_limit.value.i64;
		zfs_vdev_write_gap_limit =
			ks->zfs_vdev_write_gap_limit.value.i64;

		zfs_dirty_data_max =
			ks->zfs_dirty_data_max.value.i64;
		zfs_dirty_data_sync =
			ks->zfs_dirty_data_sync.value.i64;

	} else {

		/* kstat READ */


		/* Darwin */
		ks->darwin_active_vnodes.value.ui64          = vnop_num_vnodes;
		ks->darwin_reclaim_nodes.value.ui64          = vnop_num_reclaims;
		ks->darwin_pageout_nodes.value.ui64          = vnop_num_pageout;
		ks->darwin_debug.value.ui64                  = debug_vnop_osx_printf;
		ks->darwin_ignore_negatives.value.ui64       = zfs_vnop_ignore_negatives;
		ks->darwin_ignore_positives.value.ui64       = zfs_vnop_ignore_positives;
		ks->darwin_create_negatives.value.ui64       = zfs_vnop_create_negatives;
		ks->darwin_reclaim_throttle.value.ui64       = zfs_vnop_reclaim_throttle;
		ks->darwin_force_formd_normalized.value.ui64 = zfs_vnop_force_formd_normalized_output;

		/* ARC */
		arc_kstat_update(ksp, rw);

		/* vdev_queue */
		ks->zfs_vdev_max_active.value.ui64 =
			zfs_vdev_max_active ;
		ks->zfs_vdev_sync_read_min_active.value.ui64 =
			zfs_vdev_sync_read_min_active ;
		ks->zfs_vdev_sync_read_max_active.value.ui64 =
			zfs_vdev_sync_read_max_active ;
		ks->zfs_vdev_sync_write_min_active.value.ui64 =
			zfs_vdev_sync_write_min_active ;
		ks->zfs_vdev_sync_write_max_active.value.ui64 =
			zfs_vdev_sync_write_max_active ;
		ks->zfs_vdev_async_read_min_active.value.ui64 =
			zfs_vdev_async_read_min_active ;
		ks->zfs_vdev_async_read_max_active.value.ui64 =
			zfs_vdev_async_read_max_active ;
		ks->zfs_vdev_async_write_min_active.value.ui64 =
			zfs_vdev_async_write_min_active ;
		ks->zfs_vdev_async_write_max_active.value.ui64 =
			zfs_vdev_async_write_max_active ;
		ks->zfs_vdev_scrub_min_active.value.ui64 =
			zfs_vdev_scrub_min_active ;
		ks->zfs_vdev_scrub_max_active.value.ui64 =
			zfs_vdev_scrub_max_active ;
		ks->zfs_vdev_async_write_active_min_dirty_percent.value.i64 =
			zfs_vdev_async_write_active_min_dirty_percent ;
		ks->zfs_vdev_async_write_active_max_dirty_percent.value.i64 =
			zfs_vdev_async_write_active_max_dirty_percent ;
		ks->zfs_vdev_aggregation_limit.value.i64 =
			zfs_vdev_aggregation_limit ;
		ks->zfs_vdev_read_gap_limit.value.i64 =
			zfs_vdev_read_gap_limit ;
		ks->zfs_vdev_write_gap_limit.value.i64 =
			zfs_vdev_write_gap_limit;

		ks->zfs_dirty_data_max.value.i64 =
			zfs_dirty_data_max;
		ks->zfs_dirty_data_sync.value.i64 =
			zfs_dirty_data_sync;

	}
	return 0;
}



int kstat_osx_init(void)
{
	osx_kstat_ksp = kstat_create("zfs", 0, "tunable", "darwin",
	    KSTAT_TYPE_NAMED, sizeof (osx_kstat) / sizeof (kstat_named_t),
	    KSTAT_FLAG_VIRTUAL|KSTAT_FLAG_WRITABLE);

	if (osx_kstat_ksp != NULL) {
		osx_kstat_ksp->ks_data = &osx_kstat;
        osx_kstat_ksp->ks_update = osx_kstat_update;
		kstat_install(osx_kstat_ksp);
	}

	return 0;
}

void kstat_osx_fini(void)
{
    if (osx_kstat_ksp != NULL) {
        kstat_delete(osx_kstat_ksp);
        osx_kstat_ksp = NULL;
    }
}