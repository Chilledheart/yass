add_library(tcmalloc_new_extension STATIC
  src/tcmalloc/new_extension.cc
  src/tcmalloc/new_extension.h
)

target_include_directories(tcmalloc_new_extension PUBLIC
  src
)

target_link_libraries(tcmalloc_new_extension PRIVATE
  absl::core_headers
)

add_library(tcmalloc_malloc_extension STATIC
  src/tcmalloc/malloc_extension.cc
  src/tcmalloc/malloc_extension.h
)

target_include_directories(tcmalloc_malloc_extension PUBLIC
  src
)

target_link_libraries(tcmalloc_malloc_extension PRIVATE
  absl::core_headers
)

add_library(tcmalloc_common OBJECT
  src/tcmalloc/internal/affinity.h
  src/tcmalloc/internal/affinity.cc
  src/tcmalloc/internal/cache_topology.h
  src/tcmalloc/internal/cache_topology.cc
  src/tcmalloc/internal/environment.h
  src/tcmalloc/internal/environment.cc
  src/tcmalloc/internal/logging.h
  src/tcmalloc/internal/logging.cc
  src/tcmalloc/internal/numa.h
  src/tcmalloc/internal/numa.cc
  src/tcmalloc/internal/memory_stats.h
  src/tcmalloc/internal/memory_stats.cc
  src/tcmalloc/internal/mincore.h
  src/tcmalloc/internal/mincore.cc
  src/tcmalloc/internal/page_size.h
  src/tcmalloc/internal/page_size.cc
  src/tcmalloc/internal/percpu.h
  src/tcmalloc/internal/percpu.cc
  src/tcmalloc/internal/percpu_rseq_asm.S
  src/tcmalloc/internal/percpu_rseq_unsupported.cc
  src/tcmalloc/internal/proc_maps.h
  src/tcmalloc/internal/proc_maps.cc
  src/tcmalloc/internal/sysinfo.h
  src/tcmalloc/internal/sysinfo.cc
  src/tcmalloc/internal/util.h
  src/tcmalloc/internal/util.cc

  src/tcmalloc/experiment.h
  src/tcmalloc/experiment.cc

  src/tcmalloc/allocation_sample.cc
  src/tcmalloc/arena.cc
  src/tcmalloc/arena.h
  src/tcmalloc/background.cc
  src/tcmalloc/central_freelist.cc
  src/tcmalloc/central_freelist.h
  src/tcmalloc/common.cc
  src/tcmalloc/common.h
  src/tcmalloc/cpu_cache.cc
  src/tcmalloc/cpu_cache.h
  src/tcmalloc/deallocation_profiler.cc
  src/tcmalloc/experimental_pow2_size_class.cc
  src/tcmalloc/global_stats.cc
  src/tcmalloc/guarded_allocations.h
  src/tcmalloc/guarded_page_allocator.cc
  src/tcmalloc/guarded_page_allocator.h
  src/tcmalloc/hinted_tracker_lists.h
  src/tcmalloc/huge_address_map.cc
  src/tcmalloc/huge_allocator.cc
  src/tcmalloc/huge_allocator.h
  src/tcmalloc/huge_cache.cc
  src/tcmalloc/huge_cache.h
  src/tcmalloc/huge_page_aware_allocator.cc
  src/tcmalloc/huge_page_aware_allocator.h
  src/tcmalloc/huge_page_filler.h
  src/tcmalloc/huge_pages.h
  src/tcmalloc/huge_region.h
  src/tcmalloc/legacy_size_classes.cc
  src/tcmalloc/page_allocator.cc
  src/tcmalloc/page_allocator.h
  src/tcmalloc/page_allocator_interface.cc
  src/tcmalloc/page_allocator_interface.h
  src/tcmalloc/page_heap.cc
  src/tcmalloc/page_heap.h
  src/tcmalloc/page_heap_allocator.h
  src/tcmalloc/pagemap.cc
  src/tcmalloc/pagemap.h
  src/tcmalloc/parameters.cc
  src/tcmalloc/peak_heap_tracker.cc
  src/tcmalloc/sampler.cc
  src/tcmalloc/sampler.h
  src/tcmalloc/segv_handler.cc
  src/tcmalloc/segv_handler.h
  src/tcmalloc/size_classes.cc
  src/tcmalloc/sizemap.cc
  src/tcmalloc/span.cc
  src/tcmalloc/span.h
  src/tcmalloc/span_stats.h
  src/tcmalloc/stack_trace_table.cc
  src/tcmalloc/stack_trace_table.h
  src/tcmalloc/static_vars.cc
  src/tcmalloc/static_vars.h
  src/tcmalloc/stats.cc
  src/tcmalloc/system-alloc.cc
  src/tcmalloc/system-alloc.h
  src/tcmalloc/thread_cache.cc
  src/tcmalloc/thread_cache.h
  src/tcmalloc/transfer_cache.cc
  src/tcmalloc/transfer_cache.h
  src/tcmalloc/transfer_cache_internals.h
  src/tcmalloc/transfer_cache_stats.h
  src/tcmalloc/allocation_sample.h
  src/tcmalloc/allocation_sampling.h
  src/tcmalloc/arena.h
  src/tcmalloc/central_freelist.h
  src/tcmalloc/common.h
  src/tcmalloc/cpu_cache.h
  src/tcmalloc/deallocation_profiler.h
  src/tcmalloc/global_stats.h
  src/tcmalloc/guarded_allocations.h
  src/tcmalloc/guarded_page_allocator.h
  src/tcmalloc/hinted_tracker_lists.h
  src/tcmalloc/huge_address_map.h
  src/tcmalloc/huge_allocator.h
  src/tcmalloc/huge_cache.h
  src/tcmalloc/huge_page_aware_allocator.h
  src/tcmalloc/huge_page_filler.h
  src/tcmalloc/huge_pages.h
  src/tcmalloc/huge_region.h
  src/tcmalloc/page_allocator.h
  src/tcmalloc/page_allocator_interface.h
  src/tcmalloc/page_heap.h
  src/tcmalloc/page_heap_allocator.h
  src/tcmalloc/pagemap.h
  src/tcmalloc/pages.h
  src/tcmalloc/parameters.h
  src/tcmalloc/peak_heap_tracker.h
  src/tcmalloc/sampled_allocation_allocator.h
  src/tcmalloc/sampler.h
  src/tcmalloc/segv_handler.h
  src/tcmalloc/sizemap.h
  src/tcmalloc/span.h
  src/tcmalloc/span_stats.h
  src/tcmalloc/stack_trace_table.h
  src/tcmalloc/static_vars.h
  src/tcmalloc/stats.h
  src/tcmalloc/system-alloc.h
  src/tcmalloc/tcmalloc_policy.h
  src/tcmalloc/thread_cache.h
  src/tcmalloc/transfer_cache.h
  src/tcmalloc/transfer_cache_internals.h
  src/tcmalloc/transfer_cache_stats.h
)

target_include_directories(tcmalloc_common PRIVATE
  src
)

target_link_libraries(tcmalloc_common PUBLIC
  tcmalloc_malloc_extension
  absl::core_headers
  absl::flat_hash_map
  absl::fixed_array
  absl::optional
  absl::span
  absl::strings
  absl::time
)

add_library(tcmalloc STATIC
  src/tcmalloc/tcmalloc.h
  src/tcmalloc/tcmalloc.cc
)

target_include_directories(tcmalloc PRIVATE
  src
)

target_compile_options(tcmalloc PRIVATE
 -fvisibility=default
)

target_link_libraries(tcmalloc PUBLIC
  tcmalloc_common
)
