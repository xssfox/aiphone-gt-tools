#!/bin/sh
cd payload || exit
gtar -czf ../wp_verup_proc.tar .
cd ..
openssl smime -encrypt -binary -outform DEM -in wp_verup_proc.tar pub.cer > wp_verup_proc.bin
gtar -cf GT-1C7W_VerUP3p.tar wp_verup_proc.bin
cp GT-1C7W_VerUP3p.tar GT-1C7W_VerUP3p.bin