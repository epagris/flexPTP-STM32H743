#include "FreeRTOS.h"
#include "task.h"

#include "lwip/netif.h"

#include "user_tasks.h"

#include "cli.h"

#include "netterm.h"

// ----- TASK PROPERTIES -----
static TaskHandle_t sTH; // task handle
static uint8_t sPrio = 5; // priority
static uint16_t sStkSize = 1024; // stack size
void task_eth(void * pParam); // task routine function
// ---------------------------

typedef union {
	uint32_t qword;
	uint8_t bytes[4];
} IPAddrUnion;

static struct {
	IPAddrUnion ipAddr; // ip-address
	bool isConnected; // are we connected to a network?
} sState;


#define PRINT_IP(ip) MSG("IP-address: %u.%u.%u.%u\r\n", (ip & 0xFF), ((ip >> 8) & 0xFF), ((ip >> 16) & 0xFF), ((ip >> 24) & 0xFF));

static int CB_ip(const CliToken_Type *ppArgs, uint8_t argc) {
	PRINT_IP(netif_default->ip_addr.addr);
	return 0;
}

// register task
void reg_task_eth() {
	BaseType_t result = xTaskCreate(task_eth, "eth", sStkSize, NULL, sPrio, &sTH);
	if (result != pdPASS) { // error handling
    	MSG("Failed to create task! (errcode: %ld)\n", result);
	}


	// register CLI commands
	cli_register_command("ip \t\t\tPrint IP-address", 1, 0, CB_ip);
}

#define IP_ADDR_VALID(ip) (ip != 0 && ip != ~0)

void task_eth(void * pParam) {
	uint32_t currentIP = 0; // current IP address

	// MAIN LOOP
	while (1) {
		vTaskDelay(pdMS_TO_TICKS(500)); // delay between pollings

		currentIP = netif_default->ip_addr.addr;

		// if we were not up before but now we have received an IP address
		if (sState.isConnected == false && IP_ADDR_VALID(currentIP) && netif_is_link_up(netif_default)) {
			sState.isConnected = true;

			// print IP address
			MSG("Connected!\n");
			PRINT_IP(currentIP);

			// --------------------

			vTaskDelay(pdMS_TO_TICKS(200));

			// start PTP task
			MSG("Starting PTP-task!\n");
			reg_task_ptp();

			// --------------------

			MSG("Starting NETTERM!\n");
			netterm_init();

		} else if (sState.isConnected == true && !IP_ADDR_VALID(currentIP) && !netif_is_link_up(netif_default)) { // if we have disconnected from the network

		    MSG("Disconnected!\n");

			sState.isConnected = false;

			// stop PTP task
			MSG("Stopping PTP-task!\n");
			unreg_task_ptp();

			// -------------------

			MSG("Stopping NETTERM!\n");
			netterm_deinit();

		} else { // we are connected
			// Todo ...
		}
	}
}
