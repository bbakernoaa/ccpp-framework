/// @file ccpp_group.cpp
/// @brief Implementation of the Group class for the CCPP C++ Framework.

#include "ccpp/ccpp_group.hpp"

#include <utility>

namespace ccpp {

Group::Group(const std::string& name,
             std::vector<std::unique_ptr<PipelineElement>> elements)
    : name_(name),
      elements_(std::move(elements)) {}

SchemeError Group::execute(const std::string& phase, SchemeContext& ctx) {
    for (auto& element : elements_) {
        SchemeError err = element->execute(phase, ctx);
        if (err) {
            return err;
        }
    }
    return {};
}

} // namespace ccpp
