#include "main.h"
#include "test_harness.h"

int main(void)
{
	HAL_Init();
	SystemClock_Config();
	MX_GPIO_Init();
	MX_USART2_UART_Init();

	test_suite_run();

	while (1) {
	}
}
