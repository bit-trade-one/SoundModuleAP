#include "stdio.h"
#ifndef mips
#include "stdlib.h"
#endif
#include "xlisp.h"
#include "sound.h"

#include "falloc.h"
#include "cext.h"
#include "stkchorus.h"

void stkchorus_free();


typedef struct stkchorus_susp_struct {
    snd_susp_node susp;
    long terminate_cnt;
    boolean logically_stopped;
    sound_type s1;
    long s1_cnt;
    sample_block_values_type s1_ptr;

    struct stkEffect *mych;
} stkchorus_susp_node, *stkchorus_susp_type;


      #include "stkint.h"


void stkchorus_n_fetch(register stkchorus_susp_type susp, snd_list_type snd_list)
{
    int cnt = 0; /* how many samples computed */
    int togo;
    int n;
    sample_block_type out;
    register sample_block_values_type out_ptr;

    register sample_block_values_type out_ptr_reg;

    register struct stkEffect * mych_reg;
    register sample_block_values_type s1_ptr_reg;
    falloc_sample_block(out, "stkchorus_n_fetch");
    out_ptr = out->samples;
    snd_list->block = out;

    while (cnt < max_sample_block_len) { /* outer loop */
	/* first compute how many samples to generate in inner loop: */
	/* don't overflow the output sample block: */
	togo = max_sample_block_len - cnt;

	/* don't run past the s1 input sample block: */
	susp_check_term_log_samples(s1, s1_ptr, s1_cnt);
	togo = min(togo, susp->s1_cnt);

	/* don't run past terminate time */
	if (susp->terminate_cnt != UNKNOWN &&
	    susp->terminate_cnt <= susp->susp.current + cnt + togo) {
	    togo = susp->terminate_cnt - (susp->susp.current + cnt);
	    if (togo == 0) break;
	}


	/* don't run past logical stop time */
	if (!susp->logically_stopped && susp->susp.log_stop_cnt != UNKNOWN) {
	    int to_stop = susp->susp.log_stop_cnt - (susp->susp.current + cnt);
	    /* break if to_stop == 0 (we're at the logical stop)
	     * AND cnt > 0 (we're not at the beginning of the
	     * output block).
	     */
	    if (to_stop < togo) {
		if (to_stop == 0) {
		    if (cnt) {
			togo = 0;
			break;
		    } else /* keep togo as is: since cnt == 0, we
		            * can set the logical stop flag on this
		            * output block
		            */
			susp->logically_stopped = true;
		} else /* limit togo so we can start a new
		        * block at the LST
		        */
		    togo = to_stop;
	    }
	}

	n = togo;
	mych_reg = susp->mych;
	s1_ptr_reg = susp->s1_ptr;
	out_ptr_reg = out_ptr;
	if (n) do { /* the inner sample computation loop */

	*out_ptr_reg++ = (sample_type) (stkEffectTick(mych_reg, *s1_ptr_reg++))
;
	} while (--n); /* inner loop */

	susp->mych = mych_reg;
	/* using s1_ptr_reg is a bad idea on RS/6000: */
	susp->s1_ptr += togo;
	out_ptr += togo;
	susp_took(s1_cnt, togo);
	cnt += togo;
    } /* outer loop */

    /* test for termination */
    if (togo == 0 && cnt == 0) {
	snd_list_terminate(snd_list);
    } else {
	snd_list->block_len = cnt;
	susp->susp.current += cnt;
    }
    /* test for logical stop */
    if (susp->logically_stopped) {
	snd_list->logically_stopped = true;
    } else if (susp->susp.log_stop_cnt == susp->susp.current) {
	susp->logically_stopped = true;
    }
} /* stkchorus_n_fetch */


void stkchorus_s_fetch(register stkchorus_susp_type susp, snd_list_type snd_list)
{
    int cnt = 0; /* how many samples computed */
    int togo;
    int n;
    sample_block_type out;
    register sample_block_values_type out_ptr;

    register sample_block_values_type out_ptr_reg;

    register struct stkEffect * mych_reg;
    register sample_type s1_scale_reg = susp->s1->scale;
    register sample_block_values_type s1_ptr_reg;
    falloc_sample_block(out, "stkchorus_s_fetch");
    out_ptr = out->samples;
    snd_list->block = out;

    while (cnt < max_sample_block_len) { /* outer loop */
	/* first compute how many samples to generate in inner loop: */
	/* don't overflow the output sample block: */
	togo = max_sample_block_len - cnt;

	/* don't run past the s1 input sample block: */
	susp_check_term_log_samples(s1, s1_ptr, s1_cnt);
	togo = min(togo, susp->s1_cnt);

	/* don't run past terminate time */
	if (susp->terminate_cnt != UNKNOWN &&
	    susp->terminate_cnt <= susp->susp.current + cnt + togo) {
	    togo = susp->terminate_cnt - (susp->susp.current + cnt);
	    if (togo == 0) break;
	}


	/* don't run past logical stop time */
	if (!susp->logically_stopped && susp->susp.log_stop_cnt != UNKNOWN) {
	    int to_stop = susp->susp.log_stop_cnt - (susp->susp.current + cnt);
	    /* break if to_stop == 0 (we're at the logical stop)
	     * AND cnt > 0 (we're not at the beginning of the
	     * output block).
	     */
	    if (to_stop < togo) {
		if (to_stop == 0) {
		    if (cnt) {
			togo = 0;
			break;
		    } else /* keep togo as is: since cnt == 0, we
		            * can set the logical stop flag on this
		            * output block
		            */
			susp->logically_stopped = true;
		} else /* limit togo so we can start a new
		        * block at the LST
		        */
		    togo = to_stop;
	    }
	}

	n = togo;
	mych_reg = susp->mych;
	s1_ptr_reg = susp->s1_ptr;
	out_ptr_reg = out_ptr;
	if (n) do { /* the inner sample computation loop */

	*out_ptr_reg++ = (sample_type) (stkEffectTick(mych_reg, (s1_scale_reg * *s1_ptr_reg++)))
;
	} while (--n); /* inner loop */

	susp->mych = mych_reg;
	/* using s1_ptr_reg is a bad idea on RS/6000: */
	susp->s1_ptr += togo;
	out_ptr += togo;
	susp_took(s1_cnt, togo);
	cnt += togo;
    } /* outer loop */

    /* test for termination */
    if (togo == 0 && cnt == 0) {
	snd_list_terminate(snd_list);
    } else {
	snd_list->block_len = cnt;
	susp->susp.current += cnt;
    }
    /* test for logical stop */
    if (susp->logically_stopped) {
	snd_list->logically_stopped = true;
    } else if (susp->susp.log_stop_cnt == susp->susp.current) {
	susp->logically_stopped = true;
    }
} /* stkchorus_s_fetch */


