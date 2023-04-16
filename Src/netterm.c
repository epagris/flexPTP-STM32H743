/*
 * task_netterm.c
 *
 *  Created on: 2021. nov. 19.
 *      Author: epagris
 */

#include <stdio.h>
#include <string.h>

#include "lwip/igmp.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "lwip/ip.h"

#include <string.h>
#include <retarget.h>

#include "netterm.h"
#include "cli.h"
#include "utils.h"

static struct udp_pcb *spBeacon_pcb;
static struct tcp_pcb *spNettermListen_pcb;

static void beacon_recv_cb(void *pArg, struct udp_pcb *pPCB, struct pbuf *pP, const ip_addr_t *pAddr, uint16_t port);
static ip_addr_t sBeaconMulticastAddr;

static err_t netterm_tcp_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err);

static int output_netterm(char *ptr, int len);

//static void netterm_recv_cb(void * pArg, struct udp_pcb * pPCB, struct pbuf *pP, const ip_addr_t * pAddr, uint16_t port);

void netterm_init() {
	// join igmp group for beacon messages
	sBeaconMulticastAddr.addr = ipaddr_addr(NETTERM_BEACON_ADDR);
	igmp_joingroup(&netif_default->ip_addr, &sBeaconMulticastAddr);

	// create mcast socket
	spBeacon_pcb = udp_new();
	udp_bind(spBeacon_pcb, IP_ADDR_ANY, NETTERM_BEACON_PORT);
	udp_recv(spBeacon_pcb, beacon_recv_cb, NULL);

	// create terminal socket
	spNettermListen_pcb = tcp_new();
	tcp_bind(spNettermListen_pcb, IP_ADDR_ANY, NETTERM_TERMINAL_PORT);
	spNettermListen_pcb = tcp_listen(spNettermListen_pcb);
	tcp_accept(spNettermListen_pcb, netterm_tcp_accept_cb);
}

void netterm_deinit() {
	// leave igmp group
	ip_addr_t addr = { ipaddr_addr(NETTERM_BEACON_ADDR) };
	igmp_leavegroup(&netif_default->ip_addr, &addr);

	// disconnect udp
	udp_disconnect(spBeacon_pcb);

	// remove udp pcb
	udp_remove(spBeacon_pcb);
}

// ------------------------

struct BeaconMsg {
	ip_addr_t senderAddr; // sender address
	uint16_t terminalPort; // port of terminal
	uint8_t server_nClient; // server or client
};

static void beacon_recv_cb(void *pArg, struct udp_pcb *pPCB, struct pbuf *pP, const ip_addr_t *pAddr, uint16_t port) {
	// reuse pbuf WARNING: this is only viable due to matching receive and transmit size
	struct BeaconMsg bmsg;
	memcpy(&bmsg, pP->payload, sizeof(struct BeaconMsg));

	// if message came from a server then, respond
	if (bmsg.server_nClient == 1) {
		struct pbuf *pBufResp = pbuf_alloc(PBUF_TRANSPORT, sizeof(struct BeaconMsg), PBUF_RAM);
		bmsg.senderAddr = netif_default->ip_addr;
		bmsg.server_nClient = 0; // we're a client
		bmsg.terminalPort = NETTERM_TERMINAL_PORT; // fill in terminal port
		memcpy(pBufResp->payload, &bmsg, sizeof(struct BeaconMsg)); // copy back to buffer
		udp_sendto(spBeacon_pcb, pBufResp, &sBeaconMulticastAddr, NETTERM_BEACON_PORT);
	}

	pbuf_free(pP);
}

/*static void netterm_recv_cb(void * pArg, struct udp_pcb * pPCB, struct pbuf *pP, const ip_addr_t * pAddr, uint16_t port) {
 const char * sLine = pP->payload;
 process_cli_line(pP->payload);
 pbuf_free(pP);
 }*/

// trim whitespace (e.g. CR and LF) characters from the end of input string in a non-destructive way
static void trim_end_nondest(char *pStr, size_t *len) {
	while (pStr[(*len) - 1] <= ' ' && (*len) > 0) {
		(*len)--;
	}
}

#define NETTERM_MAX_LINE_LENGTH (127)
static struct tcp_pcb *sDefOutputConnection = NULL;
static struct tcp_pcb *sCurrentOutput = NULL;

