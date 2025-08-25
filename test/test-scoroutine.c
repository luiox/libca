#include "component/scoroutine.h"
#include <stdio.h>

sc_begin(task1)
for (;;) {
    printf("t1\n");
    sc_yield();
    printf("t2\n");
    sc_yield();
    // sc_delay_ms(1000);
    printf("t3\n");
    sc_yield();
}
sc_end()

sc_begin(task2)
for (;;) {
    printf("t6\n");
    sc_yield();
    printf("t7\n");
    sc_yield();
    // sc_delay_ms(1000);
    printf("t8\n");
    sc_yield();
}
sc_end()

int main()
{
    sc_create_coroutine(task1);
    sc_create_coroutine(task2);

    sc_scheduler_start();

    return 0;
}