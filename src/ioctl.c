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
#include <nexioctls.h>          // ioctls added in the nexmon patch
#include <tx_task.h>

struct tx_context *tx_ctx = 0;

int 
wlc_ioctl_hook(struct wlc_info *wlc, int cmd, char *arg, int len, void *wlc_if)
{
    int ret = IOCTL_ERROR;

    switch(cmd) {
        /* init tx task */
        case 429:
        {
            struct params {
                uint8 spatial_mode;
                uint8 periodic;
                uint32 delay;
                uint32 repetitions;
                uint32 rate;
                uint8 frame[];
            } __attribute__((packed));

            if (len < sizeof(struct params)) {
                printf("NEXMON %s: too few input arguments\n", __func__);
                break;
            }

            struct params *params = (struct params *)arg;
            uint32 frame_length = len - sizeof(struct params);

            /* disable minimum power consumption */
            set_mpc(wlc, 0);
            /* suppress scanning */
            set_scansuppress(wlc, 1);
            /* reduce long/short retry limit to one (no automatic retransmissions) */
            set_intioctl(wlc, WLC_SET_LRL, 1);
            set_intioctl(wlc, WLC_SET_SRL, 1);

            /* apply spatial mode setting to each band and subband */
            int sm[5] = {params->spatial_mode, params->spatial_mode, params->spatial_mode, params->spatial_mode, params->spatial_mode};
            if (wlc_stf_doiovar(wlc, 0, 27, 0, sm, sizeof(int)*5, 0, 0, 0, 0) != 0) {
                printf("NEXMON %s: could not set spatial mode\n", __func__);
            }

            /* clear old tx task and context */
            if (tx_ctx != 0 && tx_ctx->tx_task != 0) {
                tx_task_free(tx_ctx);
                tx_ctx = 0;
                printf("NEXMON %s: cleared current tx task context due to reinitalization\n", __func__);
            }

            /* get new tx task context */
            if ((tx_ctx = (struct tx_context *)mallocz(sizeof(struct tx_context) + frame_length)) == 0) {
                printf("NEXMON %s: fatal, cannot alloc tx task context\n", __func__);
                break;
            }

            if (tx_ctx->wlc == 0)
                tx_ctx->wlc = wlc;

            /* initialize tx task */
            if ((tx_ctx->tx_task = wl_init_timer(wlc->wl, tx_task_tx_now, tx_ctx, "ioctl_tx_task")) == 0) {
                printf("NEXMON %s: fatal, cannot initialize tx task\n", __func__);
                free(tx_ctx);
                tx_ctx = 0;
                break;
            }

            /* copy arguments to tx task context */
            tx_ctx->delay = params->delay;
            tx_ctx->repetitions = params->repetitions;
            tx_ctx->periodic = params->periodic;
            tx_ctx->rate = params->rate;
            tx_ctx->frame_length = frame_length;
            memcpy(tx_ctx->frame, params->frame, frame_length);
            printf("NEXMON %s: initialized tx task (delay %dms, periodic %d, repetitions %d, rate 0x%08x, frame length %d)\n", __func__, params->delay, params->periodic, params->repetitions, params->rate, frame_length);

            ret = IOCTL_SUCCESS;
        }
        break;

        /* start configured tx task */
        case 430:
        {
            if (tx_ctx == 0) {
                printf("NEXMON %s: cannot start tx task, context not initialized\n", __func__);
                break;
            }

            if (tx_ctx->tx_task != 0) {
                /* start the task */
                tx_task_start(wlc, tx_ctx->tx_task, tx_ctx->delay, tx_ctx->periodic);
                printf("NEXMON %s: tx task started\n", __func__);
                ret = IOCTL_SUCCESS;
            } else {
                printf("NEXMON %s: cannot start tx task, no task found\n", __func__);
            }
        }
        break;

        /* stop tx task */
        case 431:
        {
            if (tx_ctx == 0) {
                printf("NEXMON %s: cannot stop tx task, context not initialized\n", __func__);
                break;
            }

            if (tx_ctx->tx_task != 0) {
                /* stop the task */
                wl_del_timer(wlc->wl, tx_ctx->tx_task);
                printf("NEXMON %s: stopped tx task on command\n", __func__);
                ret = IOCTL_SUCCESS;
            } else {
                printf("NEXMON %s: cannot stop tx task, no task found\n", __func__);
            }
        }
        break;

        /* remove tx task context */
        case 432:
        {
            if (tx_ctx == 0) {
                printf("NEXMON %s: cannot remove tx task, context not initialized\n", __func__);
                break;
            }
            /* remove task and clear context */
            tx_task_free(tx_ctx);
            tx_ctx = 0;
            printf("NEXMON %s: removed tx task context on command\n", __func__);
            ret = IOCTL_SUCCESS;
        }
        break;

        default:
            ret = wlc_ioctl(wlc, cmd, arg, len, wlc_if);
    }

    return ret;
}

__attribute__((at(0x2F0CF8, "", CHIP_VER_BCM4366c0, FW_VER_10_10_122_20)))
GenericPatch4(wlc_ioctl_hook, wlc_ioctl_hook + 1);
