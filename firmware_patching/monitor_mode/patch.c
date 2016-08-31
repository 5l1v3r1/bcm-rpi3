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
 * Warning:                                                                *
 *                                                                         *
 * Our software may damage your hardware and may void your hardware’s      *
 * warranty! You use our tools at your own risk and responsibility!        *
 *                                                                         *
 * License:                                                                *
 * Copyright (c) 2015 NexMon Team                                          *
 *                                                                         *
 * Permission is hereby granted, free of charge, to any person obtaining   *
 * a copy of this software and associated documentation files (the         *
 * "Software"), to deal in the Software without restriction, including     *
 * without limitation the rights to use, copy, modify, merge, publish,     *
 * distribute copies of the Software, and to permit persons to whom the    *
 * Software is furnished to do so, subject to the following conditions:    *
 *                                                                         *
 * The above copyright notice and this permission notice shall be included *
 * in all copies or substantial portions of the Software.                  *
 *                                                                         *
 * Any use of the Software which results in an academic publication or     *
 * other publication which includes a bibliography must include a citation *
 * to the author's publication "M. Schulz, D. Wegemer and M. Hollick.      *
 * NexMon: A Cookbook for Firmware Modifications on Smartphones to Enable  *
 * Monitor Mode.".                                                         *
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

#include "../include/bcm43438.h"
#include "../include/wrapper.h"	// wrapper definitions for functions that already exist in the firmware
#include "../include/structs.h"	// structures that are used by the code in the firmware
#include "ieee80211_radiotap.h"
#include "radiotap.h"
#include "d11.h"

struct brcmf_proto_bcdc_header {
	unsigned char flags;
	unsigned char priority;
	unsigned char flags2;
	unsigned char data_offset;
};

struct bdc_radiotap_header {
    struct brcmf_proto_bcdc_header bdc;
    struct ieee80211_radiotap_header radiotap;
} __attribute__((packed));

void *
skb_push(sk_buff *p, unsigned int len) {
    p->data -= len;
    p->len += len;

    return p->data;
}

void *
skb_pull(sk_buff *p, unsigned int len) {
    p->data += len;
    p->len -= len;

    return p->data;
}

int
wlc_bmac_recv_hook(struct wlc_hw_info *wlc_hw, unsigned int fifo, int bound, int *processed_frame_cnt) {
    struct wlc_pub *pub = wlc_hw->wlc->pub;
    sk_buff *p;
    struct bdc_radiotap_header *frame;
    struct wlc_d11rxhdr *wlc_rxhdr;
    int n = 0;
    int bound_limit = bound ? pub->tunables->rxbnd : -1;
    //struct tsf tsf;

    do {
        p = dma_rx (wlc_hw->di[fifo]);
        if(!p) {
            goto LEAVE;
        }

        //TODO: check wlc_rxhdr->rxhdr.RxStatus1
        wlc_rxhdr = (struct wlc_d11rxhdr *) p->data;

        if(wlc_rxhdr->rxhdr.RxStatus1 & 4) {
            skb_pull(p, 46);
        } else {
            skb_pull(p, 44);
        }

		skb_push(p, sizeof(struct bdc_radiotap_header));

        frame = (struct bdc_radiotap_header *) p->data;
        frame->bdc.flags = 0x20;
        frame->bdc.priority = 0;
        frame->bdc.flags2 = 0;
        frame->bdc.data_offset = 0;

        frame->radiotap.it_version = 0;
        frame->radiotap.it_pad = 0;
        frame->radiotap.it_len = sizeof(struct ieee80211_radiotap_header);
        frame->radiotap.it_present = 
             (1<<IEEE80211_RADIOTAP_TSFT) 
           | (1<<IEEE80211_RADIOTAP_FLAGS);
        frame->radiotap.tsf.tsf_l = 0;
        frame->radiotap.tsf.tsf_h = 0;
        frame->radiotap.flags = IEEE80211_RADIOTAP_F_FCS;

        dngl_sendpkt(SDIO_INFO_ADDR, p, 2);

    } while(n < bound_limit);

LEAVE:
    dma_rxfill(wlc_hw->di[fifo]);

    *processed_frame_cnt += n;
    
    if(n < bound_limit) {
        return 0;
    } else {
        return 1;
    }
}

void
dummy_0F0(void) {
    ;
}
