#pragma once
#include <core/std.h>
#include <drivers/virtio/virtio.h>
#include <drivers/virtio/virtqueue.h>

#define VIRTSND_CONTROLQ 0
#define VIRTSND_EVENTQ   1
#define VIRTSND_TXQ      2
#define VIRTSND_RXQ      3

enum {
    /* jack control request types */
    VIRTIO_SND_R_JACK_INFO = 1,
    VIRTIO_SND_R_JACK_REMAP,
    /* PCM control request types */
    VIRTIO_SND_R_PCM_INFO = 0x0100,
    VIRTIO_SND_R_PCM_SET_PARAMS,
    VIRTIO_SND_R_PCM_PREPARE,
    VIRTIO_SND_R_PCM_RELEASE,
    VIRTIO_SND_R_PCM_START,
    VIRTIO_SND_R_PCM_STOP,
    /* channel map control request types */
    VIRTIO_SND_R_CHMAP_INFO = 0x0200,
    /* jack event types */
    VIRTIO_SND_EVT_JACK_CONNECTED = 0x1000,
    VIRTIO_SND_EVT_JACK_DISCONNECTED,
    /* PCM event types */
    VIRTIO_SND_EVT_PCM_PERIOD_ELAPSED = 0x1100,
    VIRTIO_SND_EVT_PCM_XRUN,
    /* common status codes */
    VIRTIO_SND_S_OK = 0x8000,
    VIRTIO_SND_S_BAD_MSG,
    VIRTIO_SND_S_NOT_SUPP,
    VIRTIO_SND_S_IO_ERR
};

enum {
    VIRTIO_SND_D_OUTPUT = 0,
    VIRTIO_SND_D_INPUT
};

/* a common header */
struct __packed virtio_snd_hdr {
    u32 code;
};

/* an event notification */
struct __packed virtio_snd_event {
    struct virtio_snd_hdr hdr;
    u32 data;
};

struct __packed virtio_snd_query_info {
    struct virtio_snd_hdr hdr;
    u32 start_id;
    u32 count;
    u32 size;
};

struct __packed virtio_snd_info {
    u32 hda_fn_nid;
};

struct __packed virtio_snd_jack_hdr {
    struct virtio_snd_hdr hdr;
    u32 jack_id;
};

enum {
    VIRTIO_SND_JACK_F_REMAP = 0
};

struct __packed virtio_snd_jack_info {
    struct virtio_snd_info hdr;
    u32 features; /* 1 << VIRTIO_SND_JACK_F_XXX */
    u32 hda_reg_defconf;
    u32 hda_reg_caps;
    u8 connected;
    u8 padding[7];
};

struct __packed virtio_snd_jack_remap {
    struct virtio_snd_jack_hdr hdr; /* .code = VIRTIO_SND_R_JACK_REMAP */
    u32 association;
    u32 sequence;
};

struct __packed virtio_snd_pcm_hdr {
    struct virtio_snd_hdr hdr;
    u32 stream_id;
};

/* supported PCM stream features */
enum {
    VIRTIO_SND_PCM_F_SHMEM_HOST = 0,
    VIRTIO_SND_PCM_F_SHMEM_GUEST,
    VIRTIO_SND_PCM_F_MSG_POLLING,
    VIRTIO_SND_PCM_F_EVT_SHMEM_PERIODS,
    VIRTIO_SND_PCM_F_EVT_XRUNS
};

/* supported PCM sample formats */
enum {
    /* analog formats (width / physical width) */
    VIRTIO_SND_PCM_FMT_IMA_ADPCM = 0, /* 4 / 4 bits */
    VIRTIO_SND_PCM_FMT_MU_LAW, /* 8 / 8 bits */
    VIRTIO_SND_PCM_FMT_A_LAW, /* 8 / 8 bits */
    VIRTIO_SND_PCM_FMT_S8, /* 8 / 8 bits */
    VIRTIO_SND_PCM_FMT_U8, /* 8 / 8 bits */
    VIRTIO_SND_PCM_FMT_S16, /* 16 / 16 bits */
    VIRTIO_SND_PCM_FMT_U16, /* 16 / 16 bits */
    VIRTIO_SND_PCM_FMT_S18_3, /* 18 / 24 bits */
    VIRTIO_SND_PCM_FMT_U18_3, /* 18 / 24 bits */
    VIRTIO_SND_PCM_FMT_S20_3, /* 20 / 24 bits */
    VIRTIO_SND_PCM_FMT_U20_3, /* 20 / 24 bits */
    VIRTIO_SND_PCM_FMT_S24_3, /* 24 / 24 bits */
    VIRTIO_SND_PCM_FMT_U24_3, /* 24 / 24 bits */
    VIRTIO_SND_PCM_FMT_S20, /* 20 / 32 bits */
    VIRTIO_SND_PCM_FMT_U20, /* 20 / 32 bits */
    VIRTIO_SND_PCM_FMT_S24, /* 24 / 32 bits */
    VIRTIO_SND_PCM_FMT_U24, /* 24 / 32 bits */
    VIRTIO_SND_PCM_FMT_S32, /* 32 / 32 bits */
    VIRTIO_SND_PCM_FMT_U32, /* 32 / 32 bits */
    VIRTIO_SND_PCM_FMT_FLOAT, /* 32 / 32 bits */
    VIRTIO_SND_PCM_FMT_FLOAT64, /* 64 / 64 bits */
    /* digital formats (width / physical width) */
    VIRTIO_SND_PCM_FMT_DSD_U8, /* 8 / 8 bits */
    VIRTIO_SND_PCM_FMT_DSD_U16, /* 16 / 16 bits */
    VIRTIO_SND_PCM_FMT_DSD_U32, /* 32 / 32 bits */
    VIRTIO_SND_PCM_FMT_IEC958_SUBFRAME /* 32 / 32 bits */
};

