#!/bin/sh

# Requires an environment with Docker, the GNU RISC-V toolchain,
# and the riscof Python package installed.

make -C ../cmodel

if [ ! -e riscv-arch-test ]; then
    riscof arch-test --clone
else
    riscof arch-test --update
fi

docker pull registry.gitlab.com/incoresemi/docker-images/compliance

riscof run \
    --suite riscv-arch-test/riscv-test-suite \
    --env riscv-arch-test/riscv-test-suite/env
