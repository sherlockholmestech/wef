#pragma once

#include "command_router.hpp"

namespace wef {

HRESULT runTtd(DbgSession& session, const Output& out, const std::vector<std::string>& args);

}