/* supported PCM frame rates */
enum {
    VIRTIO_SND_PCM_RATE_5512 = 0,
    VIRTIO_SND_PCM_RATE_8000,
    VIRTIO_SND_PCM_RATE_11025,
    VIRTIO_SND_PCM_RATE_16000,
    VIRTIO_SND_PCM_RATE_22050,
    VIRTIO_SND_PCM_RATE_32000,
    VIRTIO_SND_PCM_RATE_44100,
    VIRTIO_SND_PCM_RATE_48000,
    VIRTIO_SND_PCM_RATE_64000,
    VIRTIO_SND_PCM_RATE_88200,
    VIRTIO_SND_PCM_RATE_96000,
    VIRTIO_SND_PCM_RATE_176400,
    VIRTIO_SND_PCM_RATE_192000,
    VIRTIO_SND_PCM_RATE_384000
};

struct __packed virtio_snd_pcm_info {
    struct virtio_snd_info hdr;
    u32 features; /* 1 << VIRTIO_SND_PCM_F_XXX */
    u64 formats; /* 1 << VIRTIO_SND_PCM_FMT_XXX */
    u64 rates; /* 1 << VIRTIO_SND_PCM_RATE_XXX */
    u8 direction;
    u8 channels_min;
    u8 channels_max;
    u8 padding[5];
};

struct __packed virtio_snd_pcm_set_params {
    struct virtio_snd_pcm_hdr hdr; /* .code = VIRTIO_SND_R_PCM_SET_PARAMS */
    u32 buffer_bytes;
    u32 period_bytes;
    u32 features; /* 1 << VIRTIO_SND_PCM_F_XXX */
    u8 channels;
    u8 format;
    u8 rate;
    u8 padding;
};

/* an I/O header */
struct __packed virtio_snd_pcm_xfer {
    u32 stream_id;
};

/* an I/O status */
struct __packed virtio_snd_pcm_status {
    u32 status;
    u32 latency_bytes;
};

/* standard channel position definition */
enum {
    VIRTIO_SND_CHMAP_NONE = 0, /* undefined */
    VIRTIO_SND_CHMAP_NA, /* silent */
    VIRTIO_SND_CHMAP_MONO, /* mono stream */
    VIRTIO_SND_CHMAP_FL, /* front left */
    VIRTIO_SND_CHMAP_FR, /* front right */
    VIRTIO_SND_CHMAP_RL, /* rear left */
    VIRTIO_SND_CHMAP_RR, /* rear right */
    VIRTIO_SND_CHMAP_FC, /* front center */
    VIRTIO_SND_CHMAP_LFE, /* low frequency (LFE) */
    VIRTIO_SND_CHMAP_SL, /* side left */
    VIRTIO_SND_CHMAP_SR, /* side right */
    VIRTIO_SND_CHMAP_RC, /* rear center */
    VIRTIO_SND_CHMAP_FLC, /* front left center */
    VIRTIO_SND_CHMAP_FRC, /* front right center */
    VIRTIO_SND_CHMAP_RLC, /* rear left center */
    VIRTIO_SND_CHMAP_RRC, /* rear right center */
    VIRTIO_SND_CHMAP_FLW, /* front left wide */
    VIRTIO_SND_CHMAP_FRW, /* front right wide */
    VIRTIO_SND_CHMAP_FLH, /* front left high */
    VIRTIO_SND_CHMAP_FCH, /* front center high */
    VIRTIO_SND_CHMAP_FRH, /* front right high */
    VIRTIO_SND_CHMAP_TC, /* top center */
    VIRTIO_SND_CHMAP_TFL, /* top front left */
    VIRTIO_SND_CHMAP_TFR, /* top front right */
    VIRTIO_SND_CHMAP_TFC, /* top front center */
    VIRTIO_SND_CHMAP_TRL, /* top rear left */
    VIRTIO_SND_CHMAP_TRR, /* top rear right */
    VIRTIO_SND_CHMAP_TRC, /* top rear center */
    VIRTIO_SND_CHMAP_TFLC, /* top front left center */
    VIRTIO_SND_CHMAP_TFRC, /* top front right center */
    VIRTIO_SND_CHMAP_TSL, /* top side left */
    VIRTIO_SND_CHMAP_TSR, /* top side right */
    VIRTIO_SND_CHMAP_LLFE, /* left LFE */
    VIRTIO_SND_CHMAP_RLFE, /* right LFE */
    VIRTIO_SND_CHMAP_BC, /* bottom center */
    VIRTIO_SND_CHMAP_BLC, /* bottom left center */
    VIRTIO_SND_CHMAP_BRC /* bottom right center */
};

/* maximum possible number of channels */
#define VIRTIO_SND_CHMAP_MAX_SIZE 18
struct __packed virtio_snd_chmap_info {
    struct virtio_snd_info hdr;
    u8 direction;
    u8 channels;
    u8 positions[VIRTIO_SND_CHMAP_MAX_SIZE];
};

typedef struct {
    virtio_dev_t dev;
    virtqueue_t queues[4];
} virtio_snd_dev_t;