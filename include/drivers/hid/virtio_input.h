#pragma once

#include <core/std.h>

int virtio_input_init(void);
void virtio_input_poll(void);
bool virtio_input_kb_available(void);
bool virtio_input_ptr_available(void);
