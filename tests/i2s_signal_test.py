"""Execute the PIO assembly subset and decode its generated I2S wire stream."""
from pathlib import Path
import re
import unittest


def simulate(source, words, edges_needed):
    labels, instructions = {}, []
    for raw in source.splitlines():
        line = raw.split(';')[0].strip()
        if not line or line.startswith('.'):
            continue
        if line.endswith(':'):
            labels[line.removeprefix('public ').rstrip(':')] = len(instructions)
            continue
        match = re.fullmatch(r'(out pins, 1|set x, \d+|jmp x-- \w+)\s+side (0b[01]+)', line)
        if not match:
            raise ValueError(f'Unsupported instruction: {line}')
        instructions.append((match[1], int(match[2], 2)))
    pc, x, bits, data, bck = labels['entry_point'], 0, [], 0, 0
    words = iter(words)
    edges = []
    for _ in range(edges_needed * 8):
        op, side = instructions[pc]
        next_pc = (pc + 1) % len(instructions)
        if op == 'out pins, 1':
            if not bits:
                word = next(words)
                bits = [(word >> bit) & 1 for bit in range(31, -1, -1)]
            data = bits.pop(0)
        elif op.startswith('set x,'):
            x = int(op.split(',')[1])
        else:
            if x != 0:
                next_pc = labels[op.split()[-1]]
            x = (x - 1) & 0xffffffff
        new_bck, ws = side & 1, side >> 1
        if not bck and new_bck:
            edges.append((ws, data))
            if len(edges) == edges_needed:
                return edges
        bck, pc = new_bck, next_pc
    raise AssertionError('PIO did not emit the required clocks')


class I2SSignalTest(unittest.TestCase):
    def test_stereo_words_and_i2s_delay(self):
        source = (Path(__file__).resolve().parents[1] / 'src/audio/i2s_tx.pio').read_text()
        frames = [0x1234fedc, 0x80007fff, 0x0001ffff, 0xabcd5678]
        words = []
        for frame in frames:
            words.extend((frame & 0xffff0000, (frame & 0xffff) << 16))
        edges = simulate(source, words, 257)[1:]  # Initial set establishes BCK/WS.
        expected_samples = [0x1234, 0xfedc, 0x8000, 0x7fff, 1, 0xffff, 0xabcd, 0x5678]
        for i, expected in enumerate(expected_samples):
            chunk = edges[i * 32:(i + 1) * 32]
            value = 0
            for _, bit in chunk:
                value = (value << 1) | bit
            self.assertEqual(value, expected << 16)
            # WS switches one bit before the following channel's MSB.
            self.assertEqual([ws for ws, _ in chunk], [i % 2] * 31 + [1 - i % 2])


if __name__ == '__main__':
    unittest.main()
