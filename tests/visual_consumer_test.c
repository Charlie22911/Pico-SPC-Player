#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app/visual_consumer.h"

static aram_activity_t activity;
static aram_data_mailbox_t data;
static aram_view_t view;
static visual_consumer_t consumer;

static void reset(void) {
    aram_activity_init(&activity, 7u);
    aram_data_init(&data);
    aram_view_init(&view);
    visual_consumer_init(&consumer);
}

static void publish_data(uint32_t generation, uint32_t request) {
    uint8_t *pixels = aram_data_begin_write(&data);
    assert(pixels != NULL);
    memset(pixels, 0x53, ARAM_DATA_PIXELS_BYTES);
    aram_data_publish(&data, request, generation, 10u);
}

int main(void) {
    reset();
    publish_data(7u, 1u);
    /* A ready map can be consumed immediately, with no model timer involved. */
    assert(visual_consumer_poll(&consumer, &activity, &data, &view, 7u, 1u, true));
    assert(view.valid && view.kind == ARAM_VIEW_DATA && view.sequence == 1u);
    assert(view.pixels[0] == 0x53 && consumer.consumed == 1u);
    assert(!visual_consumer_poll(&consumer, &activity, &data, &view, 7u, 1u, true));
    assert(consumer.consumed == 1u && view.sequence == 1u);

    publish_data(6u, 1u); /* Old track. */
    assert(!visual_consumer_poll(&consumer, &activity, &data, &view, 7u, 1u, true));
    publish_data(7u, 1u); /* Old request, even though the mode still matches. */
    assert(!visual_consumer_poll(&consumer, &activity, &data, &view, 7u, 3u, true));
    publish_data(7u, 3u);
    assert(!visual_consumer_poll(&consumer, &activity, &data, &view, 7u, 3u, false));
    assert(consumer.discarded == 3u && view.sequence == 1u);
    /* Every rejection returned the slot to the producer. */
    publish_data(7u, 3u);
    assert(visual_consumer_poll(&consumer, &activity, &data, &view, 7u, 3u, true));

    reset();
    uint8_t *read;
    aram_activity_producer_maps(&activity, &read, NULL, NULL);
    aram_activity_mark(read, 0x1234u);
    assert(aram_activity_publish_request(&activity, 0u));
    /* Rapid Data -> Activity changes must not resurrect an old Activity bank. */
    assert(!visual_consumer_poll(&consumer, &activity, &data, &view, 7u, 2u, true));
    assert(!view.valid && consumer.discarded == 1u);
    aram_activity_restart(&activity, 7u);
    aram_activity_producer_maps(&activity, &read, NULL, NULL);
    aram_activity_mark(read, 0x4321u);
    assert(aram_activity_publish_request(&activity, 2u));
    assert(visual_consumer_poll(&consumer, &activity, &data, &view, 7u, 2u, true));
    assert(aram_view_pixel(&view, 0x1234u) == UI_COLOR_INACTIVE);
    assert(aram_view_pixel(&view, 0x4321u) == UI_COLOR_READ);
    assert(view.request == 2u && consumer.consumed == 1u);
    assert(aram_activity_publish_request(&activity, 2u));
    assert(!visual_consumer_poll(&consumer, &activity, &data, &view, 8u, 2u, true));
    assert(consumer.discarded == 2u && view.sequence == 1u);
    assert(aram_activity_publish_request(&activity, 2u));
    assert(!visual_consumer_poll(&consumer, &activity, &data, &view, 7u, 3u, true));
    assert(consumer.discarded == 3u);
    puts("visual consumer ownership, generation and request checks passed");
    return 0;
}
