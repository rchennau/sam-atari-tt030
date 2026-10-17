# TT030 audio check (2026-10-16)

Tool: `src/ttsound.c` (`/c/ttsound.ttp`): `info`, `psg [0|1]`, `dma [l|r|b]`. Run over serial; listened by the operator.

| Check | Result |
| :--- | :--- |
| LMC1992 volume/mixer over Microwire (master 0 dB, L/R 0 dB, flat tone, mix DMA + YM2149) | acknowledged: the Microwire mask rotated and returned |
| DMA sound, 440 Hz, 25 kHz stereo, ST-RAM buffer | playing: control = 1, address counter advanced 24,588 bytes in 0.5 s (= 25 kHz stereo) |
| YM2149 tone, 440 Hz, channel A volume 15, port A bit 6 = 0 and = 1 | program ran; no sound either way |
| conterm (0x484) | 0x0F: keyclick on, bell on; operator has never heard a keyclick |
| Rear audio out, external speaker connected | no sound (DMA and YM tones) — **the external speaker was faulty** (see correction) |
| Internal speaker, external speaker unplugged | no sound (YM ×2, DMA) |

**Correction (operator, 2026-10-17):** rear audio out **works** with a known-good speaker; the external speaker
used above was faulty. So the analogue stage is not dead. The internal speaker has not been re-tested.

**Original conclusion (superseded for the rear output):** both sound sources run and the mixer interface answers, yet neither the internal speaker nor the
rear output makes any sound. The common part after the chips is the TT's analogue audio stage (LMC1992 output,
amplifier, speaker and output socket wiring, supply), so this is a hardware fault, not software. Next checks are
inside the case: speaker connector seated, speaker coil continuity, audio amplifier supply and output with a scope
or a known-good earphone on the output pins.

Lesson: an unbounded wait on the Microwire mask in supervisor mode froze the TT (MiNT cannot preempt supervisor
code); all waits in `ttsound` are bounded.
