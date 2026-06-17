#pragma once

#include "command_router.hpp"

namespace wef {

HRESULT runInstall(DbgSession& session, const Output& out, const std::vector<std::string>& args);
HRESULT runUninstall(DbgSession& session, const Output& out, const std::vector<std::string>& args);
HRESULT runAliases(DbgSession& session, const Output& out, const std::vector<std::string>& args);

}
