#!/bin/sh
set -eu

rm -rf pahole

git clone https://git.kernel.org/pub/scm/devel/pahole/pahole.git \
  --depth 1 \
  --branch v1.22

mkdir pahole/build
cd pahole/build

cmake -D__LIB=lib -DBUILD_SHARED_LIBS=OFF ..
make
