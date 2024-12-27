#!/bin/sh

mkdir -p tyrian
rm -f tyrian/*
cp COPYING tyrian/copying
cp README tyrian/readme
cp tyrian.prg tyrian/tyrian.prg
cp ../SDL-1.2/README.MiNT tyrian/readme.sdl

rm -f tyrian.zip
zip -o tyrian.zip tyrian/*

gh release delete latest --cleanup-tag -y
gh release create latest --notes "latest"
gh release upload latest tyrian.zip --clobber
