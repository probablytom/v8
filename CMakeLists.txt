# install devel/glib20
# pkg64 install devel/py-Jinja2
# pkg64c install devel/icu
# pkg64 install git
# pkg64 install python3.9
# Make sure `python3` points to python3.9 somehow, e.g. symlinking in /home/username/bin

cmake_minimum_required(VERSION 3.24)
include(ExternalProject)
project(v8)

find_package(
        Python3
        REQUIRED
        COMPONENTS Interpreter
)

ExternalProject_Add(
  cheribsd-ports 
  SOURCE_DIR     cheribsd-ports
  GIT_REPOSITORY https://github.com/probablytom/cheribsd-ports.git
  GIT_SHALLOW    true
  GIT_TAG        main
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
)

set(gn-args "custom_toolchain=\"//tools/toolchain:morello_llvm_hybrid_native\" clang_use_chrome_plugins=false extra_cxxflags=\"-mabi=aapcs -march=morello -Xclang -morello-vararg=new -DV8_COMPRESS_POINTERS -DV8_ENABLE_DIRECT_HANDLE -DV8_ENABLE_CHERI_SANDBOX -DV8_ENABLE_DIRECT_HANDLE -DV8_ENABLE_CAPABILITY_HANDLE -DV8_ENABLE_CONSERVATIVE_STACK_SCANNING\" extra_ldflags=\"\" is_clang=true is_component_build=true is_debug=true treat_warnings_as_errors=false use_aura=true use_lld=true use_custom_libcxx=false v8_use_snapshot=false v8_use_external_snapshot_data=false v8_enable_webassembly=false v8_enable_external_code_space=true v8_enable_sandbox=true target_os=\"freebsd\" v8_enable_i18n_support=false")

add_custom_target(apply-patches ALL
    COMMAND find ${CMAKE_BINARY_DIR}/cheribsd-ports/lang/v8/files -type f -name 'patch-*' -print0 | xargs -t -0 -n 1 patch -p0 -u -N -d ${CMAKE_SOURCE_DIR} -i || true)
add_custom_target(gcc-toolchain ALL
    COMMAND echo 'checkout_google_benchmark = false' > ${CMAKE_SOURCE_DIR}/build/config/gclient_args.gni)
# === TW 240123 commented out because patches _appear_ to already do this. ===
# add_custom_target(checkout-google-benchmark ALL
#     COMMAND sed -i '' 's|%%LOCALBASE%%|/usr/local|' ${CMAKE_SOURCE_DIR}/build/toolchain/gcc_toolchain.gni ${CMAKE_SOURCE_DIR}/buildtools/third_party/libc++/BUILD.gn)
add_custom_target(replace-icu ALL
    COMMAND python3 ${CMAKE_SOURCE_DIR}/build/linux/unbundle/replace_gn_files.py --system-libraries icu)
add_custom_target(add-toolchain-target ALL
    COMMAND if ! grep -q "morello_llvm_hybrid_native" ${CMAKE_SOURCE_DIR}/tools/toolchain/BUILD.gn$<SEMICOLON> then printf "\"gcc_toolchain(\\\"morello_llvm_hybrid_native\\\"){cc = \\\"/usr/local64/bin/clang-morello\\\"\\ncxx = \\\"/usr/local64/bin/clang++-morello\\\"\\nreadelf = \\\"/usr/local64/bin/llvm-readelf-morello\\\"\\nnm = \\\"/usr/local64/bin/llvm-nm-morello\\\"\\nar = \\\"/usr/local64/bin/llvm-ar-morello\\\"\\nld = cxx\\nextra_cflags = \\\"--target=aarch64-unknown-freebsd -mabi=aapcs -march=morello -Xclang -morello-vararg=new -DV8_COMPRESS_POINTERS -DV8_ENABLE_DIRECT_HANDLE -DV8_ENABLE_CHERI_SANDBOX -DV8_ENABLE_DIRECT_HANDLE -DV8_ENABLE_CAPABILITY_HANDLE -DV8_ENABLE_CONSERVATIVE_STACK_SCANNING\\\"\\nextra_cxxflags = \\\"--target=aarch64-unknown-freebsd -mabi=aapcs -march=morello -Xclang -morello-vararg=new -DV8_COMPRESS_POINTERS -DV8_ENABLE_DIRECT_HANDLE -DV8_ENABLE_CHERI_SANDBOX -DV8_ENABLE_DIRECT_HANDLE -DV8_ENABLE_CAPABILITY_HANDLE -DV8_ENABLE_CONSERVATIVE_STACK_SCANNING\\\"\\nextra_ldflags = \\\"--target=aarch64-unknown-freebsd -mabi=aapcs -march=morello -Xclang -morello-vararg=new -DV8_COMPRESS_POINTERS -DV8_ENABLE_DIRECT_HANDLE -DV8_ENABLE_CHERI_SANDBOX -DV8_ENABLE_DIRECT_HANDLE -DV8_ENABLE_CAPABILITY_HANDLE -DV8_ENABLE_CONSERVATIVE_STACK_SCANNING\\\"\\ntoolchain_args = {current_cpu = \\\"arm64\\\"\\ncurrent_os = \\\"freebsd\\\"\\nis_clang = true\\nuse_lld = true\\nv8_enable_i18n_support = false\\nsimple_template_names = false}}\"" >> ${CMAKE_SOURCE_DIR}/tools/toolchain/BUILD.gn$<SEMICOLON> fi
    )
add_custom_target(generate-build-config ALL
    COMMAND gn gen out/hybrid --args=${gn-args}
    VERBATIM)
add_custom_target(compile ALL
    COMMAND ninja -C out/hybrid)
