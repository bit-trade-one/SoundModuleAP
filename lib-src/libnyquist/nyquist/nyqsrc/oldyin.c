/* yin.c -- partial implementation of the YIN algorithm, with some 
 *   fixes by DM. This code should be replaced with the fall 2002
 *   intro to computer music implementation project.
 */

#include "stdio.h"
#ifdef UNIX
#include "sys/file.h"
#endif
#ifndef mips
#include "stdlib.h"
#endif
#include "snd.h"
#include "xlisp.h"
#include "sound.h"
#include "falloc.h"
#include "yin.h"

void yin_free();

/* for multiple channel results, one susp is shared by all sounds */
/* the susp in turn must point back to all sound list tails */

typedef struct yin_susp_struct {
    snd_susp_node susp;
    long terminate_cnt;
    boolean logically_stopped;
    sound_type s;
    long s_cnt;
    sample_block_values_type s_ptr;
    long blocksize;
    long stepsize;
    sample_type *block;
    float *temp;
    sample_type *fillptr;
    sample_type *endptr;
    snd_list_type chan[2];	/* array of back pointers */
    long cnt;	/* how many sample frames to read */
    long m;
    long middle;
} yin_susp_node, *yin_susp_type;


// Uses cubic interpolation to return the value of x such
// that the function defined by f(0), f(1), f(2), and f(3)
// is maximized.
//
float CubicMaximize(float y0, float y1, float y2, float y3)
{
  // Find coefficients of cubic

  float a, b, c, d;
  float da, db, dc;
  float discriminant;
  float x1, x2;
  float dda, ddb;
  
  a = (float) (y0/-6.0 + y1/2.0 - y2/2.0 + y3/6.0);
  b = (float) (y0 - 5.0*y1/2.0 + 2.0*y2 - y3/2.0);
  c = (float) (-11.0*y0/6.0 + 3.0*y1 - 3.0*y2/2.0 + y3/3.0);
  d = y0;

  // Take derivative

  da = 3*a;
  db = 2*b;
  dc = c;

  // Find zeroes of derivative using quadratic equation
  
  discriminant = db*db - 4*da*dc;
  if (discriminant < 0.0)
    return -1.0; // error
  
  x1 = (float) ((-db + sqrt(discriminant)) / (2 * da));
  x2 = (float) ((-db - sqrt(discriminant)) / (2 * da));
  
  // The one which corresponds to a local _maximum_ in the
  // cubic is the one we want - the one with a negative
  // second derivative
  
  dda = 2*da;
  ddb = db;
  
  if (dda*x1 + ddb < 0)
    return x1;
  else
    return x2;
}


void yin_compute(yin_susp_type susp, float *pitch, float *harmonicity)
{
    float *samples = susp->block;
    int middle = susp->middle;
    /* int n = middle * 2; */
    int m = susp->m;
    float threshold = 0.9F;
    float *results = susp->temp;

    /* samples is a buffer of samples */
    /* n is the number of samples, equals twice longest period, must be even */
    /* m is the shortest period in samples */
    /* results is an array of size n/2 - m + 1, the number of different lags */

    /* work from the middle of the buffer: */
    int i, j; /* loop counters */
    /* how many different lags do we compute? */
    /* int iterations = middle + 1 - m; */
    float left_energy = 0;
    float right_energy = 0;
    /* for each window, we keep the energy so we can compute the next one */
    /* incrementally. First, we need to compute the energies for lag m-1: */
    *pitch = 0;
    for (i = 0; i < m - 1; i++) {
        float left = samples[middle - 1 - i];
        float right = samples[middle + i];
        left_energy += left * left;
        right_energy += right * right;
    }
    for (i = m; i <= middle; i++) {
        /* i is the lag and the length of the window */
        /* compute the energy for left and right */
        float left, right, energy, a;
        float harmonic;
        left = samples[middle - i];
        left_energy += left * left;
        right = samples[middle - 1 + i];
        right_energy += right * right;
        /*  compute the autocorrelation */
        a = 0;
        for (j = 0; j < i; j++) {
            a += samples[middle - i + j] * samples[middle + j];
        }
        energy = left_energy + right_energy;
        harmonic = (2 * a) / energy;
        results[i - m] = harmonic;
    }
    for (i = m; i <= middle; i++) {
        if (results[i - m] > threshold) {
            float f_i = (i - 1) + 
                CubicMaximize(results[i - m - 1], results[i - m],
                              results[i - m + 1], results[i - m + 2]);
            if (f_i < i - m - 1 || f_i > i - m + 2) f_i = (float) i;
            *pitch = (float) hz_to_step((float) susp->susp.sr / f_i);
            *harmonicity = results[i - m];
            break;
        }
    }
}


