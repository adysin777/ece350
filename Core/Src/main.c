#include "main.h"
#include "test_harness.h"

/* Uncomment and set 0-12 to run one test only */
/* #define RUN_SINGLE_TEST 12 */

int main(void)
{
	HAL_Init();
	SystemClock_Config();
	MX_GPIO_Init();
	MX_USART2_UART_Init();

#ifdef RUN_SINGLE_TEST
	test_suite_run_one(RUN_SINGLE_TEST);
#else
	test_suite_run();
#endif

	while (1) {
	}
}
