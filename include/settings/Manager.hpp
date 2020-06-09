/*
  Created on 02.07.18.
*/

#pragma once

#include <unordered_map>
#include <vector>
#include "Settings.hpp"

/*
 * Default value = value when variable was registered
 *
 * Manager needs to be able to:
 * - Save config
 * - Load config
 * - Get value by name
 * - Set value by name
 * - Reset variable to defaults
 * - Reset everything to defaults
 * - Rebase config (hmmm)
 */

namespace settings
{

class Manager
{
public:
    struct VariableDescriptor
    {
        explicit VariableDescriptor(IVariable &variable);
        VariableDescriptor(IVariable &variable, std::string value);

        bool isChanged();

        IVariable &variable;
        std::string defaults{};
        VariableType type{};
    };

    static Manager &instance();

public:
    void add(IVariable &me, std::string name);
    void add(IVariable &me, std::string name, std::string value);
    IVariable *lookup(const std::string &string);

    std::unordered_map<std::string, VariableDescriptor> registered{};
};
} // namespace settings
