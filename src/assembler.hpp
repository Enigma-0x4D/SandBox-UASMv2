#ifndef ASSEMBLER_HPP
#define ASSEMBLER_HPP

#include "common.hpp"
#include "parser.hpp"
#include "files.hpp"

enum ParamType { Register, Number, Invalid };

struct ParamTemplate {
	ParamType type;
	size_t begin;
	size_t bits;
};

struct InstructionTemplate {
	size_t byteNum = 0;
	vector<ParamTemplate> params;
};

struct InstrucitonParam {
	string str;
	ParamType type;
	int64_t value;

	InstrucitonParam() : type(Invalid), value(0) {}

	InstrucitonParam(const Expression &expr_) : type(Invalid), value(0) {

		if (expr_.type == Expression::Integer) {
			type = Number;
			value = expr_.intVal;
		}
		if (expr_.type == Expression::Float) {
			type = Number;
			value = (int64_t)expr_.floatVal;
		}
		else if (expr_.type == Expression::Identifier) {
			if (!expr_.stringVal.empty()) {
				if (expr_.stringVal[0] == 'R' || expr_.stringVal[0] == 'r') {
					type = Register;

					const char *begin = expr_.stringVal.data() + 1;
					const char *end = expr_.stringVal.data() + expr_.stringVal.size();

					auto result = std::from_chars(begin, end, value);
					if (result.ec != std::errc{} || result.ptr != end) type = Invalid;
				}
			}
		}
	}
};

Result assembleCode(vector<Expression> &rScript_, vector<Instruction> &rCode_, vector<Marker> &rMarkers_, bool addMarkers_, vector<ProcessedFile> &rFileStack_, int &rInstructionCount_);

#endif