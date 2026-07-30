// Library-facing index-build entry point, added for embedding minibwa as a
// static library in host processes that must not crash on bad input. Mirrors
// the non-GPL, non-low-memory branch of main_index() in index.c (the `l`
// and `-b` options are the GPL bwtgen.c path and are intentionally not
// reachable from here), but reports failures via a return code instead of
// kom_assert()/kom_panic(), which calls abort().
#include <stdlib.h>
#include <string.h>
#include "mbpriv.h"

int mb_idx_build(const char *fasta, const char *prefix, int sa_bit, int n_thread, int32_t is_meth, uint64_t seed)
{
	l2b_t *l2b;
	mb_bwt_t *bwt;
	char *fn_l2b, *fn_bwt, *fn_meth_bwt = 0;
	size_t prefix_len = strlen(prefix);

	l2b = l2b_import(fasta, seed);
	if (l2b == 0) return -1;

	fn_l2b = kom_calloc(char, prefix_len + 10);
	strcat(strcpy(fn_l2b, prefix), ".l2b");
	fn_bwt = kom_calloc(char, prefix_len + 10);
	strcat(strcpy(fn_bwt, prefix), ".mbw");
	if (is_meth) {
		fn_meth_bwt = kom_calloc(char, prefix_len + 15);
		strcat(strcpy(fn_meth_bwt, prefix), ".meth.mbw");
	}

	l2b_save(fn_l2b, l2b);
	bwt = mb_bwt_libsais(l2b, sa_bit, 1, 0, n_thread);
	mb_bwt_save(fn_bwt, bwt);
	mb_bwt_destroy(bwt);
	if (is_meth) {
		bwt = mb_bwt_libsais(l2b, sa_bit, 1, 1, n_thread);
		mb_bwt_save(fn_meth_bwt, bwt);
		mb_bwt_destroy(bwt);
	}
	l2b_destroy(l2b);

	free(fn_meth_bwt); free(fn_bwt); free(fn_l2b);
	return 0;
}
