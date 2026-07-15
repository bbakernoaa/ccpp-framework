/// @file ccpp_constituent_registry.cpp
/// @brief Implementation of ConstituentRegistry for the CCPP C++ Framework.

#include "ccpp/ccpp_constituent_registry.hpp"

#include <stdexcept>

namespace ccpp {

void ConstituentRegistry::declare(const ConstituentMetadata& meta) {
    if (name_index_.find(meta.standard_name) != name_index_.end()) {
        throw std::runtime_error(
            "ConstituentRegistry::declare: constituent '" + meta.standard_name +
            "' is already registered");
    }
    name_index_[meta.standard_name] = entries_.size();
    entries_.push_back(meta);
}

void ConstituentRegistry::bind_storage(const std::string& name, double* data,
                                       const std::array<int, 3>& dims) {
    if (name_index_.find(name) == name_index_.end()) {
        throw std::runtime_error(
            "ConstituentRegistry::bind_storage: constituent '" + name +
            "' was not declared");
    }
    storage_[name] = data;
    storage_dims_[name] = dims;
}

real64_3d ConstituentRegistry::get_view(const std::string& name) const {
    auto it = name_index_.find(name);
    if (it == name_index_.end()) {
        throw std::runtime_error(
            "ConstituentRegistry::get_view: constituent '" + name +
            "' was not declared");
    }
    auto storage_it = storage_.find(name);
    auto dims_it = storage_dims_.find(name);
    if (storage_it == storage_.end() || dims_it == storage_dims_.end()) {
        throw std::runtime_error(
            "ConstituentRegistry::get_view: constituent '" + name +
            "' has no bound storage");
    }
    return make_mdspan_left<double, 3>(storage_it->second, dims_it->second);
}

} // namespace ccpp
