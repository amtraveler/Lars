#include "host_info.h"
#include "main_server.h"

void host_info::set_overload()
{
	vsucc = 0;
	verr = lb_config.init_err_cnt;
	rsucc = 0;
	rerr = 0;
	contin_succ = 0;
	contin_err = 0;
	overload = true;

	overload_ts = time(NULL);
}
void host_info::set_idle()
{
	vsucc = lb_config.init_succ_cnt;;
	verr = 0;
	rsucc = 0;
	rerr = 0;
	contin_succ = 0;
	contin_err = 0;
	overload = false;

	idle_ts = time(NULL);
}
