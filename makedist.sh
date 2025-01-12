#!/bin/sh

mkdir -p tyrian
rm -f tyrian/*
cp COPYING tyrian/copying
cp README tyrian/readme
make clean
make target=68000
cp tyrian00.prg tyrian/tyrian00.prg
make clean
make target=68060
cp tyrian60.prg tyrian/tyrian60.prg
cp ../SDL-1.2/README.MiNT tyrian/readme.sdl

rm -f tyrian.zip
zip -o tyrian.zip tyrian/*

gh release delete latest --cleanup-tag -y
gh release create latest --notes "latest"
gh release upload latest tyrian.zip --clobber
