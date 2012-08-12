#include <config.h>
#include "if1_fdc.h"

#include "compat.h"
#include "if1.h"
#include "memory.h"
#include "module.h"
#include "options.h"
#include "periph.h"
#include "peripherals/disk/fdd.h"
#include "peripherals/disk/upd_fdc.h"
#include "settings.h"
#include "ui/ui.h"
#include "utils.h"

#if 0
#define dbg(fmt, args...) fprintf(stderr, "%s:%d: " fmt "\n", __func__, __LINE__, ## args)
#define dbgp(x...) dbg(x)
#else
#define dbg(x...)
#define dbgp(x...)
#endif

#define DISK_TRY_MERGE(heads) ( option_enumerate_diskoptions_disk_try_merge() == 2 || \
				( option_enumerate_diskoptions_disk_try_merge() == 1 && heads == 1 ) )

#define IF1_FDC_RAM_SIZE	1024	/* bytes */
#define IF1_NUM_DRIVES 2

int if1_fdc_available;

static upd_fdc *if1_fdc;
static upd_fdc_drive if1_drives[IF1_NUM_DRIVES];
static int deselect_event;

static int if1_fdc_memory_source;
static memory_page if1_fdc_memory_map_romcs[MEMORY_PAGES_IN_8K];

static void update_menu(void) 
{
	const fdd_params_t *dt;

	/* We can eject disks only if they are currently present */
	ui_menu_activate(UI_MENU_ITEM_MEDIA_DISK_IF1_FDC_A_EJECT, if1_drives[0].fdd.loaded);
	ui_menu_activate(UI_MENU_ITEM_MEDIA_DISK_IF1_FDC_A_FLIP_SET, !if1_drives[0].fdd.upsidedown);
	ui_menu_activate(UI_MENU_ITEM_MEDIA_DISK_IF1_FDC_A_WP_SET, !if1_drives[0].fdd.wrprot);

	dt = &fdd_params[option_enumerate_diskoptions_drive_if1_fdc_b_type()];
	ui_menu_activate(UI_MENU_ITEM_MEDIA_DISK_IF1_FDC_B, dt->enabled);
	ui_menu_activate(UI_MENU_ITEM_MEDIA_DISK_IF1_FDC_B_EJECT, if1_drives[1].fdd.loaded);
	ui_menu_activate(UI_MENU_ITEM_MEDIA_DISK_IF1_FDC_B_FLIP_SET, !if1_drives[1].fdd.upsidedown);
	ui_menu_activate(UI_MENU_ITEM_MEDIA_DISK_IF1_FDC_B_WP_SET, !if1_drives[1].fdd.wrprot);
}

int if1_fdc_insert(if1_drive_number which, const char *filename, int autoload)
{
	int error;
	upd_fdc_drive *d;
	const fdd_params_t *dt;

	if (which >= IF1_NUM_DRIVES) {
		ui_error(UI_ERROR_ERROR, "if1_fdc_insert: unknown drive %d", which);
		fuse_abort();
	}

	d = &if1_drives[which];

	/* Eject any disk already in the drive */
	if (d->fdd.loaded) {
		/* Abort the insert if we want to keep the current disk */
		if (if1_fdc_eject(which))
			return 0;
	}

	if (filename) {
		error = disk_open(&d->disk, filename, 0, DISK_TRY_MERGE(d->fdd.fdd_heads));
		if (error != DISK_OK) {
			ui_error(UI_ERROR_ERROR, "Failed to open disk image: %s", disk_strerror(error));
			return 1;
		}
	} else {
		switch (which) {
		case 0:
			dt = &fdd_params[option_enumerate_diskoptions_drive_if1_fdc_a_type() + 1];	/* +1 => there is no `Disabled' */
			break;
		case 1:
		default:
			dt = &fdd_params[option_enumerate_diskoptions_drive_if1_fdc_b_type()];
			break;
		}
		error = disk_new(&d->disk, dt->heads, dt->cylinders, DISK_DENS_AUTO, DISK_UDI);
		if (error != DISK_OK) {
			ui_error(UI_ERROR_ERROR, "Failed to create disk image: %s", disk_strerror(error));
			return 1;
		}
	}

	fdd_load(&d->fdd, &d->disk, 0);
	update_menu();

	if (filename && autoload) {
		/* XXX */
	}

	return 0;
}

