#pragma once

#include <stdexcept>
#include <string>

class ToDoError : std::runtime_error {
public:
    explicit ToDoError(const std::string& s)
        : std::runtime_error("TODO: " + s)
    {
    }
};
