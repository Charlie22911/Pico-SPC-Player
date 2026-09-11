#include "spc/spc_snapshot.h"
#include "spc/spc_snapshot_queue.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    uint8_t registers[128] = {0};
    spc_voice_internal_t internals[8] = {0};
    registers[0x00] = 0x80u;
    registers[0x01] = 0x7fu;
    registers[0x02] = 0x34u;
    registers[0x03] = 0xd2u;
    registers[0x04] = 0x56u;
    registers[0x08] = 0x4au;
    registers[0x09] = 0xf0u;
    registers[0x4c] = 0x01u;
    registers[0x5c] = 0x02u;
    registers[0x7c] = 0x01u;
    internals[0] = (spc_voice_internal_t){0xabcd, 0x900u, SPC_ENV_ATTACK};

    spc_snapshot_t snapshot;
    spc_snapshot_build(&snapshot, registers, internals, 7u, 1234u, 9u, 2u);
    assert(snapshot.validity ==
           (SPC_SNAPSHOT_VALID_DSP_REGISTERS | SPC_SNAPSHOT_VALID_VOICE_INTERNALS));
    assert(snapshot.generation == 7u);
    assert(snapshot.track_frames == 1234u);
    assert(snapshot.apu_cycles == 1234u * 32u);
    assert(snapshot.voices[0].volume_left == INT8_MIN);
    assert(snapshot.voices[0].volume_right == INT8_MAX);
    assert(snapshot.voices[0].pitch == 0x1234u);
    assert(snapshot.voices[0].source_number == 0x56u);
    assert(snapshot.voices[0].outx == -16);
    assert(snapshot.voices[0].brr_address == 0xabcdu);
    assert(snapshot.voices[0].envelope == 0x7ffu);
    assert(snapshot.voices[0].envelope_mode == SPC_ENV_ATTACK);
    assert(snapshot.voices[0].key_on && snapshot.voices[0].endx);
    assert(!snapshot.voices[0].key_off);
    assert(snapshot.voices[1].key_off);
    assert(memcmp(snapshot.dsp_registers, registers, sizeof(registers)) == 0);

    spc_snapshot_queue_t queue;
    spc_snapshot_queue_init(&queue);
    for (uint32_t i = 0u; i < SPC_SNAPSHOT_QUEUE_CAPACITY; ++i) {
        snapshot.sequence = i;
        assert(spc_snapshot_queue_push(&queue, &snapshot));
    }
    assert(!spc_snapshot_queue_push(&queue, &snapshot));
    snapshot = (spc_snapshot_t){0};
    assert(spc_snapshot_queue_pop_latest(&queue, &snapshot));
    assert(snapshot.sequence == SPC_SNAPSHOT_QUEUE_CAPACITY - 1u);
    assert(!spc_snapshot_queue_pop_latest(&queue, &snapshot));
    puts("SPC snapshot contract OK");
    return 0;
}
