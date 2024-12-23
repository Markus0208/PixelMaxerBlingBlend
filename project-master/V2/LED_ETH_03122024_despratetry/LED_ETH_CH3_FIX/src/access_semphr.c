#include "access_semphr.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

SemaphoreHandle_t ptr_binary_semphr = NULL;
uint32_t NEW_DATA_FLAG = 0;
uint32_t *global_received_array = NULL;
uint8_t *recv_buffer = NULL;

void initSemphr(void){

	if (global_received_array != NULL) {
		free(global_received_array);
	}

	global_received_array = malloc(4096*sizeof(uint32_t));

	if (global_received_array == NULL) {
		xil_printf("Memory allocation failed for global buffer\n");
	}
	recv_buffer = malloc(4096);

	ptr_binary_semphr = xSemaphoreCreateBinary();

	if (ptr_binary_semphr == NULL){
		xil_printf("Semaphore could not be created");
	}
	xSemaphoreGive(ptr_binary_semphr);

}