/* yin_fetch - compute F0 and harmonicity using YIN approach.  */
/*
 * The pitch (F0) is determined by finding two periods whose
 * inner product accounts for almost all of the energy. Let X and Y
 * be adjacent vectors of length N in the sample stream. Then, 
 *    if 2X*Y > threshold * (X*X + Y*Y)
 *    then the period is given by N
 * In the algorithm, we compute different sizes until we find a
 * peak above threshold. Then, we use cubic interpolation to get
 * a precise value. If no peak above threshold is found, we return
 * the first peak. The second channel returns the value 2X*Y/(X*X+Y*Y)
 * which is refered to as the "harmonicity" -- the amount of energy
 * accounted for by periodicity.
 *
 * Low sample rates are advised because of the high cost of computing
 * inner products (fast autocorrelation is not used).
 *
 * The result is a 2-channel signal running at the requested rate.
 * The first channel is the estimated pitch, and the second channel
 * is the harmonicity.
 *
 * This code is adopted from multiread, currently the only other
 * multichannel suspension in Nyquist. Comments from multiread include:
 * The susp is shared by all channels.  The susp has backpointers
 * to the tail-most snd_list node of each channel, and it is by
 * extending the list at these nodes that sounds are read in.
 * To avoid a circularity, the reference counts on snd_list nodes
 * do not include the backpointers from this susp.  When a snd_list
 * node refcount goes to zero, the yin susp's free routine
 * is called.  This must scan the backpointers to find the node that
 * has a zero refcount (the free routine is called before the node
 * is deallocated, so this is safe).  The backpointer is then set
 * to NULL.  When all backpointers are NULL, the susp itself is
 * deallocated, because it can only be referenced through the
 * snd_list nodes to which there are backpointers.
 */
