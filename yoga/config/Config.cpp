/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <mutex>

#include <yoga/config/Config.h>
#include <yoga/debug/Log.h>
#include <yoga/node/Node.h>

namespace facebook::yoga {

namespace {
// Process-global, append-only env name -> id registry (see Config.h). A plain
// mutex guards the rare insert/lookup at serialise / setEnv time; the layout
// hot path never touches this (evaluate reads Config::envValues_ by id).
struct EnvNameRegistry {
  std::mutex mutex;
  std::unordered_map<std::string, uint16_t> nameToId;
};

EnvNameRegistry& envRegistry() {
  // Function-local static: thread-safe, lazy, no static-init-order issues.
  static EnvNameRegistry registry;
  return registry;
}
} // namespace

uint16_t internEnvName(std::string_view name) {
  auto& registry = envRegistry();
  std::lock_guard<std::mutex> lock(registry.mutex);
  auto it = registry.nameToId.find(std::string{name});
  if (it != registry.nameToId.end()) {
    return it->second;
  }
  // Append-only; ids are never reused. Cap at the uint16 ceiling: past it,
  // every further name aliases kEnvNameNotFound, which resolves to undefined
  // (no Config sizes its value vector that large). 65534 distinct env names is
  // far beyond any real use.
  if (registry.nameToId.size() >= kEnvNameNotFound) {
    return kEnvNameNotFound;
  }
  auto id = static_cast<uint16_t>(registry.nameToId.size());
  registry.nameToId.emplace(std::string{name}, id);
  return id;
}

uint16_t lookupEnvName(std::string_view name) {
  auto& registry = envRegistry();
  std::lock_guard<std::mutex> lock(registry.mutex);
  auto it = registry.nameToId.find(std::string{name});
  return it != registry.nameToId.end() ? it->second : kEnvNameNotFound;
}

bool configUpdateInvalidatesLayout(
    const Config& oldConfig,
    const Config& newConfig) {
  return oldConfig.getErrata() != newConfig.getErrata() ||
      oldConfig.getEnabledExperiments() != newConfig.getEnabledExperiments() ||
      oldConfig.getPointScaleFactor() != newConfig.getPointScaleFactor() ||
      oldConfig.useWebDefaults() != newConfig.useWebDefaults();
  // Note: env values are intentionally not compared here. version_ already
  // gates cached-layout reuse, and setEnv/removeEnv bump version_, so any env
  // value change forces a relayout via the existing configVersion check.
}

Config::Config(YGLogger logger) : logger_{logger} {
  // Pre-populate the standard safe-area-inset-* env names with a value of 0.
  // These register an id and a defined value of 0 directly (not via setEnv) so
  // the getDefault() singleton's constructor does not churn version_. Hosts may
  // override or remove them.
  for (std::string_view name :
       {"safe-area-inset-top",
        "safe-area-inset-right",
        "safe-area-inset-bottom",
        "safe-area-inset-left"}) {
    uint16_t id = internEnvName(name);
    if (id >= envValues_.size()) {
      envValues_.resize(static_cast<size_t>(id) + 1u, FloatOptional{});
    }
    envValues_[id] = FloatOptional{0.0f};
  }
}

void Config::setUseWebDefaults(bool useWebDefaults) {
  useWebDefaults_ = useWebDefaults;
}

bool Config::useWebDefaults() const {
  return useWebDefaults_;
}

void Config::setExperimentalFeatureEnabled(
    ExperimentalFeature feature,
    bool enabled) {
  if (isExperimentalFeatureEnabled(feature) != enabled) {
    experimentalFeatures_.set(static_cast<size_t>(feature), enabled);
    version_++;
  }
}

bool Config::isExperimentalFeatureEnabled(ExperimentalFeature feature) const {
  return experimentalFeatures_.test(static_cast<size_t>(feature));
}

ExperimentalFeatureSet Config::getEnabledExperiments() const {
  return experimentalFeatures_;
}

void Config::setErrata(Errata errata) {
  if (errata_ != errata) {
    errata_ = errata;
    version_++;
  }
}

void Config::addErrata(Errata errata) {
  if (!hasErrata(errata)) {
    errata_ |= errata;
    version_++;
  }
}

void Config::removeErrata(Errata errata) {
  if (hasErrata(errata)) {
    errata_ &= (~errata);
    version_++;
  }
}

Errata Config::getErrata() const {
  return errata_;
}

bool Config::hasErrata(Errata errata) const {
  return (errata_ & errata) != Errata::None;
}

void Config::setPointScaleFactor(float pointScaleFactor) {
  if (pointScaleFactor_ != pointScaleFactor) {
    pointScaleFactor_ = pointScaleFactor;
    version_++;
  }
}

float Config::getPointScaleFactor() const {
  return pointScaleFactor_;
}

FloatOptional Config::getEnvValueById(uint16_t id) const {
  if (id < envValues_.size()) {
    return envValues_[id];
  }
  return FloatOptional{};
}

void Config::setEnv(std::string_view name, FloatOptional value) {
  // YGUndefined / undefined clears the value (== removeEnv).
  uint16_t id = internEnvName(name);
  if (id == kEnvNameNotFound) {
    return; // env name id space exhausted (see internEnvName); ignore.
  }
  if (id >= envValues_.size()) {
    envValues_.resize(static_cast<size_t>(id) + 1u, FloatOptional{});
  }
  if (envValues_[id] != value) {
    envValues_[id] = value;
    version_++;
  }
}

void Config::removeEnv(std::string_view name) {
  setEnv(name, FloatOptional{});
}

FloatOptional Config::getEnv(std::string_view name) const {
  // Lookup-only: reading an env value must never register the name globally.
  // An unregistered name (kEnvNameNotFound) or one unset on this config both
  // resolve to undefined via getEnvValueById's bounds check.
  return getEnvValueById(lookupEnvName(name));
}

void Config::copyEnvFrom(const Config& src) {
  // Wholesale vector copy is valid because env name ids are process-global, so
  // src's id -> value mapping is meaningful in any config without remapping.
  if (envValues_ != src.envValues_) {
    envValues_ = src.envValues_;
    version_++;
  }
}

void Config::setContext(void* context) {
  context_ = context;
}

void* Config::getContext() const {
  return context_;
}

uint32_t Config::getVersion() const noexcept {
  return version_;
}

void Config::setLogger(YGLogger logger) {
  logger_ = logger;
}

void Config::log(
    const yoga::Node* node,
    LogLevel logLevel,
    const char* format,
    va_list args) const {
  logger_(this, node, unscopedEnum(logLevel), format, args);
}

void Config::setCloneNodeCallback(YGCloneNodeFunc cloneNode) {
  cloneNodeCallback_ = cloneNode;
}

YGNodeRef Config::cloneNode(
    YGNodeConstRef node,
    YGNodeConstRef owner,
    size_t childIndex) const {
  YGNodeRef clone = nullptr;
  if (cloneNodeCallback_ != nullptr) {
    clone = cloneNodeCallback_(node, owner, childIndex);
  }
  if (clone == nullptr) {
    clone = YGNodeClone(node);
  }
  return clone;
}

/*static*/ const Config& Config::getDefault() {
  static Config config{getDefaultLogger()};
  return config;
}

} // namespace facebook::yoga
