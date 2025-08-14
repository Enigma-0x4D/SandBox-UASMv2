#ifndef COMPILER_COMMANDS_HPP
#define COMPILER_COMMANDS_HPP

#include "common.hpp"
#include "parser.hpp"
#include "files.hpp"

Result defineMacro(const vector<Expression> &line_, MacroMap &rGlobalMacros_, MacroMap &rLocalMacros_, MacroRefMap &rMacroRefMap_);
Result undefMacro(const vector<Expression> &line_, MacroMap &rGlobalMacros_, MacroMap &rLocalMacros_, MacroRefMap &rMacroRefMap_);
Result ifCondition(const vector<Expression> &line_, vector<Expression> &rScript_, int &rGlobalLineIdx_, int &rLocalLineIdx_, const MacroRefMap &macroRefMap_);
Result includeFile(const vector<Expression> &line_, vector<Expression> &rScript_, int &rGlobalLineIdx_, vector<ProcessedFile> &rFileStack_, unordered_map<string, vector<Expression>> &rFiles_);
Result defineFile(const vector<Expression> &line_, vector<Expression> &rScript_, int &rGlobalLineIdx_, vector<ProcessedFile> &rFileStack_, unordered_map<string, vector<Expression>> &rFiles_);
Result pushFile(const vector<Expression> &line_, vector<ProcessedFile> &rFileStack_, const MacroMap &globalMacros_, MacroRefMap &rMacroMap_);
Result inheritMacros(const vector<Expression> &line_, vector<ProcessedFile> &rFileStack_, const MacroMap &globalMacros_, MacroRefMap &rMacroMap_);
Result errorDirective(const vector<Expression> &line_);

#endif