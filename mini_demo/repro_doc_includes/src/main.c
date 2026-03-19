#ifndef LIB_led
#error "LIB_led define should be injected by my_add_libs"
#endif

void led_impl(void);

int main(void)
{
    led_impl();
    return 0;
}
