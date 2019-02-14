#!/usr/bin/env bash
diet gcc -g -static hello.c -o hello
diet gcc -g -static loader.c -o loader -static -Wl,--section-start -Wl,.text=0x100800 -Wl,--section-start -Wl,.note.gnu.build-id=0x200000 -Wl,--section-start -Wl,.note.ABI-tag=0x300000
