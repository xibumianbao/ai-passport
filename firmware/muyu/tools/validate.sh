#!/usr/bin/env bash
set -euo pipefail

mode="${1:---all}"
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    echo "Usage: $0 [--all|--static|--firmware|--preview]" >&2
}

run_static_checks() {
    local actionlint_bin
    local test_dir

    python3 tools/check_repo.py

    actionlint_bin="${ACTIONLINT_BIN:-}"
    if [[ -z "${actionlint_bin}" ]]; then
        actionlint_bin="$(command -v actionlint || true)"
    fi
    if [[ -z "${actionlint_bin}" || ! -x "${actionlint_bin}" ]]; then
        actionlint_bin="$(./tools/install-actionlint.sh)"
    fi
    "${actionlint_bin}" -color .github/workflows/*.yml

    test_dir="$(mktemp -d /tmp/ai-passport-host-tests.XXXXXX)"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_ui_pixel_math.c main/ui_pixel_math.c \
        -o "${test_dir}/test_ui_pixel_math"
    "${test_dir}/test_ui_pixel_math"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_muyu_logic.c main/muyu_logic.c -lm \
        -o "${test_dir}/test_muyu_logic"
    "${test_dir}/test_muyu_logic"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -Imain \
        tests/test_passport_core.c main/passport_core.c -o "${test_dir}/test_passport_core"
    "${test_dir}/test_passport_core"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g -Imain         tests/test_passport_extensions.c main/passport_keyboard.c main/passport_screen.c main/passport_core.c         -o "${test_dir}/test_passport_extensions"
    "${test_dir}/test_passport_extensions"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
        -Icomponents/pp_app_pet/include -Icomponents/pp_avatar/include \
        tests/test_pet.c components/pp_app_pet/pp_pet.c components/pp_avatar/pp_avatar.c \
        components/pp_avatar/pp_pixel_art.c components/pp_avatar/pp_pixel_buffer.c -o "${test_dir}/test_pet"
    "${test_dir}/test_pet"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
        -Itests/pet_stubs -Icomponents/pp_app_pet/include \
        tests/test_pet_store.c components/pp_app_pet/pp_pet.c components/pp_app_pet/pp_pet_store.c \
        -o "${test_dir}/test_pet_store"
    "${test_dir}/test_pet_store"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
        -Icomponents/pp_voice/include tests/test_voice_wire.c components/pp_voice/pp_voice_wire.c \
        -o "${test_dir}/test_voice_wire"
    "${test_dir}/test_voice_wire"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
        -Icomponents/pp_voice/include tests/test_voice_turn.c components/pp_voice/pp_voice_turn.c \
        -o "${test_dir}/test_voice_turn"
    "${test_dir}/test_voice_turn"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
        -Itests/voice_stubs -Icomponents/pp_voice/include \
        tests/test_voice_codec.c components/pp_voice/pp_voice_codec.c -o "${test_dir}/test_voice_codec"
    "${test_dir}/test_voice_codec"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
        -Itests/voice_stubs -Icomponents/pp_voice/include \
        tests/test_voice_selftest.c components/pp_voice/pp_voice_codec.c components/pp_voice/pp_voice_selftest.c \
        -o "${test_dir}/test_voice_selftest"
    "${test_dir}/test_voice_selftest"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icomponents/pp_avatar/include \
        tools/preview/pet_art_export.c components/pp_avatar/pp_pixel_art.c components/pp_avatar/pp_pixel_buffer.c \
        -o "${test_dir}/pet_art_export"
    "${test_dir}/pet_art_export" > "${test_dir}/pp_pet_room_i4.c"
    python3 tools/check_pet_assets.py "${test_dir}/pp_pet_room_i4.c"
    python3 tools/generate_catalog.py
    python3 tests/test_capacity.py
    python3 tests/test_verify_firmware.py
    rm -rf "${test_dir}"
    echo "Host tests: PASS"
}

run_preview_checks() (
    local preview_build_dir="${PREVIEW_BUILD_DIR:-${repo_root}/build/preview}"
    local -a cmake_args=(-S tools/preview -B "${preview_build_dir}" -DCMAKE_BUILD_TYPE=Debug)
    # A local pinned LVGL checkout can be supplied without fetching components.
    # CI uses the managed LVGL source resolved by the preceding firmware build.
    if [[ -n "${LVGL_SOURCE_DIR:-}" ]]; then
        cmake_args+=("-DLVGL_SOURCE_DIR=${LVGL_SOURCE_DIR}")
    fi
    cmake "${cmake_args[@]}"
    preview_build_dir="$(cd -- "${preview_build_dir}" && pwd)"
    cmake --build "${preview_build_dir}" --parallel "${PREVIEW_JOBS:-4}"
    mkdir -p "${repo_root}/build"
    cd "${repo_root}/build"
    "${preview_build_dir}/avatar_view_test"
    "${preview_build_dir}/passport_preview"
    "${preview_build_dir}/passport_catalog_test"
    "${preview_build_dir}/pet_preview"
    "${preview_build_dir}/xiaozhi_preview"
    "${preview_build_dir}/muyu_audio" muyu-knock.pcm
    python3 "${repo_root}/tools/preview/write_media.py"
    echo "LVGL preview and lifecycle tests: PASS"
)

run_firmware_checks() (
    local validation_build_dir

    if ! command -v idf.py >/dev/null 2>&1; then
        echo "ERROR: idf.py is not available; activate ESP-IDF 5.5.3 first." >&2
        return 1
    fi

    validation_build_dir="$(mktemp -d /tmp/ai-passport-firmware.XXXXXX)"
    trap 'case "${validation_build_dir}" in /tmp/ai-passport-firmware.*) rm -rf -- "${validation_build_dir}" ;; esac' EXIT

    SDKCONFIG_DEFAULTS="${repo_root}/sdkconfig.defaults" \
        idf.py -B "${validation_build_dir}" \
        -D "SDKCONFIG=${validation_build_dir}/sdkconfig" build
    python3 tools/build_capacity.py "${validation_build_dir}"
    idf.py -B "${validation_build_dir}" build
    python3 tools/build_capacity.py "${validation_build_dir}" --verify
    idf.py -B "${validation_build_dir}" merge-bin \
        -o "${validation_build_dir}/FoloToy-AI-Passport-full.bin"
    python3 tools/verify_firmware.py "${validation_build_dir}"
    mkdir -p "${repo_root}/build"
    install -m 0644 \
        "${validation_build_dir}/FoloToy-AI-Passport-full.bin" \
        "${repo_root}/build/FoloToy-AI-Passport-full.bin"
    # Preserve the exact checked segments for safe device flashing and local
    # readback verification. Never export NVS or the per-device cardid region.
    mkdir -p "${repo_root}/build/bootloader" "${repo_root}/build/partition_table"
    for relative in flash_args sdkconfig capacity-report.json FoloToy-AI-Passport.bin \
                    bootloader/bootloader.bin partition_table/partition-table.bin; do
        install -m 0644 "${validation_build_dir}/${relative}" "${repo_root}/build/${relative}"
    done
    python3 tools/verify_muyu.py "${repo_root}/build"
    echo "Firmware build: PASS"
)

cd "${repo_root}"
case "${mode}" in
    --all)
        run_static_checks
        run_firmware_checks
        run_preview_checks
        ;;
    --static)
        run_static_checks
        ;;
    --firmware)
        run_firmware_checks
        ;;
    --preview)
        run_preview_checks
        ;;
    *)
        usage
        exit 2
        ;;
esac
