"""Fixed point oscillator based on PPG Wavetable 28 (sync)"""


class SyncGen:
    def __init__(self, wave=0):
        self.set_wave(wave)

    def set_wave(self, wave):
        wave = max(0, min(wave, 59))
        self.wave = wave
        self.step = (1 << 24) + wave * 1441792  # Q8.24, 704<<11
        self.reset()

    def reset(self):
        self.acc = 0  # 32-bit unsigned
        self.last_pos = 0  # 8-bit
        self.prev_pos = 0
        self.last_val = -64  # 8-bit
        self.prev_val = -64

    def get(self, pos):
        pos = int(pos) % 128
        if pos == self.last_pos:
            return self.last_val
        elif pos == self.prev_pos:
            return self.prev_val
        self.prev_pos = self.last_pos
        self.prev_val = self.last_val
        if pos < self.last_pos:
            self.acc = 0
            self.last_pos = 0
            self.last_val = -64
        while self.last_pos < pos:
            self.acc += self.step
            if self.acc & 0x80000000:
                self.acc = 0  # sync reset
            self.last_pos += 1
        # self.last_val = (self.acc >> 24) - 64
        self.last_val = (self.acc * 5.960464477539063e-08) - 64
        return self.last_val


if __name__ == "__main__":
    if 0:
        tab = []
        for wave in range(61):
            step = (1 << 24) + wave * 1441792
            acc = 0
            count = 0
            while (not acc & 0x80000000):
                acc += step
                count += 1
            tab.append(count)
        print(tab)
        raise SystemExit

    from matplotlib import pyplot as plt

    gen = SyncGen()
    gen.set_wave(10)

    f0 = 50
    fs = 48000
    step = 128 * f0 / fs
    # step = 1

    phase = 0
    y = []

    for _ in range(2048):
        pos = int(phase)
        alpha = phase - pos
        y1 = gen.get(pos)
        y2 = gen.get(pos + 1)
        ya = (1 - alpha) * y1 + alpha * y2
        y.append(ya)
        phase = (phase + step) % 128

    plt.plot(y)
    plt.grid()
    plt.show()
