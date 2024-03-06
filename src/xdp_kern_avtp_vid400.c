// SPDX-License-Identifier: (GPL-2.0 OR BSD-2-Clause)
/*
 * Copyright (C) 2022 Linutronix GmbH
 * Author Kurt Kanzenbach <kurt@linutronix.de>
 */

#include <linux/bpf.h>

#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>

#include <xdp/xdp_helpers.h>

#include "net_def.h"

struct {
	__uint(type, BPF_MAP_TYPE_XSKMAP);
	__uint(key_size, sizeof(int));
	__uint(value_size, sizeof(int));
	__uint(max_entries, 128);
} xsks_map SEC(".maps");

struct {
	__uint(priority, 20);
	__uint(XDP_PASS, 1);
} XDP_RUN_CONFIG(xdp_sock_prog);

SEC("xdp_sock")
int xdp_sock_prog(struct xdp_md *ctx)
{
	void *dataEnd = (void *)(long)ctx->data_end;
	void *data = (void *)(long)ctx->data;
	struct VLANEthernetHeader *veth;
	int idx = ctx->rx_queue_index;
	void *p = data;

	veth = p;
	if ((void *)(veth + 1) > dataEnd)
		return XDP_PASS;

	/* Check for VLAN frames */
	if (veth->VLANProto != bpf_htons(ETH_P_8021Q))
		return XDP_PASS;

	/* Check for AVTP EtherType */
	if (veth->VLANEncapsulatedProto != bpf_htons(ETH_P_AVTP))
		return XDP_PASS;

	/* Check for VID 400 */
	if ((bpf_ntohs(veth->VLANTCI) & VLAN_ID_MASK) != 400)
		return XDP_PASS;

	/* If socket bound to rx_queue then redirect to user space */
	if (bpf_map_lookup_elem(&xsks_map, &idx))
		return bpf_redirect_map(&xsks_map, idx, 0);

	return XDP_PASS;
}

char _license[] SEC("license") = "Dual BSD/GPL";