// parameters of a tcp connection
struct NettermConnArgs {
	bool canBeDefaultOutputTTY; // marks if this connection could be a default output
	char inputBuf[NETTERM_MAX_LINE_LENGTH + 1]; // input buffer
	char cmdBuf[NETTERM_MAX_LINE_LENGTH + 1]; // input buffer
};

static err_t netterm_tcp_recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
	if (p == NULL) {
		goto close_conn;
	}

	// ----- RECEIVE COMMAND STRING ------

	struct NettermConnArgs *pConnArgs = arg;

	size_t len = MIN(p->len, NETTERM_MAX_LINE_LENGTH);
	memcpy(pConnArgs->inputBuf, p->payload, len);
	pbuf_free(p);

	pConnArgs->inputBuf[len] = '\0';

	trim_end_nondest(pConnArgs->inputBuf, &len);

	pConnArgs->inputBuf[len] = '\0';

	// ----- PROCESS COMMANDS ONE-BY-ONE -----
	char *c = pConnArgs->inputBuf;
	while (*c != '\0') {

		// ------- EXTRACT COMMAND ---------

		// determine the length of current command
		char *c_next = c; // beginning of the next command, i. e. end of current one
		while ((*c_next != '\n') && (*c_next != '\0')) {
			c_next++;
		}
		size_t cmdlen = c_next - c; // determine command length from the pointers
		strncpy(pConnArgs->cmdBuf, c, cmdlen); // extract command
		trim_end_nondest(pConnArgs->cmdBuf, &cmdlen); // trim trailing whitespaces
		if (cmdlen == 0) { // if the string contained only whitespaces, then skip processing
			continue;
		}
		pConnArgs->cmdBuf[cmdlen] = '\0'; // ...otherwise stub string

		c = c_next; // move iterator
		if ((*c) != '\0') { // if we are not at the end of the input string...
			c++;	// ...advance iterator to the next, unexamined character
		}

		// ---------- PROCESS COMMAND ------------

		if (!strcmp(pConnArgs->cmdBuf, "exit")) {
			goto close_conn;
		} else if (!strncmp(pConnArgs->cmdBuf, "msg", 3)) {
			if (sDefOutputConnection != NULL) {
				tcp_write(sDefOutputConnection, pConnArgs->cmdBuf + 4, strlen(pConnArgs->cmdBuf) - 4, TCP_WRITE_FLAG_COPY);
				tcp_write(sDefOutputConnection, "\r\n", 2, TCP_WRITE_FLAG_COPY);
			}
		} else if (!strcmp(pConnArgs->cmdBuf, "nodeftty")) {
			pConnArgs->canBeDefaultOutputTTY = false;
		} else {

			struct NettermConnArgs *pConnParams = arg;
			if (pConnParams->canBeDefaultOutputTTY && sDefOutputConnection == NULL) { // set default output if needed
				sDefOutputConnection = tpcb;
				RetargetSetOutput(output_netterm);
			}

			// set new output function
			StreamOutputFunction oldSof = RetargetGetOutput();
			RetargetSetOutput(output_netterm);
			sCurrentOutput = tpcb;

			// process line
			process_cli_line(pConnArgs->cmdBuf);

			// restore old output function
			RetargetSetOutput(oldSof);
			sCurrentOutput = NULL;
		}

	}

	return ERR_OK;

	// close connection
	close_conn:

	if (sDefOutputConnection == tpcb) {
		RetargetSetOutput(NULL);
		sDefOutputConnection = NULL;
	}

	tcp_close(tpcb);
	free(arg);

	return ERR_OK;

}

static void netterm_tcp_err_cb(void *arg, err_t err) {
	printf("TCP error: %d!\n", err);
}

static int output_netterm(char *ptr, int len) {
	struct tcp_pcb *pcb = (sCurrentOutput == NULL) ? sDefOutputConnection : sCurrentOutput;
	if (pcb != NULL) {
		tcp_write(pcb, ptr, len, TCP_WRITE_FLAG_COPY);
	}
	return len;
}

static err_t netterm_tcp_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err) {
	// allocate space for connection parameters
	struct NettermConnArgs *pConnPar = malloc(sizeof(struct NettermConnArgs));
	pConnPar->canBeDefaultOutputTTY = true;

	tcp_arg(newpcb, pConnPar);
	tcp_recv(newpcb, netterm_tcp_recv_cb);
	tcp_err(newpcb, netterm_tcp_err_cb);
	tcp_write(newpcb, TERMINAL_LEAD, strlen(TERMINAL_LEAD), TCP_WRITE_FLAG_COPY);

	return ERR_OK;
}
