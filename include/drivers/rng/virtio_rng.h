#pragma once
#include <core/std.h>
#include <drivers/virtio/virtio.h>

int virtio_rng_init();
usize virtio_rng_read(u8* buf, usize len);
bool virtio_rng_available();
