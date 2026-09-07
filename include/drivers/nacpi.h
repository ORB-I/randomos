#pragma once

#include <uacpi/tables.h>
#include <uacpi/acpi.h>

void init_acpi();
extern struct acpi_fadt* gfadt;