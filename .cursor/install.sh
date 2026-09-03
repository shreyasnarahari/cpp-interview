#!/usr/bin/env bash
# Idempotent Cloud Agent bootstrap for the cpp-interview C++ laboratory.
#
# The repository has no third-party package dependencies: every module builds
# against the system toolchain plus standard glibc/kernel headers
# (<elf.h>, <linux/io_uring.h>, POSIX shared memory, futex, pthreads).
#
# There are two independent CMake projects:
#   - practice-cpp/               (C++20 micro-benchmark playground)
#   - system-level-programming/   (C++20/C low-latency systems laboratory)
#
# Both are configured and compiled here with GCC in Release mode so a fresh
# agent boots with ready-to-run build trees. GCC is the project's primary
# recommended compiler (README: "GCC 13+ / Clang 17+"); Clang is also made
# functional for ad-hoc use.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPO_ROOT}"

JOBS="$(nproc)"

echo "==> Ensuring C++ toolchain link support (libstdc++-14-dev)"
# The base image ships gcc-13/g++-13, clang-18 and cmake, but only the gcc-14
# runtime (no matching -dev). Clang selects the highest GCC install (14) as its
# toolchain and then fails with 'cannot find -lstdc++'. Installing the dev
# package makes the default `c++`/clang driver able to link. apt is idempotent.
if ! printf 'int main(){return 0;}\n' | c++ -x c++ - -o /tmp/.cxx_link_probe >/dev/null 2>&1; then
  sudo apt-get update -qq
  sudo apt-get install -y --no-install-recommends libstdc++-14-dev
fi
rm -f /tmp/.cxx_link_probe

export CC=gcc
export CXX=g++

configure_and_build() {
  local dir="$1"
  local keep_going="$2"
  echo "==> Configuring ${dir}"
  cmake -S "${dir}" -B "${dir}/build" -DCMAKE_BUILD_TYPE=Release >/dev/null

  echo "==> Building ${dir}"
  if [[ "${keep_going}" == "keep-going" ]]; then
    # A handful of targets in system-level-programming have pre-existing
    # source bugs (see .cursor/install.sh notes below). Keep going so every
    # buildable target is compiled and cached; do not fail environment setup
    # on those known code-level issues.
    if ! cmake --build "${dir}/build" -j"${JOBS}" -- -k; then
      echo "    NOTE: some targets in ${dir} did not compile (pre-existing"
      echo "          source bugs, not an environment problem). Continuing."
    fi
  else
    cmake --build "${dir}/build" -j"${JOBS}"
  fi
}

# practice-cpp compiles cleanly end to end.
configure_and_build "practice-cpp" "strict"

# system-level-programming: most targets build; a few have pre-existing bugs:
#   * 03 .../vectorized_math_pipeline/bench_pipeline.cpp   bad relative include
#   * 09 .../mini_transactional_engine/transaction_manager.hpp bad relative include
#   * 08 .../concurrent_lsm_tree_engine/sstable.hpp        missing <algorithm> for std::lower_bound
# These are source defects, independent of the environment.
configure_and_build "system-level-programming" "keep-going"

echo "==> Toolchain summary"
gcc --version | head -1
cmake --version | head -1

echo "==> Install complete."
