/*
  Created on 02.07.18.
*/

#include "logging.hpp"
#include <settings/Manager.hpp>

namespace settings
{

Manager &Manager::instance()
{
    static Manager object{};
    return object;
}

void Manager::add(IVariable &me, std::string name)
{
    if (registered.find(name) != registered.end())
    {
        logging::Info(("Double registering variable: " + name).c_str());
        throw std::runtime_error("Double registering variable: " + name);
    }
    registered.emplace(name, me);
}

void Manager::add(IVariable &me, std::string name, std::string value)
{
    if (registered.find(name) != registered.end())
    {
        logging::Info(("Double registering variable: " + name).c_str());
        throw std::runtime_error("Double registering variable: " + name);
    }
    registered.emplace(name, Manager::VariableDescriptor{ me, value });
}

IVariable *Manager::lookup(const std::string &string)
{
    auto it = registered.find(string);
    if (it != registered.end())
        return &it->second.variable;
    return nullptr;
}

Manager::VariableDescriptor::VariableDescriptor(IVariable &variable) : variable(variable)
{
    type     = variable.getType();
    defaults = variable.toString();
}

Manager::VariableDescriptor::VariableDescriptor(IVariable &variable, std::string value) : variable(variable)
{
    type     = variable.getType();
    defaults = value;
}

bool Manager::VariableDescriptor::isChanged()
{
    return variable.toString() != defaults;
}
} // namespace settings
