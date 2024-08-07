#include <pjmedia.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <stdlib.h>
#include <stdio.h>

#define THIS_FILE   "confsample.c"

#define RECORDER    1

#define CLOCK_RATE      44100
#define NSAMPLES        (CLOCK_RATE * 20 / 1000)
#define NCHANNELS       1
#define NBITS           16

static int app_perror(const char* sender, const char* title, pj_status_t status)
{
    char errmsg[PJ_ERR_MSG_SIZE];

    pj_strerror(status, errmsg, sizeof(errmsg));

    PJ_LOG(3, (sender, "%s: %s [code=%d]", title, errmsg, status));
    return 1;
}

int main(int argc, char* argv[])
{
    const char* files[] = {
        "01_Congas_mono.wav",
        "02_Bass_mono.wav",
        "03_UkeleleMic_mono.wav",
        "04_UkeleleDI_mono.wav",
        "05_Viola_mono.wav",
        "06_Cello_mono.wav",
        "07_LeadVox_mono.wav",
        "08_BackingVox_mono.wav"
    };
    const int file_count = sizeof(files) / sizeof(files[0]);
    unsigned  recorder_slot =0;
    unsigned file_slots[file_count] = {0};
    int clock_rate = CLOCK_RATE;
    int channel_count = NCHANNELS;
    int samples_per_frame = NSAMPLES;
    int bits_per_sample = NBITS;

    pj_caching_pool cp;
    pjmedia_endpt* med_endpt;
    pj_pool_t* pool;
    pjmedia_conf* conf;
    pjmedia_master_port* master_port;
    pjmedia_port* file_ports[file_count];

    int i, port_count;
    pjmedia_port* rec_port = NULL;

    pj_status_t status;

    /* Initialize pjlib */
    status = pj_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 0);

    status = pjmedia_endpt_create(&cp.factory, NULL, 1, &med_endpt);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    pool = pj_pool_create(&cp.factory, "wav", 4000, 4000, NULL);

    port_count = file_count + 1 + RECORDER;

    status = pjmedia_conf_create(pool, port_count, clock_rate, channel_count, samples_per_frame, bits_per_sample, PJMEDIA_CONF_NO_DEVICE, &conf);
    if (status != PJ_SUCCESS) {
        app_perror(THIS_FILE, "Unable to create conference bridge", status);
        return 1;
    }

#if RECORDER
    status = pjmedia_wav_writer_port_create(pool, "confrecord.wav", clock_rate, channel_count, samples_per_frame, bits_per_sample, 0, 0, &rec_port);
    if (status != PJ_SUCCESS) {
        app_perror(THIS_FILE, "Unable to create WAV writer", status);
        return 1;
    }
    
    status = pjmedia_conf_add_port(conf, pool, rec_port, NULL, &recorder_slot );
    if (status != PJ_SUCCESS) {
        app_perror(THIS_FILE, "Unable to add WAV writer port", status);
        return 1;
    }
#endif

   /* file_ports = static_cast<pjmedia_port**>(pj_pool_alloc(pool, file_count * sizeof(pjmedia_port*)));*/

    for (i = 0; i < file_count; ++i) {
        status = pjmedia_wav_player_port_create(pool, files[i], 0, 0, 0, &file_ports[i]);
        if (status != PJ_SUCCESS) {
            char title[80];
            pj_ansi_snprintf(title, sizeof(title), "Unable to use %s", files[i]);
            app_perror(THIS_FILE, title, status);
            return 1;
        }

        status = pjmedia_conf_add_port(conf, pool, file_ports[i], NULL, &file_slots[i]);
        if (status != PJ_SUCCESS) {
            app_perror(THIS_FILE, "Unable to add file port to conference bridge", status);
            return 1;
        }
    }

#if RECORDER
    /* Connect each file port to the recorder port */
    for (i = 0; i < file_count; ++i) {
        status = pjmedia_conf_connect_port(conf, file_slots[i], recorder_slot, 0);
        if (status != PJ_SUCCESS) {
            app_perror(THIS_FILE, "Unable to connect file port to recorder port", status);
            return 1;
        }
    }
#endif
    /* Create null port */
    pjmedia_port* null_port;
    pjmedia_port* conf_port;
    status = pjmedia_null_port_create(pool, clock_rate, channel_count, samples_per_frame, bits_per_sample, &null_port);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    conf_port = pjmedia_conf_get_master_port(conf);

    /* Create master port */
    status = pjmedia_master_port_create(pool, null_port, conf_port, 0, &master_port);

    if (status != PJ_SUCCESS) {
        app_perror(THIS_FILE, "Unable to create master port", status);
        return 1;
    }

    status = pjmedia_master_port_start(master_port);
    if (status != PJ_SUCCESS) {
        app_perror(THIS_FILE, "Unable to start master port", status);
        return 1;
    }

    PJ_LOG(3, (THIS_FILE, "Running conference bridge with %d file(s) and recorder", file_count));

    pj_thread_sleep(10000);

    pjmedia_port_destroy(rec_port);
    pjmedia_master_port_stop(master_port);
    pjmedia_master_port_destroy(master_port, PJ_TRUE);
    pjmedia_conf_destroy(conf);
    pj_pool_release(pool);

    pjmedia_endpt_destroy(med_endpt);
    pj_caching_pool_destroy(&cp);
    pj_shutdown();

    return 0;
}
