#include <drivers/virtio/virtio.h>
#include <drivers/virtio/virtqueue.h>
#include <core/errno.h>
#include <core/kprint.h>
#include <drivers/sound/virtio_snd.h>

static int vsnd_query_jacks(virtio_snd_dev_t* dev) {
    u32 jacks = virtio_read_config32(&dev->dev, 0);
    struct virtio_snd_query_info jack_query = {
        {VIRTIO_SND_R_JACK_INFO},
        0,
        0,
        sizeof(struct virtio_snd_jack_info)
    };

    

    struct {
        struct virtio_snd_hdr hdr;
        struct virtio_snd_jack_info info[5];
    } resp;

    for (u32 start_id = 0; start_id < jacks; start_id += 5) {
        u32 count = jacks - start_id;
        if (count > 5) count = 5;

        jack_query.start_id = start_id;
        jack_query.count    = count;

        int d0 = virtqueue_alloc_desc(&dev->queues[VIRTSND_CONTROLQ]);
        int d1 = virtqueue_alloc_desc(&dev->queues[VIRTSND_CONTROLQ]);
    }
}

int virtio_snd_init(virtio_snd_dev_t* dev) {
    int ret = 0;
    if ((ret = virtio_find_pci_device(VIRTIO_DEV_SND, &dev->dev, 0)) < 0) {
        return ret;
    }

    virtio_reset(&dev->dev);
    virtio_set_status(&dev->dev, VIRTIO_STATUS_ACKNOWLEDGE);
    virtio_add_status(&dev->dev, VIRTIO_STATUS_DRIVER);

    u64 features = virtio_get_features64(&dev->dev);
    u64 accept = features & VIRTIO_F_VERSION_1;
    if (!(accept & VIRTIO_F_VERSION_1)) {
        kprint("virtio sound device does not support version 1 feature\n");
        virtio_set_status(&dev->dev, VIRTIO_STATUS_FAILED);
        return -EINVAL;
    }
    virtio_set_features64(&dev->dev, accept);
    virtio_add_status(&dev->dev, VIRTIO_STATUS_FEATURES_OK);
    if (!(virtio_get_status(&dev->dev) & VIRTIO_STATUS_FEATURES_OK)) {
        kprint("feature negotiation failed for virtio sound device\n");
        virtio_set_status(&dev->dev, VIRTIO_STATUS_FAILED);
        return -EINVAL;
    }

    if (virtqueue_init(&dev->dev, VIRTSND_CONTROLQ, &dev->queues[VIRTSND_CONTROLQ]) < 0) {
        virtio_set_status(&dev->dev, VIRTIO_STATUS_FAILED);
        return -EINVAL;
    }

    if (virtqueue_init(&dev->dev, VIRTSND_EVENTQ, &dev->queues[VIRTSND_EVENTQ]) < 0) {
        virtio_set_status(&dev->dev, VIRTIO_STATUS_FAILED);
        return -EINVAL;
    }

    if (virtqueue_init(&dev->dev, VIRTSND_TXQ, &dev->queues[VIRTSND_TXQ]) < 0) {
        virtio_set_status(&dev->dev, VIRTIO_STATUS_FAILED);
        return -EINVAL;
    }

    if (virtqueue_init(&dev->dev, VIRTSND_RXQ, &dev->queues[VIRTSND_RXQ]) < 0) {
        virtio_set_status(&dev->dev, VIRTIO_STATUS_FAILED);
        return -EINVAL;
    }


}