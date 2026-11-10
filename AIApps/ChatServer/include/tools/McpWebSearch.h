#pragma once 

#include <boost/json.hpp>

#include <string>

bool mcpSearchConfigured();

std::string mcpWebSearch(const boost::json::object& args);