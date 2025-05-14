#!/usr/bin/env bash
set -eu

kernel_dir=linux

if [ -z "${MAKEFLAGS+x}" ]; then
  MAKEFLAGS="-j $(nproc)"
  export MAKEFLAGS
fi

run_as_user() {
  if [ -z "${SUDO_USER+x}" ]; then
    "$@"
  else
    sudo -u "$SUDO_USER" --preserve-env=MAKEFLAGS -- "$@"
  fi
}

install_kernel_build_deps() {
  apt-get install -y build-essential
  apt-get build-dep -y linux
}

install_pahole_if_needed() {
  command -v pahole > /dev/null 2>&1 && \
    [ "$(pahole --version)" = "v1.22" ] && \
    return
  apt-get install -y libdw-dev libbpf-dev
  apt-get remove -y pahole dwarves
  run_as_user ./build-pahole.sh
  (cd pahole/build && make install)
}

install_usm_deps() {
  apt-get install -y cmake pkg-config
  install_pahole_if_needed
}



build_and_install_kernel() {
  local worktree_name=$1
  local remote_name=$2
  local remote_url=$3
  local rev=$4

  pushd "$kernel_dir"

  git_remote_add_if_missing "$remote_name" "$remote_url"
  git_worktree_checkout "$worktree_name" "$rev"

  cd "$worktree_name"

  install_kernel_build_deps

  make
  make install

  make modules
  make modules_install
  
  popd
}

# Now, we need to kexec into the new kernel.

extmem() {
  run_as_user ./build-kernel.sh extmem extmem "https://github.com/SepehrDV2/linux.git" 0fa2007a9aa3f86902f44a7550ea46504e103b41
  run_as_user ./build-kernel.sh fbmm fbmm "https://github.com/multifacet/fbmm.git" bd6aeef6578abd6e7737bdb15338229e01faa2a3
  run_as_user ./build-kernel.sh uintr uintr "https://github.com/intel/uintr-linux-kernel.git" 0ee776bd38532358159013ed0188693b34c46cf5
}

convert_timelines_to_stacked_area_charts() {
  local f
  local name
  for f in results/*-timeline.csv; do
    name="${f%-timeline.csv}"
    ./convert-timeline-to-stacked-area-chart.py "$f" --lines 1000 > "$name-stacked.csv"
  done
}

if [ ! -f results/extmem-read.csv ]; then
  extmem
fi

convert_timelines_to_stacked_area_charts
