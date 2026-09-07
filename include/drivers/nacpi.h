#pragma once

#include <uacpi/tables.h>
#include <uacpi/acpi.h>

void init_acpi();
void init_acpi_ns();
void uacpi_drain_work();
extern struct acpi_fadt* gfadt;