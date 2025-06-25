#ifndef PREPROCESSOR_HPP
#define PREPROCESSOR_HPP

#include "common.hpp"
#include "parser.hpp"
#include "compiler_commands.hpp"

Result preprocessor(vector<Expression> &rTokScript_, vector<ProcessedFile> &rFileStack_);

#endif