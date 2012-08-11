#include <config.h>
#include "if1_fdc.h"

#include "compat.h"
#include "module.h"
#include "periph.h"
#include "settings.h"

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

void if1_fdc_activate(void)
{
	dbg("called");
}

static periph_port_t if1_fdc_ports[] = {
	{ 0, 0, NULL, NULL, },
};

static periph_t if1_fdc_periph = {
	.option			= &settings_current.interface1_fdc,
	.ports			= if1_fdc_ports,
	.activate		= if1_fdc_activate,
};

static module_info_t if1_fdc_module = {
	.reset			= if1_fdc_reset,
	.romcs			= if1_fdc_romcs,
};

void if1_fdc_init(void)
{
	module_register(&if1_fdc_module);
	periph_register(PERIPH_TYPE_INTERFACE1_FDC, &if1_fdc_periph);
}
