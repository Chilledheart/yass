# *****************************************************************************************
#           re2 Library
# *****************************************************************************************
# build re2 required by absl
set(RE2_SOURCES
  re2/re2/bitmap256.cc
  re2/re2/bitstate.cc
  re2/re2/compile.cc
  re2/re2/dfa.cc
  re2/re2/filtered_re2.cc
  re2/re2/mimics_pcre.cc
  re2/re2/nfa.cc
  re2/re2/onepass.cc
  re2/re2/parse.cc
  re2/re2/perl_groups.cc
  re2/re2/prefilter.cc
  re2/re2/prefilter_tree.cc
  re2/re2/prog.cc
  re2/re2/re2.cc
  re2/re2/regexp.cc
  re2/re2/set.cc
  re2/re2/simplify.cc
  re2/re2/tostring.cc
  re2/re2/unicode_casefold.cc
  re2/re2/unicode_groups.cc
  re2/util/rune.cc
  re2/util/strutil.cc
  )

set(RE2_HEADERS
  re2/re2/filtered_re2.h
  re2/re2/re2.h
  re2/re2/set.h
  re2/re2/stringpiece.h
  )

add_library(re2 ${RE2_SOURCES} ${RE2_HEADERS})
if (USE_LTO_CMAKE)
  set_property(TARGET re2
    PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
target_include_directories(re2 PUBLIC re2)

target_link_libraries(re2 PUBLIC
  absl::absl_check
  absl::absl_log
  absl::base
  absl::core_headers
  absl::fixed_array
  absl::flags
  absl::flat_hash_map
  absl::flat_hash_set
  absl::hash
  absl::inlined_vector
  absl::optional
  absl::span
  absl::str_format
  absl::strings
  absl::synchronization
  )

if (USE_ICU)
  target_compile_definitions(re2 PRIVATE RE2_USE_ICU)
  target_link_libraries(re2 PRIVATE icuuc)
endif()

add_library(re2::re2 ALIAS re2)

# *****************************************************************************************
#           googleurl Library
# *****************************************************************************************

add_library(url STATIC
  # polyfills
  googleurl/polyfills/base/allocator/buildflags.h
  googleurl/polyfills/base/base_export.h
  googleurl/polyfills/base/check.h
  googleurl/polyfills/base/check_op.h
  googleurl/polyfills/base/component_export.h
  googleurl/polyfills/base/cpu_reduction_experiment.h
  googleurl/polyfills/base/dcheck_is_on.h
  googleurl/polyfills/base/debug/alias.h
  googleurl/polyfills/base/export_template.h
  googleurl/polyfills/base/feature_list.h
  googleurl/polyfills/base/logging.h
  googleurl/polyfills/base/memory/raw_ptr.h
  googleurl/polyfills/base/metrics/histogram_macros.h
  googleurl/polyfills/base/notreached.h
  googleurl/polyfills/base/trace_event/memory_usage_estimator.h
  # base
  googleurl/base/bits.h
  googleurl/base/compiler_specific.h
  googleurl/base/containers/checked_iterators.h
  googleurl/base/containers/contains.h
  googleurl/base/containers/contiguous_iterator.h
  googleurl/base/containers/span.h
  googleurl/base/containers/util.h
  googleurl/base/cxx20_is_constant_evaluated.h
  googleurl/base/debug/crash_logging.h
  googleurl/base/debug/leak_annotations.h
  googleurl/base/functional/identity.h
  googleurl/base/functional/invoke.h
  googleurl/base/functional/not_fn.h
  #googleurl/base/memory/raw_ptr_exclusion.h
  googleurl/base/no_destructor.h
  googleurl/base/numerics/checked_math.h
  googleurl/base/numerics/checked_math_impl.h
  googleurl/base/numerics/clamped_math.h
  googleurl/base/numerics/clamped_math_impl.h
  googleurl/base/numerics/safe_conversions.h
  googleurl/base/numerics/safe_conversions_arm_impl.h
  googleurl/base/numerics/safe_conversions_impl.h
  googleurl/base/numerics/safe_math.h
  googleurl/base/numerics/safe_math_arm_impl.h
  googleurl/base/numerics/safe_math_clang_gcc_impl.h
  googleurl/base/numerics/safe_math_shared_impl.h
  googleurl/base/ranges/algorithm.h
  googleurl/base/ranges/functional.h
  googleurl/base/ranges/ranges.h
  googleurl/base/stl_util.h
  googleurl/base/template_util.h
  googleurl/base/types/supports_ostream_operator.h
  googleurl/base/strings/string_piece.h
  googleurl/base/strings/to_string.h
  googleurl/base/strings/string_util_impl_helpers.h
  googleurl/base/strings/string_util_internal.h
  googleurl/base/strings/string_number_conversions.h
  googleurl/base/strings/string_number_conversions.cc
  googleurl/base/strings/string_number_conversions_win.cc
  googleurl/base/strings/utf_ostream_operators.h
  googleurl/base/strings/utf_string_conversions.h
  googleurl/base/strings/utf_string_conversion_utils.h
  googleurl/base/win/win_handle_types.h
  googleurl/base/debug/crash_logging.cc
  googleurl/base/strings/string_util.h
  googleurl/base/strings/string_util.cc
  googleurl/base/strings/string_util_constants.cc
  # FIXME vs2022 doesn't even compile
  googleurl/base/strings/utf_ostream_operators.cc
  googleurl/base/strings/utf_string_conversion_utils.cc
  googleurl/base/strings/utf_string_conversions.cc
  # icu
  googleurl/base/third_party/icu/icu_utf.h
  # url
  googleurl/url/gurl.cc
  googleurl/url/third_party/mozilla/url_parse.cc
  googleurl/url/url_canon.cc
  googleurl/url/url_canon_etc.cc
  googleurl/url/url_canon_filesystemurl.cc
  googleurl/url/url_canon_fileurl.cc
  googleurl/url/url_canon_host.cc
  googleurl/url/url_canon_internal.cc
  googleurl/url/url_canon_internal.h
  googleurl/url/url_canon_internal_file.h
  googleurl/url/url_canon_ip.cc
  googleurl/url/url_canon_mailtourl.cc
  googleurl/url/url_canon_path.cc
  googleurl/url/url_canon_pathurl.cc
  googleurl/url/url_canon_query.cc
  googleurl/url/url_canon_relative.cc
  googleurl/url/url_canon_stdstring.cc
  googleurl/url/url_canon_stdurl.cc
  googleurl/url/url_constants.cc
  googleurl/url/url_features.cc
  googleurl/url/url_parse_file.cc
  googleurl/url/url_parse_internal.h
  googleurl/url/url_util.cc
  googleurl/url/url_util_internal.h
  # googleurl/url/scheme_host_port.cc
  # googleurl/url/scheme_host_port.h
  # googleurl/url/origin.cc
  # googleurl/url/origin.h
  googleurl-override/base/atomic_ref_count.h
  googleurl-override/base/atomicops.h
  googleurl-override/base/atomicops_internals_portable.h
  googleurl-override/base/atomicops_internals_atomicword_compat.h
  googleurl-override/base/macros/concat.h
  googleurl-override/base/macros/if.h
  googleurl-override/base/macros/remove_parens.h
  googleurl-override/base/macros/uniquify.h
  googleurl-override/base/memory/raw_ptr_exclusion.h
  googleurl-override/base/memory/ref_counted.cc
  googleurl-override/base/memory/ref_counted.h
  googleurl-override/base/memory/scoped_refptr.h
  googleurl-override/base/threading/platform_thread.cc
  googleurl-override/base/threading/platform_thread.h
  googleurl-override/base/threading/thread_collision_warner.cc
  googleurl-override/base/threading/thread_collision_warner.h
  googleurl-override/base/rand_util.h
  googleurl-override/base/rand_util.cc
  googleurl-override/polyfills/base/logging.h
  googleurl-override/polyfills/base/check.h
  googleurl-override/polyfills/base/check.cc
  googleurl-override/polyfills/base/check_op.h
  googleurl-override/polyfills/base/check_op.cc
  googleurl-override/polyfills/base/debug/alias.h
  googleurl-override/polyfills/base/debug/alias.cc
  googleurl-override/polyfills/base/debug/debugger.h
  googleurl-override/polyfills/base/debug/debugger.cc
  )

if (WIN32)
  target_sources(url PRIVATE
    googleurl/base/strings/sys_string_conversions_win.cc)

  target_sources(url PRIVATE
    googleurl/base/strings/string_util_win.h
    googleurl/base/strings/string_util_win.cc)
elseif(NOT APPLE)
  target_sources(url PRIVATE
    googleurl/base/strings/sys_string_conversions_posix.cc)
else()
  target_sources(url PRIVATE
    googleurl-override/base/apple/scoped_cftyperef.h
    googleurl-override/base/apple/scoped_typeref.h
    googleurl-override/base/apple/foundation_util.mm
    googleurl-override/base/apple/foundation_util.h
    googleurl-override/base/mac/scoped_ioobject.h
    googleurl-override/base/strings/sys_string_conversions_apple.mm
  )
  # missing apple part
endif()

if (WIN32)
  target_sources(url PRIVATE
    googleurl-override/polyfills/base/debug/debugger_win.cc
    googleurl-override/base/scoped_clear_last_error_win.cc
    googleurl-override/base/rand_util_win.cc)
else()
  target_sources(url PRIVATE
    googleurl-override/polyfills/base/debug/debugger_posix.cc
    googleurl-override/base/posix/safe_strerror.cc
    googleurl-override/base/posix/safe_strerror.h
    googleurl-override/base/rand_util_posix.cc)
endif()

if (USE_ICU)
  target_sources(url PRIVATE
    googleurl/url/url_idna_icu.cc
    googleurl/url/url_canon_icu.h
  )
  target_link_libraries(url PRIVATE
    icuuc
  )
else()
  target_sources(url PRIVATE
    googleurl/url/url_idna_ascii_only.cc
  )
endif()

add_library(double_conversion STATIC
  googleurl-override/base/third_party/double_conversion/double-conversion/bignum-dtoa.cc
  googleurl-override/base/third_party/double_conversion/double-conversion/bignum-dtoa.h
  googleurl-override/base/third_party/double_conversion/double-conversion/bignum.cc
  googleurl-override/base/third_party/double_conversion/double-conversion/bignum.h
  googleurl-override/base/third_party/double_conversion/double-conversion/cached-powers.cc
  googleurl-override/base/third_party/double_conversion/double-conversion/cached-powers.h
  googleurl-override/base/third_party/double_conversion/double-conversion/diy-fp.h
  googleurl-override/base/third_party/double_conversion/double-conversion/double-to-string.cc
  googleurl-override/base/third_party/double_conversion/double-conversion/double-to-string.h
  googleurl-override/base/third_party/double_conversion/double-conversion/double-conversion.h
  googleurl-override/base/third_party/double_conversion/double-conversion/fast-dtoa.cc
  googleurl-override/base/third_party/double_conversion/double-conversion/fast-dtoa.h
  googleurl-override/base/third_party/double_conversion/double-conversion/fixed-dtoa.cc
  googleurl-override/base/third_party/double_conversion/double-conversion/fixed-dtoa.h
  googleurl-override/base/third_party/double_conversion/double-conversion/ieee.h
  googleurl-override/base/third_party/double_conversion/double-conversion/string-to-double.cc
  googleurl-override/base/third_party/double_conversion/double-conversion/string-to-double.h
  googleurl-override/base/third_party/double_conversion/double-conversion/strtod.cc
  googleurl-override/base/third_party/double_conversion/double-conversion/strtod.h
  googleurl-override/base/third_party/double_conversion/double-conversion/utils.h
)

if (USE_LTO_CMAKE)
  set_property(TARGET double_conversion
    PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

target_include_directories(double_conversion PRIVATE
  googleurl-override/base/third_party/double_conversion/double-conversion
)

target_compile_options(double_conversion PRIVATE -Wno-unused-const-variable -Wno-unused-function)

target_link_libraries(url PUBLIC double_conversion)

if (USE_LTO_CMAKE)
  set_property(TARGET url
    PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

target_include_directories(url PUBLIC
  googleurl-override/polyfills
  googleurl-override
  googleurl/polyfills
  googleurl
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  )

target_link_libraries(url PUBLIC absl::optional)

target_sources(url PRIVATE
  googleurl-override/base/files/memory_mapped_file.h
  googleurl-override/base/files/memory_mapped_file.cc
  googleurl-override/base/location.h
  googleurl-override/base/location.cc
)
if (WIN32)
  target_sources(url PRIVATE
    googleurl-override/base/win/pe_image.cc
    googleurl-override/base/win/pe_image.h
    googleurl-override/base/win/scoped_handle.cc
    googleurl-override/base/win/scoped_handle.h
    googleurl-override/base/win/windows_types.h
    googleurl-override/base/win/win_handle_types.h
    googleurl-override/base/files/memory_mapped_file_win.cc
  )
  target_link_libraries(url PRIVATE delayimp)
else()
  target_sources(url PRIVATE
    googleurl-override/base/files/memory_mapped_file_posix.cc
  )
endif()
