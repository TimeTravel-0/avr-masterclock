#! /usr/bin/env bash
echo "lfuse: "
avrdude -c stk500 -p t2313 -q -q -U lfuse:r:-:b
echo "hfuse:"
avrdude -c stk500 -p t2313 -q -q -U hfuse:r:-:b
echo "efuse:"
avrdude -c stk500 -p t2313 -q -q -U efuse:r:-:b

echo "fuses in elf:"
avr-objdump -d -S -j.fuse main.elf