void stkchorus_toss_fetch(susp, snd_list)
  register stkchorus_susp_type susp;
  snd_list_type snd_list;
{
    long final_count = susp->susp.toss_cnt;
    time_type final_time = susp->susp.t0;
    long n;

    /* fetch samples from s1 up to final_time for this block of zeros */
    while ((round((final_time - susp->s1->t0) * susp->s1->sr)) >=
	   susp->s1->current)
	susp_get_samples(s1, s1_ptr, s1_cnt);
    /* convert to normal processing when we hit final_count */
    /* we want each signal positioned at final_time */
    n = round((final_time - susp->s1->t0) * susp->s1->sr -
         (susp->s1->current - susp->s1_cnt));
    susp->s1_ptr += n;
    susp_took(s1_cnt, n);
    susp->susp.fetch = susp->susp.keep_fetch;
    (*(susp->susp.fetch))(susp, snd_list);
}


void stkchorus_mark(stkchorus_susp_type susp)
{
    sound_xlmark(susp->s1);
}


void stkchorus_free(stkchorus_susp_type susp)
{

	deleteStkEffect(susp->mych);
    sound_unref(susp->s1);
    ffree_generic(susp, sizeof(stkchorus_susp_node), "stkchorus_free");
}


void stkchorus_print_tree(stkchorus_susp_type susp, int n)
{
    indent(n);
    stdputstr("s1:");
    sound_print_tree_1(susp->s1, n);
}


sound_type snd_make_stkchorus(sound_type s1, double baseDelay, double depth, double freq, double mix, rate_type sr)
{
    register stkchorus_susp_type susp;
    /* sr specified as input parameter */
    time_type t0 = s1->t0;
    int interp_desc = 0;
    sample_type scale_factor = 1.0F;
    time_type t0_min = t0;
    falloc_generic(susp, stkchorus_susp_node, "snd_make_stkchorus");
    susp->mych = initStkChorus(baseDelay, depth, freq,  round(sr));
stkEffectSetMix(susp->mych, mix);

    /* select a susp fn based on sample rates */
    interp_desc = (interp_desc << 2) + interp_style(s1, sr);
    switch (interp_desc) {
      case INTERP_n: susp->susp.fetch = stkchorus_n_fetch; break;
      case INTERP_s: susp->susp.fetch = stkchorus_s_fetch; break;
      default: snd_badsr(); break;
    }

    susp->terminate_cnt = UNKNOWN;
    /* handle unequal start times, if any */
    if (t0 < s1->t0) sound_prepend_zeros(s1, t0);
    /* minimum start time over all inputs: */
    t0_min = min(s1->t0, t0);
    /* how many samples to toss before t0: */
    susp->susp.toss_cnt = (long) ((t0 - t0_min) * sr + 0.5);
    if (susp->susp.toss_cnt > 0) {
	susp->susp.keep_fetch = susp->susp.fetch;
	susp->susp.fetch = stkchorus_toss_fetch;
    }

    /* initialize susp state */
    susp->susp.free = stkchorus_free;
    susp->susp.sr = sr;
    susp->susp.t0 = t0;
    susp->susp.mark = stkchorus_mark;
    susp->susp.print_tree = stkchorus_print_tree;
    susp->susp.name = "stkchorus";
    susp->logically_stopped = false;
    susp->susp.log_stop_cnt = logical_stop_cnt_cvt(s1);
    susp->susp.current = 0;
    susp->s1 = s1;
    susp->s1_cnt = 0;
    return sound_create((snd_susp_type)susp, t0, sr, scale_factor);
}


sound_type snd_stkchorus(sound_type s1, double baseDelay, double depth, double freq, double mix, rate_type sr)
{
    sound_type s1_copy = sound_copy(s1);
    return snd_make_stkchorus(s1_copy, baseDelay, depth, freq, mix, sr);
}