void yin_fetch(yin_susp_type susp, snd_list_type snd_list)
{
    int cnt = 0; /* how many samples computed */
    int togo = 0;
    int n;
    sample_block_type f0;
    sample_block_values_type f0_ptr = NULL;
    sample_block_type harmonicity;
    sample_block_values_type harmonicity_ptr = NULL;

    register sample_block_values_type s_ptr_reg;
    register sample_type *fillptr_reg;
    register sample_type *endptr_reg = susp->endptr;

    if (susp->chan[0]) {
        falloc_sample_block(f0, "yin_fetch");
        f0_ptr = f0->samples;
        /* Since susp->chan[i] exists, we want to append a block of samples.
         * The block, out, has been allocated.  Before we insert the block,
         * we must figure out whether to insert a new snd_list_type node for
         * the block.  Recall that before SND_get_next is called, the last
         * snd_list_type in the list will have a null block pointer, and the
         * snd_list_type's susp field points to the suspension (in this case,
         * susp).  When SND_get_next (in sound.c) is called, it appends a new
         * snd_list_type and points the previous one to internal_zero_block 
         * before calling this fetch routine.  On the other hand, since 
         * SND_get_next is only going to be called on one of the channels, the
         * other channels will not have had a snd_list_type appended.
         * SND_get_next does not tell us directly which channel it wants (it
         * doesn't know), but we can test by looking for a non-null block in the
         * snd_list_type pointed to by our back-pointers in susp->chan[].  If
         * the block is null, the channel was untouched by SND_get_next, and
         * we should append a snd_list_type.  If it is non-null, then it
         * points to internal_zero_block (the block inserted by SND_get_next)
         * and a new snd_list_type has already been appended.
         */
        /* Before proceeding, it may be that garbage collection ran when we
         * allocated out, so check again to see if susp->chan[j] is Null:
         */
        if (!susp->chan[0]) {
            ffree_sample_block(f0, "yin_fetch");
            f0 = NULL; /* make sure we don't free it again */
            f0_ptr = NULL; /* make sure we don't output f0 samples */
        } else if (!susp->chan[0]->block) {
            snd_list_type snd_list = snd_list_create((snd_susp_type) susp);
            /* Now we have a snd_list to append to the channel, but a very
             * interesting thing can happen here.  snd_list_create, which
             * we just called, MAY have invoked the garbage collector, and
             * the GC MAY have freed all references to this channel, in which
             * case yin_free(susp) will have been called, and susp->chan[0]
             * will now be NULL!
             */
            if (!susp->chan[0]) {
                ffree_snd_list(snd_list, "yin_fetch");
            } else {
                susp->chan[0]->u.next = snd_list;
            }
        }
        /* see the note above: we don't know if susp->chan still exists */
        /* Note: We DO know that susp still exists because even if we lost
         * some channels in a GC, someone is still calling SND_get_next on
         * some channel.  I suppose that there might be some very pathological
         * code that could free a global reference to a sound that is in the
         * midst of being computed, perhaps by doing something bizarre in the
         * closure that snd_seq activates at the logical stop time of its first
         * sound, but I haven't thought that one through.
         */
        if (susp->chan[0]) {
            susp->chan[0]->block = f0;
            /* check some assertions */
            if (susp->chan[0]->u.next->u.susp != (snd_susp_type) susp) {
                nyquist_printf("didn't find susp at end of list for chan 0\n");
            }
        } else if (f0) { /* we allocated f0, but don't need it anymore due to GC */
            ffree_sample_block(f0, "yin_fetch");
            f0_ptr = NULL;
        }
    }

    /* Now, repeat for channel 1 (comments omitted) */
    if (susp->chan[1]) {
        falloc_sample_block(harmonicity, "yin_fetch");
        harmonicity_ptr = harmonicity->samples;
        if (!susp->chan[1]) {
            ffree_sample_block(harmonicity, "yin_fetch");
            harmonicity = NULL; /* make sure we don't free it again */
            harmonicity_ptr = NULL;
        } else if (!susp->chan[1]->block) {
            snd_list_type snd_list = snd_list_create((snd_susp_type) susp);
            if (!susp->chan[1]) {
                ffree_snd_list(snd_list, "yin_fetch");
            } else {
                susp->chan[1]->u.next = snd_list;
            }
        }
        if (susp->chan[1]) {
            susp->chan[1]->block = harmonicity;
            if (susp->chan[1]->u.next->u.susp != (snd_susp_type) susp) {
                nyquist_printf("didn't find susp at end of list for chan 1\n");
            }
        } else if (harmonicity) { /* we allocated harmonicity, but don't need it anymore due to GC */
            ffree_sample_block(harmonicity, "yin_fetch");
            harmonicity_ptr = NULL;
        }
    }

    while (cnt < max_sample_block_len) { /* outer loop */
        /* first, compute how many samples to generate in inner loop: */
        /* don't overflow the output sample block */
        togo = (max_sample_block_len - cnt) * susp->stepsize;

        /* don't run past the s input sample block */
        susp_check_term_log_samples(s, s_ptr, s_cnt);
        togo = min(togo, susp->s_cnt);

        /* don't run past terminate time */
        if (susp->terminate_cnt != UNKNOWN &&
            susp->terminate_cnt <= susp->susp.current + cnt + togo/susp->stepsize) {
            togo = (susp->terminate_cnt - (susp->susp.current + cnt)) * susp->stepsize;
            if (togo == 0) break;
        }

        /* don't run past logical stop time */
        if (!susp->logically_stopped && susp->susp.log_stop_cnt != UNKNOWN) {
            int to_stop = susp->susp.log_stop_cnt - (susp->susp.current + cnt);
            /* break if to_stop = 0 (we're at the logical stop)
             * AND cnt > 0 (we're not at the beginning of the output block)
             */
            if (to_stop < togo/susp->stepsize) {
                if (to_stop == 0) {
                    if (cnt) {
                        togo = 0;
                        break;
                    } else /* keep togo as is: since cnt == 0, we can set
                            * the logical stop flag on this output block
                            */
                        susp->logically_stopped = true;
                } else /* limit togo so we can start a new block a the LST */
                    togo = to_stop * susp->stepsize;
            }
        }
        n = togo;
        s_ptr_reg = susp->s_ptr;
        fillptr_reg = susp->fillptr;
        if (n) do { /* the inner sample computation loop */
            *fillptr_reg++ = *s_ptr_reg++;
            if (fillptr_reg >= endptr_reg) {
                float f0;
                float harmonicity;
                yin_compute(susp, &f0, &harmonicity);
                if (f0_ptr) *f0_ptr++ = f0;
                if (harmonicity_ptr) *harmonicity_ptr++ = harmonicity;
                cnt++;
                fillptr_reg -= susp->stepsize;
            }
        } while (--n); /* inner loop */

        /* using s_ptr_reg is a bad idea on RS/6000: */
        susp->s_ptr += togo;
        susp->fillptr = fillptr_reg;
        susp_took(s_cnt, togo);
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
} /* yin_fetch */

  
void yin_mark(yin_susp_type susp)
{
    sound_xlmark(susp->s);
}


void yin_free(yin_susp_type susp)
{
    int j;
    boolean active = false;
/*    stdputstr("yin_free: "); */

    for (j = 0; j < 2; j++) {
        if (susp->chan[j]) {
            if (susp->chan[j]->refcnt) active = true;
            else {
                susp->chan[j] = NULL;
                /* nyquist_printf("deactivating channel %d\n", j); */
            }
        }
    }
    if (!active) {
/*      stdputstr("all channels freed, freeing susp now\n"); */
        ffree_generic(susp, sizeof(yin_susp_node), "yin_free");
        sound_unref(susp->s);
        free(susp->block);
        free(susp->temp);
    }
}


void yin_print_tree(yin_susp_type susp, int n)
{
    indent(n);
    stdputstr("s:");
    sound_print_tree_1(susp->s, n);
}


LVAL snd_make_yin(sound_type s, double low_step, double high_step, long stepsize)
{
    LVAL result;
    int j;
    register yin_susp_type susp;
    rate_type sr = s->sr;
    time_type t0 = s->t0;

    falloc_generic(susp, yin_susp_node, "snd_make_yin");
    susp->susp.fetch = yin_fetch;
    susp->terminate_cnt = UNKNOWN;
    
    /* initialize susp state */
    susp->susp.free = yin_free;
    susp->susp.sr = sr / stepsize;
    susp->susp.t0 = t0;
    susp->susp.mark = yin_mark;
    susp->susp.print_tree = yin_print_tree;
    susp->susp.name = "yin";
    susp->logically_stopped = false;
    susp->susp.log_stop_cnt = logical_stop_cnt_cvt(s);
    susp->susp.current = 0;
    susp->s = s;
    susp->s_cnt = 0;
    susp->m = (long) (sr / step_to_hz(high_step));
    if (susp->m < 2) susp->m = 2;
    /* add 1 to make sure we round up */
    susp->middle = (long) (sr / step_to_hz(low_step)) + 1;
    susp->blocksize = susp->middle * 2;
    susp->stepsize = stepsize;
    /* blocksize must be at least step size to implement stepping */
    if (susp->stepsize > susp->blocksize) susp->blocksize = susp->stepsize;
    susp->block = (sample_type *) malloc(susp->blocksize * sizeof(sample_type));
    susp->temp = (float *) malloc((susp->middle - susp->m + 1) * sizeof(float));
    susp->fillptr = susp->block;
    susp->endptr = susp->block + susp->blocksize;

    xlsave1(result);

    result = newvector(2);      /* create array for F0 and harmonicity */
    /* create sounds to return */
    for (j = 0; j < 2; j++) {
        sound_type snd = sound_create((snd_susp_type)susp, 
                                      susp->susp.t0, susp->susp.sr, 1.0);
        LVAL snd_lval = cvsound(snd);
/*      nyquist_printf("yin_create: sound %d is %x, LVAL %x\n", j, snd, snd_lval); */
        setelement(result, j, snd_lval);
        susp->chan[j] = snd->list;
    }
    xlpop();
    return result;
}


LVAL snd_yin(sound_type s, double low_step, double high_step, long stepsize)
{
    sound_type s_copy = sound_copy(s);
    return snd_make_yin(s_copy, low_step, high_step, stepsize);
}
