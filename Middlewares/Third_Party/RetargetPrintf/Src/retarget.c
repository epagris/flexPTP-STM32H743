// All credit to Carmine Noviello for this code
// https://github.com/cnoviello/mastering-stm32/blob/master/nucleo-f030R8/system/src/retarget/retarget.c

#include <_ansi.h>
#include <_syslist.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/times.h>
#include <limits.h>
#include <signal.h>
#include "retarget.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <semphr.h>

#if !defined(OS_USE_SEMIHOSTING)

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

static UART_HandleTypeDef *gHuart;

static SemaphoreHandle_t sTxSem;

static StreamOutputFunction sOutputFunc = NULL, sFallbackOutputFunc = NULL;

void RetargetInit(UART_HandleTypeDef *huart) {
	gHuart = huart;

	/* Disable I/O buffering for STDOUT stream, so that
	 * chars are sent out as soon as they are printed. */
	//setvbuf(stdout, NULL, _IONBF, 0);
	sTxSem = xSemaphoreCreateBinary();
	xSemaphoreGive(sTxSem);
}

#define RETARGET_TX_MTX_LOCK() \
		if (__get_IPSR() != 0) { \
			xSemaphoreTakeFromISR(sTxSem, portMAX_DELAY); \
		} else { \
			xSemaphoreTake(sTxSem, portMAX_DELAY); \
		}

#define RETARGET_TX_MTX_UNLOCK() \
		if (__get_IPSR() != 0) { \
			xSemaphoreGiveFromISR(sTxSem, pdFALSE); \
		} else { \
			xSemaphoreGive(sTxSem); \
		}

void RetargetSetOutput(StreamOutputFunction sof) {
	RETARGET_TX_MTX_LOCK();

	sOutputFunc = sof;

	RETARGET_TX_MTX_UNLOCK();
}

StreamOutputFunction RetargetGetOutput() {
	RETARGET_TX_MTX_LOCK();

	StreamOutputFunction sof = sOutputFunc;

	RETARGET_TX_MTX_UNLOCK();

	return sof;
}

void RetargetSetFallbackOutput(StreamOutputFunction sof) {
	RETARGET_TX_MTX_LOCK();

	sFallbackOutputFunc = sof;

	RETARGET_TX_MTX_UNLOCK();
}

UART_HandleTypeDef* getPrintfUART() {
	return gHuart;
}

int _isatty(int fd) {
	if (fd >= STDIN_FILENO && fd <= STDERR_FILENO)
		return 1;

	errno = EBADF;
	return 0;
}

#define RETARGET_PRINTF_WRITE_BUF_SIZE (4095)
static char sWriteBuf[RETARGET_PRINTF_WRITE_BUF_SIZE + 1];

int _write(int fd, char *ptr, int len) {
	RETARGET_TX_MTX_LOCK();

	int retval = 0;

	if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
		// replace "\n" with "\r\n"
		size_t i_src = 0, i_dst = 0;
		char c, c_prev = '\0';
		while (i_src < len) {
			c = ptr[i_src++];

			if ((c == '\n') && (c_prev != '\r')) {
				sWriteBuf[i_dst++] = '\r';
				sWriteBuf[i_dst++] = '\n';
			} else {
				sWriteBuf[i_dst++] = c;
			}

			c_prev = ptr[i_src];
		}

		if (sOutputFunc != NULL) {
			retval = sOutputFunc(sWriteBuf, i_dst);
		} else {
			retval = sFallbackOutputFunc(sWriteBuf, i_dst);
		}

	} else {
		errno = EBADF;
		retval = -1;
	}

	RETARGET_TX_MTX_UNLOCK();

	return retval;
}

int _close(int fd) {
	if (fd >= STDIN_FILENO && fd <= STDERR_FILENO)
		return 0;

	errno = EBADF;
	return -1;
}

int _lseek(int fd, int ptr, int dir) {
	(void) fd;
	(void) ptr;
	(void) dir;

	errno = EBADF;
	return -1;
}

int _read(int fd, char *ptr, int len) {
	HAL_StatusTypeDef hstatus;

	if (fd == STDIN_FILENO) {
		hstatus = HAL_UART_Receive(gHuart, (uint8_t*) ptr, 1, HAL_MAX_DELAY);

		if (hstatus == HAL_OK) {
			return 1;
		} else {
			return EIO;
		}
	}

	errno = EBADF;
	return -1;
}

int _fstat(int fd, struct stat *st) {
	if (fd >= STDIN_FILENO && fd <= STDERR_FILENO) {
		st->st_mode = S_IFCHR;
		return 0;
	}

	errno = EBADF;
	return 0;
}

// --------------------

int output_usart(char *ptr, int len) {
	int retval;
	HAL_StatusTypeDef hstatus;

	hstatus = HAL_UART_Transmit(gHuart, (uint8_t*) ptr, len, HAL_MAX_DELAY);

	if (hstatus == HAL_OK) {
		retval = len;
	} else {
		retval = EIO;
	}

	return retval;
}

#endif //#if !defined(OS_USE_SEMIHOSTING)
