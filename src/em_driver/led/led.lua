return {
    -- 基本信息
    name = "led",
    dir = "led", -- 相对于 em_driver 的路径

    -- 驱动源码逻辑
    src = { "led.c" },
    
	port_config = {
		mode = {
			default = "extern",
			values = {
				extern = "LIBCA_LED_PORT_MODE=1",
				dynamic= "LIBCA_LED_PORT_MODE=2"
			}
		}
	}
}
