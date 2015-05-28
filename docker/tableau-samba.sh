#!/bin/sh

# just like in the 80s, no systemsd, no security
# if you have systemsd don't use this file at all
tableaufs -o $1 /mnt/tableau && \
smbd --foreground
