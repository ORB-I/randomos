#pragma once

#include <core/std.h>

int virtio_input_init(void);
bool virtio_input_available(void);
void virtio_input_poll(void);