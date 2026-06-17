#pragma once

#include "command_router.hpp"

namespace wef {

HRESULT runHeap(DbgSession& session, const Output& out, const std::vector<std::string>& args);
HRESULT runHeapVis(DbgSession& session, const Output& out, const std::vector<std::string>& args);

}
