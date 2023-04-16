// All credit to Carmine Noviello for this code
// https://github.com/cnoviello/mastering-stm32/blob/master/nucleo-f030R8/system/include/retarget/retarget.h

#ifndef _RETARGET_H__
#define _RETARGET_H__

#include <stm32h7xx_hal.h>
#include <sys/stat.h>

typedef int(*StreamOutputFunction)(char*,int);

void RetargetInit(UART_HandleTypeDef *huart);
void RetargetSetOutput(StreamOutputFunction sof);
StreamOutputFunction RetargetGetOutput();
void RetargetSetFallbackOutput(StreamOutputFunction sof);

UART_HandleTypeDef* getPrintfUART();

int _isatty(int fd);
int _write(int fd, char* ptr, int len);
int _close(int fd);
int _lseek(int fd, int ptr, int dir);
int _read(int fd, char* ptr, int len);
int _fstat(int fd, struct stat* st);

// ------------------

int output_usart(char *ptr, int len);

#endif //#ifndef _RETARGET_H__
