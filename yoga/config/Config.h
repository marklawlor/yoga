/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <bitset>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <yoga/Yoga.h>
#include <yoga/enums/Errata.h>
#include <yoga/enums/ExperimentalFeature.h>
#include <yoga/enums/LogLevel.h>
#include <yoga/numeric/FloatOptional.h>

// Tag struct used to form the opaque YGConfigRef for the public C API
struct YGConfig {};

namespace facebook::yoga {

class Config;
class Node;

using ExperimentalFeatureSet = std::bitset<ordinalCount<ExperimentalFeature>()>;

// Whether moving a node from an old to new config should dirty previously
// calculated layout results.
bool configUpdateInvalidatesLayout(
    const Config& oldConfig,
    const Config& newConfig);

// CSS env() name interning. A PROCESS-GLOBAL, append-only, thread-safe registry
// maps env variable names to stable uint16 ids, shared across all Configs and
// threads. It is global (not per-Config) so an expression can be serialised
// with no Config in scope — e.g. React Native's Fabric prop-conversion path,
// which has no node/config — and so a stored id is valid in ANY Config (each
// Config holds only the id -> value mapping). The registry is touched only at
// serialise / setEnv time, never in the layout hot path (evaluate reads the
// per-Config value vector by id directly).
//
// internEnvName registers `name` if absent and returns its id. lookupEnvName
// never inserts and returns kEnvNameNotFound for an unregistered name.
inline constexpr uint16_t kEnvNameNotFound = 0xFFFF;
uint16_t internEnvName(std::string_view name);
uint16_t lookupEnvName(std::string_view name);

class YG_EXPORT Config : public ::YGConfig {
 public:
  explicit Config(YGLogger logger);

  void setUseWebDefaults(bool useWebDefaults);
  bool useWebDefaults() const;

  void setExperimentalFeatureEnabled(ExperimentalFeature feature, bool enabled);
  bool isExperimentalFeatureEnabled(ExperimentalFeature feature) const;
  ExperimentalFeatureSet getEnabledExperiments() const;

  void setErrata(Errata errata);
  void addErrata(Errata errata);
  void removeErrata(Errata errata);
  Errata getErrata() const;
  bool hasErrata(Errata errata) const;

  void setPointScaleFactor(float pointScaleFactor);
  float getPointScaleFactor() const;

  // CSS env() support.
  //
  // Env variable names are interned to a stable uint16 id by the PROCESS-GLOBAL
  // registry (see internEnvName, above) — not per-Config — so an expression can
  // be serialised with no Config in scope (e.g. React Native's Fabric prop
  // path) and the id is valid in any Config. Each Config holds only the
  // id -> value mapping (envValues_); an undefined FloatOptional means the name
  // is unset on this config. Only setEnv / removeEnv (which change a value) bump
  // version_ and thereby invalidate cached layout under this config.
  FloatOptional getEnvValueById(uint16_t id) const;
  void setEnv(std::string_view name, FloatOptional value);
  void removeEnv(std::string_view name);
  FloatOptional getEnv(std::string_view name) const;
  // Replaces this config's entire env value table with `src`'s. Valid because
  // env name ids are process-global (see internEnvName), so the id -> value
  // vector is directly comparable across configs with no remapping. Bumps
  // version_ (and invalidates cached layout) only if the table actually
  // changed. Used to carry env values forward across cloned configs.
  void copyEnvFrom(const Config& src);

  void setContext(void* context);
  void* getContext() const;

  uint32_t getVersion() const noexcept;

  void setLogger(YGLogger logger);
  void log(
      const yoga::Node* node,
      LogLevel logLevel,
      const char* format,
      va_list args) const;

  void setCloneNodeCallback(YGCloneNodeFunc cloneNode);
  YGNodeRef
  cloneNode(YGNodeConstRef node, YGNodeConstRef owner, size_t childIndex) const;

  static const Config& getDefault();

 private:
  YGCloneNodeFunc cloneNodeCallback_{nullptr};
  YGLogger logger_{};

  bool useWebDefaults_ : 1 = false;

  uint32_t version_ = 0;
  ExperimentalFeatureSet experimentalFeatures_{};
  Errata errata_ = Errata::None;
  float pointScaleFactor_ = 1.0f;
  void* context_ = nullptr;

  // env() value store, indexed by the process-global env name id (see
  // internEnvName). `mutable` so a value slot can be sized/seeded through a
  // const Config*. An id past the end of the vector is treated as unset, so a
  // name registered globally but never set on this config resolves to undefined.
  // Holds values only; the name -> id mapping is global, not here.
  mutable std::vector<FloatOptional> envValues_{};
};

inline Config* resolveRef(const YGConfigRef ref) {
  return static_cast<Config*>(ref);
}

inline const Config* resolveRef(const YGConfigConstRef ref) {
  return static_cast<const Config*>(ref);
}

} // namespace facebook::yoga
