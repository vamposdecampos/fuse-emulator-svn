#include <config.h>

#include "module.h"

#if 0
#define dbg(fmt, args...) fprintf(stderr, "%s:%d: " fmt "\n", __func__, __LINE__, ## args)
#else
#define dbg(x...)
#endif

void if1_fdc_reset(int hard)
{
	dbg("called");
}

void if1_fdc_romcs(void)
{
	dbg("called");
}

static module_info_t if1_fdc_module = {
	.reset			= if1_fdc_reset,
	.romcs			= if1_fdc_romcs,
};

void if1_fdc_init(void)
{
	module_register(&if1_fdc_module);
}
