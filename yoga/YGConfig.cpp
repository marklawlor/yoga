/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <yoga/Yoga.h>
#include <yoga/config/Config.h>
#include <yoga/debug/AssertFatal.h>
#include <yoga/debug/Log.h>
#include <yoga/numeric/Comparison.h>
#include <yoga/numeric/FloatOptional.h>

using namespace facebook;
using namespace facebook::yoga;

YGConfigRef YGConfigNew(void) {
  return new yoga::Config(getDefaultLogger());
}

void YGConfigFree(const YGConfigRef config) {
  delete resolveRef(config);
}

YGConfigConstRef YGConfigGetDefault() {
  return &yoga::Config::getDefault();
}

void YGConfigSetUseWebDefaults(const YGConfigRef config, const bool enabled) {
  resolveRef(config)->setUseWebDefaults(enabled);
}

bool YGConfigGetUseWebDefaults(const YGConfigConstRef config) {
  return resolveRef(config)->useWebDefaults();
}

void YGConfigSetPointScaleFactor(
    const YGConfigRef config,
    const float pixelsInPoint) {
  yoga::assertFatalWithConfig(
      resolveRef(config),
      pixelsInPoint >= 0.0f,
      "Scale factor should not be less than zero");

  resolveRef(config)->setPointScaleFactor(pixelsInPoint);
}

float YGConfigGetPointScaleFactor(const YGConfigConstRef config) {
  return resolveRef(config)->getPointScaleFactor();
}

void YGConfigSetEnv(
    const YGConfigRef config,
    const char* name,
    const float value) {
  if (name == nullptr) {
    return;
  }
  // YGUndefined clears the value (== remove).
  resolveRef(config)->setEnv(
      name, yoga::isUndefined(value) ? FloatOptional{} : FloatOptional{value});
}

void YGConfigRemoveEnv(const YGConfigRef config, const char* name) {
  if (name == nullptr) {
    return;
  }
  resolveRef(config)->removeEnv(name);
}

float YGConfigGetEnv(const YGConfigConstRef config, const char* name) {
  if (name == nullptr) {
    return YGUndefined;
  }
  FloatOptional value = resolveRef(config)->getEnv(name);
  return value.isDefined() ? value.unwrap() : YGUndefined;
}

void YGConfigCopyEnv(const YGConfigRef dst, const YGConfigConstRef src) {
  if (dst == nullptr || src == nullptr) {
    return;
  }
  resolveRef(dst)->copyEnvFrom(*resolveRef(src));
}

void YGConfigSetErrata(YGConfigRef config, YGErrata errata) {
  resolveRef(config)->setErrata(scopedEnum(errata));
}

YGErrata YGConfigGetErrata(YGConfigConstRef config) {
  return unscopedEnum(resolveRef(config)->getErrata());
}

void YGConfigSetLogger(const YGConfigRef config, YGLogger logger) {
  if (logger != nullptr) {
    resolveRef(config)->setLogger(logger);
  } else {
    resolveRef(config)->setLogger(getDefaultLogger());
  }
}

void YGConfigSetContext(const YGConfigRef config, void* context) {
  resolveRef(config)->setContext(context);
}

void* YGConfigGetContext(const YGConfigConstRef config) {
  return resolveRef(config)->getContext();
}

void YGConfigSetExperimentalFeatureEnabled(
    const YGConfigRef config,
    const YGExperimentalFeature feature,
    const bool enabled) {
  resolveRef(config)->setExperimentalFeatureEnabled(
      scopedEnum(feature), enabled);
}

bool YGConfigIsExperimentalFeatureEnabled(
    const YGConfigConstRef config,
    const YGExperimentalFeature feature) {
  return resolveRef(config)->isExperimentalFeatureEnabled(scopedEnum(feature));
}

void YGConfigSetCloneNodeFunc(
    const YGConfigRef config,
    const YGCloneNodeFunc callback) {
  resolveRef(config)->setCloneNodeCallback(callback);
}