int if1_fdc_eject(if1_drive_number which)
{
	upd_fdc_drive *d;

	if (which >= IF1_NUM_DRIVES)
		return 1;

	d = &if1_drives[which];

	if (d->disk.type == DISK_TYPE_NONE)
		return 0;

	if (d->disk.dirty) {
		ui_confirm_save_t confirm = ui_confirm_save(
			"Disk in drive %d has been modified.\n"
			"Do you want to save it?", which + 1);

		switch (confirm) {
		case UI_CONFIRM_SAVE_SAVE:
			if (if1_fdc_save(which, 0))
				return 1;	/* first save it... */
			break;
		case UI_CONFIRM_SAVE_DONTSAVE:
			break;
		case UI_CONFIRM_SAVE_CANCEL:
			return 1;
		}
	}

	fdd_unload(&d->fdd);
	disk_close(&d->disk);
	update_menu();

	return 0;
}

int if1_fdc_save(if1_drive_number which, int saveas)
{
	upd_fdc_drive *d;

	if (which >= IF1_NUM_DRIVES)
		return 1;

	d = &if1_drives[which];

	if (d->disk.type == DISK_TYPE_NONE)
		return 0;

	if (d->disk.filename == NULL)
		saveas = 1;
	if (ui_if1_fdc_disk_write(which, saveas))
		return 1;
	d->disk.dirty = 0;
	return 0;
}

int if1_fdc_flip(if1_drive_number which, int flip)
{
	upd_fdc_drive *d;

	if (which >= IF1_NUM_DRIVES)
		return 1;

	d = &if1_drives[which];
	if (!d->fdd.loaded)
		return 1;

	fdd_flip(&d->fdd, flip);
	update_menu();
	return 0;
}

int if1_fdc_writeprotect(if1_drive_number which, int wrprot)
{
	upd_fdc_drive *d;

	if (which >= IF1_NUM_DRIVES)
		return 1;

	d = &if1_drives[which];

	if (!d->fdd.loaded)
		return 1;

	fdd_wrprot(&d->fdd, wrprot);
	update_menu();
	return 0;
}

int if1_fdc_disk_write(if1_drive_number which, const char *filename)
{
	upd_fdc_drive *d = &if1_drives[which];
	int error;

	d->disk.type = DISK_TYPE_NONE;
	if (filename == NULL)
		filename = d->disk.filename;	/* write over original file */
	error = disk_write(&d->disk, filename);

	if (error != DISK_OK) {
		ui_error(UI_ERROR_ERROR, "couldn't write '%s' file: %s", filename, disk_strerror(error));
		return 1;
	}

	if (d->disk.filename && strcmp(filename, d->disk.filename)) {
		free(d->disk.filename);
		d->disk.filename = utils_safe_strdup(filename);
	}
	return 0;
}

fdd_t *if1_fdc_get_fdd(if1_drive_number which)
{
	return &if1_drives[which].fdd;
}



