/***************************************************************************
 *                                                                         *
 *          ###########   ###########   ##########    ##########           *
 *         ############  ############  ############  ############          *
 *         ##            ##            ##   ##   ##  ##        ##          *
 *         ##            ##            ##   ##   ##  ##        ##          *
 *         ###########   ####  ######  ##   ##   ##  ##    ######          *
 *          ###########  ####  #       ##   ##   ##  ##    #    #          *
 *                   ##  ##    ######  ##   ##   ##  ##    #    #          *
 *                   ##  ##    #       ##   ##   ##  ##    #    #          *
 *         ############  ##### ######  ##   ##   ##  ##### ######          *
 *         ###########    ###########  ##   ##   ##   ##########           *
 *                                                                         *
 *            S E C U R E   M O B I L E   N E T W O R K I N G              *
 *                                                                         *
 * Copyright (c) 2023 Jakob Link, Matthias Schulz                          *
 *                                                                         *
 * Permission is hereby granted, free of charge, to any person obtaining a *
 * copy of this software and associated documentation files (the           *
 * "Software"), to deal in the Software without restriction, including     *
 * without limitation the rights to use, copy, modify, merge, publish,     *
 * distribute, sublicense, and/or sell copies of the Software, and to      *
 * permit persons to whom the Software is furnished to do so, subject to   *
 * the following conditions:                                               *
 *                                                                         *
 * 1. The above copyright notice and this permission notice shall be       *
 *    include in all copies or substantial portions of the Software.       *
 *                                                                         *
 * 2. Any use of the Software which results in an academic publication or  *
 *    other publication which includes a bibliography must include         *
 *    citations to the nexmon project:                                     *
 *                                                                         *
 *    "Matthias Schulz, Daniel Wegemer and Matthias Hollick. Nexmon:       *
 *     The C-based Firmware Patching Framework. https://nexmon.org"        *
 *                                                                         *
 * 3. The Software is not used by, in cooperation with, or on behalf of    *
 *    any armed forces, intelligence agencies, reconnaissance agencies,    *
 *    defense agencies, offense agencies or any supplier, contractor, or   *
 *    research associated.                                                 *
 *                                                                         *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF              *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  *
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY    *
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,    *
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE       *
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                  *
 *                                                                         *
 **************************************************************************/

#pragma NEXMON targetregion "patch"

#include <firmware_version.h>   // definition of firmware version macros
#include <wrapper.h>            // wrapper definitions for functions that already exist in the firmware
#include <structs.h>            // structures that are used by the code in the firmware
#include <helper.h>             // useful helper functions
#include <patcher.h>            // macros used to create patches such as BLPatch, BPatch, ...
#include <rates.h>              // rates used to build the ratespec for frame injection
#include <sendframe.h>
#include <tx_task.h>

static void *
tx_task_get_frame(struct tx_context *ctx)
{
    uint32 len = ctx->frame_length;
    void *p = pkt_buf_get_skb(ctx->wlc->osh, len + TXOFF);
    void *frame = 0;
    if (p == 0) {
        printf("NEXMON %s: failed to get packet\n", __func__);
        return 0;
    }
    frame = (void *) skb_pull(p, TXOFF);
    memcpy(frame, ctx->frame, len);
    return p;
}

void
tx_task_free(struct tx_context *ctx)
{
    if (ctx->wlc != 0 && ctx->tx_task != 0) {
        wl_del_timer(ctx->wlc->wl, ctx->tx_task);
        wl_free_timer(ctx->wlc->wl, ctx->tx_task);
    }
    free(ctx);
}

void
tx_task_tx_now(void *context)
{
    struct tx_context *ctx = (struct tx_context *)context;
    struct wlc_info *wlc = ctx->wlc;
    void *p = tx_task_get_frame(ctx);
    if (p != 0)  {
        if (ctx->repetitions != 0xffffffff && ctx->repetitions > 0)
            ctx->repetitions--;
        sendframe(wlc, p, TX_DATA_FIFO, ctx->rate);
        if (ctx->repetitions == 0 || ctx->periodic == 0) {
            wl_del_timer(wlc->wl, ctx->tx_task);
            printf("NEXMON %s: stopped tx task, no more repetitions\n", __func__);
        }
        return;
    }
    printf("NEXMON %s: failed to get and transmit packet\n", __func__);
}

void
tx_task_start(struct wlc_info *wlc, void *tx_task, uint delay, int periodic)
{
    if (tx_task == 0) {
        printf("NEXMON %s: cannot start tx task, no task initialized\n", __func__);
        return;
    }
    wl_add_timer(wlc->wl, tx_task, delay, periodic);
}
