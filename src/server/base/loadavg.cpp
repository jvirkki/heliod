/*
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS HEADER.
 *
 * Copyright 2008 Sun Microsystems, Inc. All rights reserved.
 *
 * THE BSD LICENSE
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer. 
 * Redistributions in binary form must reproduce the above copyright notice, 
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution. 
 *
 * Neither the name of the  nor the names of its contributors may be
 * used to endorse or promote products derived from this software without 
 * specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER 
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "time/nstime.h"
#include "base/loadavg.h"

/*
 * loadavg_init
 */
void loadavg_init(struct loadavg *loadavg)
{
    for (int i = 0; i < LOADAVG_NSTATS; i++) {
        loadavg->avgn[i] = 0;
        loadavg->hp_avgn[i] = 0;
    }

    loadavg->epoch = ft_timeIntervalNow();
    loadavg->updates = -1;
}

/*
 * loadavg_calc
 */
static void loadavg_calc(struct loadavg *loadavg, PRUint32 n)
{
    static const int f[LOADAVG_NSTATS] = { 1083, 218, 73 };

    /*
     * Compute load average over the last 1, 5, and 15 minutes
     * (60, 300, and 900 seconds).  The constants in f[3] are for
     * exponential decay:
     * (1 - exp(-1/60)) << 16 = 1083,
     * (1 - exp(-1/300)) << 16 = 218,
     * (1 - exp(-1/900)) << 16 = 73.
     */

    /*
     * a little hoop-jumping to avoid integer overflow
     */
    for (int i = 0; i < LOADAVG_NSTATS; i++) {
        PRUint32 q = loadavg->hp_avgn[i] >> 16;
        PRUint32 r = loadavg->hp_avgn[i] & 0xffff;
        loadavg->hp_avgn[i] += (n - q) * f[i] - ((r * f[i]) >> 16);
    }
}

/*
 * loadavg_update
 */
void loadavg_update(struct loadavg *loadavg, PRUint32 n)
{
    /* Avoid integer overflow in hp_avgn */
    if (n > 0xffff)
        n = 0xffff;

    /* Compensate for the fact we're not called exactly once per second */
    PRIntervalTime now = ft_timeIntervalNow();
    int elapsed = PR_IntervalToSeconds(now - loadavg->epoch);
    while (elapsed > loadavg->updates) {
        /* Calculate load average */
        loadavg_calc(loadavg, n);
        loadavg->updates++;
    }

    /* Avoid PRIntervalTime overflow */
    static const int max_updates_per_epoch = PR_IntervalToSeconds(0x80000000);
    if (loadavg->updates > max_updates_per_epoch) {
        loadavg->epoch = now;
        loadavg->updates = 0;
    }

    for (int i = 0; i < LOADAVG_NSTATS; i++)
        loadavg->avgn[i] = loadavg->hp_avgn[i] >> (16 - LOADAVG_FSHIFT);
}

/*
 * loadavg_get
 */
void loadavg_get(const struct loadavg *loadavg, PRFloat64 loadavgs[], int nelem)
{
    if (nelem > LOADAVG_NSTATS)
        nelem = LOADAVG_NSTATS;

    for (int i = 0; i < nelem; i++)
        loadavgs[i] = (PRFloat64)loadavg->avgn[i] / LOADAVG_FSCALE;
}
