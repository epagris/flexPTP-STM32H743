/*
 * network_terminal.h
 *
 *  Created on: 2021. nov. 12.
 *      Author: epagris
 */

#ifndef NETWORK_TERMINAL_H_
#define NETWORK_TERMINAL_H_

#define NETTERM_BEACON_ADDR ("224.0.2.21")
#define NETTERM_BEACON_PORT (8021)
#define NETTERM_TERMINAL_PORT (235)

void netterm_init(); // initialize network terminal
void netterm_deinit(); // deinitialize...


#endif /* NETWORK_TERMINAL_H_ */
