#include <config.h>
#include "if1_fdc.h"

#include "compat.h"
#include "if1.h"
#include "memory.h"
#include "module.h"
#include "periph.h"
#include "peripherals/disk/fdd.h"
#include "peripherals/disk/upd_fdc.h"
#include "settings.h"

#define dbg(fmt, args...) fprintf(stderr, "%s:%d: " fmt "\n", __func__, __LINE__, ## args)

#define IF1_FDC_RAM_SIZE	1024	/* bytes */
#define IF1_NUM_DRIVES 2

int if1_fdc_available;

static upd_fdc *if1_fdc;
static upd_fdc_drive if1_drives[IF1_NUM_DRIVES];

static int if1_fdc_memory_source;
static memory_page if1_fdc_memory_map_romcs[MEMORY_PAGES_IN_8K];

void if1_fdc_reset(int hard)
{
	dbg("called");

	if1_fdc_available = 0;
	if (!periph_is_active(PERIPH_TYPE_INTERFACE1_FDC))
		return;

	if1_fdc_available = 1;
	dbg("available");
}

void if1_fdc_romcs(void)
{
	dbg("called; if1_active=%d", if1_active);
	if (!if1_active)
		return;
	memory_map_romcs_8k(0x2000, if1_fdc_memory_map_romcs);
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
	int i;
	libspectrum_byte *ram;
	upd_fdc_drive *d;

	if1_fdc_memory_source = memory_source_register("If1 RAM");
	ram = memory_pool_allocate_persistent(IF1_FDC_RAM_SIZE, 1);

	for (i = 0; i < MEMORY_PAGES_IN_8K; i++) {
		libspectrum_word addr = i << MEMORY_PAGE_SIZE_LOGARITHM;
		memory_page *page = &if1_fdc_memory_map_romcs[i];

		page->source = if1_fdc_memory_source;
		page->page = ram + (addr % IF1_FDC_RAM_SIZE);
		page->writable = !!(addr & 0x800);
	}

	if1_fdc = upd_fdc_alloc_fdc(UPD765A, UPD_CLOCK_8MHZ);
	if1_fdc->drive[0] = &if1_drives[0];
	if1_fdc->drive[1] = &if1_drives[1];
	if1_fdc->drive[2] = &if1_drives[0];
	if1_fdc->drive[3] = &if1_drives[1];

	fdd_init(&if1_drives[0].fdd, FDD_SHUGART, &fdd_params[4], 0);
	fdd_init(&if1_drives[1].fdd, FDD_SHUGART, NULL, 0);	/* drive geometry 'autodetect' */
	if1_fdc->set_intrq = NULL;
	if1_fdc->reset_intrq = NULL;
	if1_fdc->set_datarq = NULL;
	if1_fdc->reset_datarq = NULL;

	module_register(&if1_fdc_module);
	periph_register(PERIPH_TYPE_INTERFACE1_FDC, &if1_fdc_periph);
}
