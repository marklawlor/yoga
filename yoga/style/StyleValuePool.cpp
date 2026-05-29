/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <yoga/style/StyleValuePool.h>

#include <yoga/debug/Log.h>
#include <yoga/style/StyleExpression.h>

namespace facebook::yoga {

void StyleValuePool::store(
    StyleValueHandle& handle,
    std::vector<ExpressionNode> nodes) {
  if (handle.isExpression()) {
    expressionSlots_[handle.value()] = std::move(nodes);
  } else {
    uint16_t slot;
    if (!freeSlots_.empty()) {
      slot = freeSlots_.back();
      freeSlots_.pop_back();
      expressionSlots_[slot] = std::move(nodes);
    } else {
      // The handle's value field is 12 bits, so slot indices must stay below
      // 4096. Past that, fail safe by ignoring the expression rather than
      // truncating the slot index (which would alias an in-use slot and
      // silently corrupt layout in release builds, where the assert is gone).
      // The handle is left untouched, preserving its existing value.
      if (expressionSlots_.size() >= (1u << 12)) {
        yoga::log(
            LogLevel::Warn,
            "yoga-expression: exceeded the maximum of 4096 stored expressions; "
            "expression ignored\n");
        return;
      }
      slot = static_cast<uint16_t>(expressionSlots_.size());
      expressionSlots_.push_back(std::move(nodes));
    }
    // If the handle previously referenced a SmallValueBuffer chunk, reclaim it
    // for reuse before discarding the handle — otherwise that chunk would be
    // orphaned (the buffer has no free of its own) and repeated value↔
    // expression toggling on a property would grow the buffer without bound.
    if (handle.isValueIndexed()) {
      freeBufferChunks_.push_back(handle.value());
    }
    // Reset handle entirely before encoding expression slot to avoid
    // corrupting SmallValueBuffer if handle was previously buffer-indexed.
    handle = StyleValueHandle{};
    handle.setType(StyleValueHandle::Type::Expression);
    handle.setValue(slot);
  }
}

FloatOptional StyleValuePool::evaluateExpression(
    StyleValueHandle handle,
    float referenceLength) const {
  assert(handle.isExpression());
  return evaluate(
      expressionSlots_[handle.value()], /*rootIndex=*/0, referenceLength);
}

const std::vector<ExpressionNode>& StyleValuePool::getExpressionNodes(
    StyleValueHandle handle) const {
  assert(handle.isExpression());
  return expressionSlots_[handle.value()];
}

} // namespace facebook::yoga
