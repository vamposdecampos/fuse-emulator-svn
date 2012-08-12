#ifndef FUSE_IF1_FDC_H
#define FUSE_IF1_FDC_H

#include "peripherals/disk/fdd.h"

typedef enum if1_drive_number {
	IF1_DRIVE_1 = 0,
	IF1_DRIVE_2,
} if1_drive_number;

extern int if1_fdc_available;

void if1_fdc_init(void);

int if1_fdc_insert(if1_drive_number which, const char *filename, int autoload);
int if1_fdc_eject(if1_drive_number which);
int if1_fdc_save(if1_drive_number which, int saveas);
int if1_fdc_disk_write(if1_drive_number which, const char *filename);
int if1_fdc_flip(if1_drive_number which, int flip);
int if1_fdc_writeprotect(if1_drive_number which, int wp);
fdd_t *if1_fdc_get_fdd(if1_drive_number which);

#endif				/* #ifndef FUSE_IF1_FDC_H */
