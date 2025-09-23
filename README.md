# Jamminate
**Created By FozzTexx**

**Jamminate** can turn your vintage computer into a **playable, networked
musical instrument**. Using **OSC (Open Sound Control)**, it receives
real-time musical commands from networked devices. OSC is a protocol
for sending musical and multimedia control messages over a
network. Jamminate is designed **to demonstrate how easy it is to use
FujiNet’s TCP capabilities**. It brings retro computers to life with
music in real time, and can be controlled from devices you already own
— phone, tablet, or computer — with no exotic hardware required.

https://www.insentricity.com/a.cl/293/jamminate-your-8-bit

---

## Overview

Jamminate shows the magic of networked music on vintage hardware:

- **Responds in real time** to musical commands from any networked device.
- **Performs melodies, chords, and harmonies** on retro computers.
- Notes continue until released, allowing expressive play.
- **Polyphony support where hardware allows**, letting multiple notes sound simultaneously.

Whether you want to experiment with your CoCo, Apple II, or other
retro systems, Jamminate makes it easy to turn your vintage computer
into a live instrument.

---

## Features

- **OSC/TCP input**: Send musical commands from any OSC-capable networked device.
- **Expressive control**: Notes start on key down and stop on key up for real-time performance.
- **Polyphony (hardware-dependent)**: Play overlapping notes or simple chords.
- **Retro-friendly**: Designed to run on 8-bit computers with FujiNet,
  but adaptable to other platforms.

---

## Why I Made This

Most retro projects focus on games, text downloads, or demos that
don’t fully explore network capabilities. **Jamminate** was created to
show **how simple it is to make a vintage computer respond to network
commands using FujiNet**, while also providing a **fun, engaging
musical experience**. It’s a way to combine retro computing with
real-time interactivity that anyone can try using devices they already
own.

---

## How It Works (High Level)

1. A **networked device sends note on/off events** over OSC.
2. **FujiNet handles the TCP connection** and forwards the commands to the retro computer.
3. The retro computer **performs the notes** in real time.
4. Notes continue until the corresponding **note off events** arrive.

This allows the vintage computer to function as a **real-time,
networked musical instrument**, with polyphony and expressive note
control where the hardware allows.

---

## Example Use Cases

- Play melodies or chords from a touchscreen piano on a phone, tablet, or desktop app.
- Explore networked instruments on vintage computers.
- Demonstrate FujiNet’s TCP capabilities in a fun, musical context.
- Jam in real time with retro hardware alongside modern devices.