void if1_fdc_reset(int hard)
{
	const fdd_params_t *dt;

	dbg("called");

	if1_fdc_available = 0;
	if (!periph_is_active(PERIPH_TYPE_INTERFACE1_FDC))
		return;

	upd_fdc_master_reset(if1_fdc);

	dt = &fdd_params[option_enumerate_diskoptions_drive_if1_fdc_a_type() + 1];	/* +1 => there is no `Disabled' */
	fdd_init(&if1_drives[0].fdd, FDD_SHUGART, dt, 1);

	dt = &fdd_params[option_enumerate_diskoptions_drive_if1_fdc_b_type()];
	fdd_init(&if1_drives[1].fdd, dt->enabled ? FDD_SHUGART : FDD_TYPE_NONE, dt, 1);

	update_menu();

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


libspectrum_byte if1_fdc_status(libspectrum_word port GCC_UNUSED, int *attached)
{
	libspectrum_byte ret;

	*attached = 1;
	ret = upd_fdc_read_status(if1_fdc);
	dbgp("port 0x%02x --> 0x%02x", port & 0xff, ret);
	return ret;
}

libspectrum_byte if1_fdc_read(libspectrum_word port GCC_UNUSED, int *attached)
{
	libspectrum_byte ret;

	*attached = 1;
	ret = upd_fdc_read_data(if1_fdc);
	dbgp("port 0x%02x --> 0x%02x", port & 0xff, ret);
	return ret;
}

void if1_fdc_write(libspectrum_word port GCC_UNUSED, libspectrum_byte data)
{
	dbgp("port 0x%02x <-- 0x%02x", port & 0xff, data);
	upd_fdc_write_data(if1_fdc, data);
}

libspectrum_byte if1_fdc_sel_read(libspectrum_word port GCC_UNUSED, int *attached)
{
	libspectrum_byte ret;
	int i;

	*attached = 1;
	ret = 0xff;
	for (i = 0; i < IF1_NUM_DRIVES; i++)
		if (if1_drives[i].fdd.motoron)
			ret &= ~1;
	dbgp("port 0x%02x --> 0x%02x", port & 0xff, ret);
	return ret;
}

void if1_fdc_sel_write(libspectrum_word port GCC_UNUSED, libspectrum_byte data)
{
	libspectrum_byte armed;

	dbg("port 0x%02x <-- 0x%02x", port & 0xff, data);

	if (!(data & 0x10)) {
		dbg("FDC reset");
		upd_fdc_master_reset(if1_fdc);
	}
	upd_fdc_tc(if1_fdc, data & 1);

	event_remove_type(deselect_event);

	armed = data & 0x08;
	if (armed) {
		fdd_select(&if1_drives[0].fdd, armed && (data & 0x02));
		fdd_select(&if1_drives[1].fdd, armed && (data & 0x04));
		fdd_motoron(&if1_drives[0].fdd, armed && (data & 0x02));
		fdd_motoron(&if1_drives[1].fdd, armed && (data & 0x04));
	} else {
		/* The IF1 contains a 555 timer configured as a retriggerable
		 * monostable.  It uses a 470 kOhm resistor and 3.3 uF capacitor.
		 * This should give a delay of approximately 1.7 seconds.
		 */
		event_add(tstates +
			17 * machine_current->timings.processor_speed / 10,
			deselect_event);
	}

	ui_statusbar_update(UI_STATUSBAR_ITEM_DISK, armed
		? UI_STATUSBAR_STATE_ACTIVE
		: UI_STATUSBAR_STATE_INACTIVE);
}

static void if1_deselect_cb(libspectrum_dword last_tstates, int type, void *user_data)
{
	dbg("called");
	fdd_select(&if1_drives[0].fdd, 0);
	fdd_select(&if1_drives[1].fdd, 0);
	fdd_motoron(&if1_drives[0].fdd, 0);
	fdd_motoron(&if1_drives[1].fdd, 0);
}

static periph_port_t if1_fdc_ports[] = {
	{ 0x00fd, 0x0005, if1_fdc_sel_read, if1_fdc_sel_write },
	{ 0x00ff, 0x0085, if1_fdc_status, NULL },
	{ 0x00ff, 0x0087, if1_fdc_read, if1_fdc_write },
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

	deselect_event = event_register(if1_deselect_cb, "IF1 FDC deselect");

	module_register(&if1_fdc_module);
	periph_register(PERIPH_TYPE_INTERFACE1_FDC, &if1_fdc_periph);
	update_menu();
}
