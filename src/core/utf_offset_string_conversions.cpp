// SPDX-License-Identifier: GPL-2.0
/* Copyright (c) 2022 Chilledheart  */

#include "core/utf_offset_string_conversions.hpp"

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "core/check_op.hpp"
#include "core/utils.hpp"

#include <absl/strings/string_view.h>

OffsetAdjuster::Adjustment::Adjustment(size_t original_offset,
                                       size_t original_length,
                                       size_t output_length)
    : original_offset(original_offset),
      original_length(original_length),
      output_length(output_length) {
}

// static
void OffsetAdjuster::AdjustOffsets(const Adjustments& adjustments,
                                   std::vector<size_t>* offsets_for_adjustment,
                                   size_t limit) {
  DCHECK(offsets_for_adjustment);
  for (auto& i : *offsets_for_adjustment)
    AdjustOffset(adjustments, &i, limit);
}

// static
void OffsetAdjuster::AdjustOffset(const Adjustments& adjustments,
                                  size_t* offset,
                                  size_t limit) {
  DCHECK(offset);
  if (*offset == std::u16string::npos)
    return;
  int adjustment = 0;
  for (const auto& i : adjustments) {
    if (*offset <= i.original_offset)
      break;
    if (*offset < (i.original_offset + i.original_length)) {
      *offset = std::u16string::npos;
      return;
    }
    adjustment += static_cast<int>(i.original_length - i.output_length);
  }
  *offset -= adjustment;

  if (*offset > limit)
    *offset = std::u16string::npos;
}

// static
void OffsetAdjuster::UnadjustOffsets(
    const Adjustments& adjustments,
    std::vector<size_t>* offsets_for_unadjustment) {
  if (!offsets_for_unadjustment || adjustments.empty())
    return;
  for (auto& i : *offsets_for_unadjustment)
    UnadjustOffset(adjustments, &i);
}

// static
void OffsetAdjuster::UnadjustOffset(const Adjustments& adjustments,
                                    size_t* offset) {
  if (*offset == std::u16string::npos)
    return;
  int adjustment = 0;
  for (const auto& i : adjustments) {
    if (*offset + adjustment <= i.original_offset)
      break;
    adjustment += static_cast<int>(i.original_length - i.output_length);
    if ((*offset + adjustment) < (i.original_offset + i.original_length)) {
      *offset = std::u16string::npos;
      return;
    }
  }
  *offset += adjustment;
}

// static
void OffsetAdjuster::MergeSequentialAdjustments(
    const Adjustments& first_adjustments,
    Adjustments* adjustments_on_adjusted_string) {
  auto adjusted_iter = adjustments_on_adjusted_string->begin();
  auto first_iter = first_adjustments.begin();
  // Simultaneously iterate over all |adjustments_on_adjusted_string| and
  // |first_adjustments|, pushing adjustments at the end of
  // |adjustments_builder| as we go.  |shift| keeps track of the current number
  // of characters collapsed by |first_adjustments| up to this point.
  // |currently_collapsing| keeps track of the number of characters collapsed by
  // |first_adjustments| into the current |adjusted_iter|'s length.  These are
  // characters that will change |shift| as soon as we're done processing the
  // current |adjusted_iter|; they are not yet reflected in |shift|.
  size_t shift = 0;
  size_t currently_collapsing = 0;
  // While we *could* update |adjustments_on_adjusted_string| in place by
  // inserting new adjustments into the middle, we would be repeatedly calling
  // |std::vector::insert|. That would cost O(n) time per insert, relative to
  // distance from end of the string.  By instead allocating
  // |adjustments_builder| and calling |std::vector::push_back|, we only pay
  // amortized constant time per push. We are trading space for time.
  Adjustments adjustments_builder;
  while (adjusted_iter != adjustments_on_adjusted_string->end()) {
    if ((first_iter == first_adjustments.end()) ||
        ((adjusted_iter->original_offset + shift +
          adjusted_iter->original_length) <= first_iter->original_offset)) {
      // Entire |adjusted_iter| (accounting for its shift and including its
      // whole original length) comes before |first_iter|.
      //
      // Correct the offset at |adjusted_iter| and move onto the next
      // adjustment that needs revising.
      adjusted_iter->original_offset += shift;
      shift += currently_collapsing;
      currently_collapsing = 0;
      adjustments_builder.push_back(*adjusted_iter);
      ++adjusted_iter;
    } else if ((adjusted_iter->original_offset + shift) >
               first_iter->original_offset) {
      // |first_iter| comes before the |adjusted_iter| (as adjusted by |shift|).

      // It's not possible for the adjustments to overlap.  (It shouldn't
      // be possible that we have an |adjusted_iter->original_offset| that,
      // when adjusted by the computed |shift|, is in the middle of
      // |first_iter|'s output's length.  After all, that would mean the
      // current adjustment_on_adjusted_string somehow points to an offset
      // that was supposed to have been eliminated by the first set of
      // adjustments.)
      DCHECK_LE(first_iter->original_offset + first_iter->output_length,
                adjusted_iter->original_offset + shift);

      // Add the |first_iter| to the full set of adjustments.
      shift += first_iter->original_length - first_iter->output_length;
      adjustments_builder.push_back(*first_iter);
      ++first_iter;
    } else {
      // The first adjustment adjusted something that then got further adjusted
      // by the second set of adjustments.  In other words, |first_iter| points
      // to something in the range covered by |adjusted_iter|'s length (after
      // accounting for |shift|).  Precisely,
      //   adjusted_iter->original_offset + shift
      //   <=
      //   first_iter->original_offset
      //   <=
      //   adjusted_iter->original_offset + shift +
      //       adjusted_iter->original_length

      // Modify the current |adjusted_iter| to include whatever collapsing
      // happened in |first_iter|, then advance to the next |first_adjustments|
      // because we dealt with the current one.
      const int collapse = static_cast<int>(first_iter->original_length) -
          static_cast<int>(first_iter->output_length);
      // This function does not know how to deal with a string that expands and
      // then gets modified, only strings that collapse and then get modified.
      DCHECK_GT(collapse, 0);
      adjusted_iter->original_length += collapse;
      currently_collapsing += collapse;
      ++first_iter;
    }
  }
  DCHECK_EQ(0u, currently_collapsing);
  if (first_iter != first_adjustments.end()) {
    // Only first adjustments are left.  These do not need to be modified.
    // (Their offsets are already correct with respect to the original string.)
    // Append them all.
    DCHECK(adjusted_iter == adjustments_on_adjusted_string->end());
    adjustments_builder.insert(adjustments_builder.end(), first_iter,
                               first_adjustments.end());
  }
  *adjustments_on_adjusted_string = std::move(adjustments_builder);
}
