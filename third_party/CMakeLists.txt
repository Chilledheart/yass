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
target_compile_definitions(re2 PRIVATE RE2_USE_ICU)
target_include_directories(re2 PUBLIC re2)

target_link_libraries(re2 PUBLIC
    absl::base
    absl::core_headers
    absl::fixed_array
    absl::flags
    absl::flat_hash_map
    absl::flat_hash_set
    absl::inlined_vector
    absl::optional
    absl::span
    absl::str_format
    absl::strings
    absl::synchronization
    icuuc
    )
add_library(re2::re2 ALIAS re2)
