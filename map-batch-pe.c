// Library-facing paired-end batch mapping, added for embedding minibwa as a
// static library. mb_map_batch() (map-algo.c) already pairs mates when
// MB_F_PE is set, but only against the *predefined* insert-size stats in
// mb_opt_t (opt->pe_avg/pe_std/pe_lo/pe_hi) -- it never calls mb_pestat() to
// estimate real insert-size stats from the batch's own data. That
// data-driven estimation only happens in the CLI's map-main.c
// worker_pipeline (kt_for(worker_for_pe, ...) preceded by a mb_pestat() call
// once >= 20 pairs have been seen), which is not part of libminibwa.a.
//
// mb_map_batch_pe() gets the per-read hits from mb_map_batch() with pairing
// disabled, then runs the same mb_pestat()-then-mb_pair() sequence
// map-main.c uses, mirroring its heuristic: use the predefined stats until
// at least 20 pairs are available, then estimate. Reads must be interleaved
// as consecutive mate pairs (mate0, mate1, mate0, mate1, ...), matching
// mb_map_batch's own convention for MB_F_PE.
#include "mbpriv.h"
#include "kalloc.h"

mb_hit_t **mb_map_batch_pe(const mb_opt_t *opt, const mb_idx_t *idx, int32_t n_seq,
							const int32_t *qlen, const char **seq, int32_t *n_hit,
							mb_tbuf_t *b0, const char **qname)
{
	mb_tbuf_t *b;
	mb_hit_t **hit;
	mb_opt_t opt_se;
	void *km;
	int32_t is_pe = !!(opt->flag & MB_F_PE);
	int32_t n_frag, i;
	int32_t *seg_off, *seg_cnt;
	mb_pestat_t pes[4];

	if (!is_pe || n_seq < 2)
		return mb_map_batch(opt, idx, n_seq, qlen, seq, n_hit, b0, qname);

	b = b0? b0 : mb_tbuf_init(0);
	km = mb_tbuf_km(b);

	opt_se = *opt;
	opt_se.flag &= ~MB_F_PE; // get unpaired per-read hits; we pair below with estimated stats
	hit = mb_map_batch(&opt_se, idx, n_seq, qlen, seq, n_hit, b, qname);
	if (hit == 0) {
		if (b0 == 0) mb_tbuf_destroy(b);
		return hit;
	}

	n_frag = n_seq / 2;
	for (i = 0; i < 4; ++i) pes[i].failed = 1;
	pes[1].failed = 0;
	pes[1].avg = opt->pe_avg, pes[1].std = opt->pe_std;
	pes[1].lo = opt->pe_lo, pes[1].hi = opt->pe_hi;

	if (!(opt->flag & MB_F_PE_PREDEF) && n_frag >= 20) {
		seg_off = Kmalloc(km, int32_t, n_frag);
		seg_cnt = Kmalloc(km, int32_t, n_frag);
		for (i = 0; i < n_frag; ++i) seg_off[i] = 2 * i, seg_cnt[i] = 2;
		mb_pestat(km, opt, n_frag, seg_off, seg_cnt, n_hit, hit, pes);
		mb_kfree(km, seg_cnt);
		mb_kfree(km, seg_off);
	}

	for (i = 0; i + 1 < n_seq; i += 2) {
		int32_t len2[2] = { qlen[i], qlen[i + 1] };
		char *seq2[2] = { (char*)seq[i], (char*)seq[i + 1] };
		mb_pair(km, opt, idx->l2b, &n_hit[i], &hit[i], pes, len2, seq2);
	}

	mb_tbuf_reset(b, opt->cap_kalloc);
	if (b0 == 0) mb_tbuf_destroy(b);
	return hit;
}
