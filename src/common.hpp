#ifndef COMMON_HPP
#define COMMON_HPP

#include <stdio.h>
#include <fstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <charconv>

using std::vector;
using std::string;
using std::unordered_map;
using std::pair;

using InstructionBytes = uint64_t;

struct Instruction {
	InstructionBytes bytes;
	size_t byteNum;
};

inline int bitNum(uint64_t val_) {
	int n = 0;
	while (val_ != 0) {
		val_ >>= 1;
		n++;
	}
	return n;
}

inline bool isDec(char c) {
	return c >= '0' && c <= '9';
}
inline bool isHex(char c) {
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
inline bool isBin(char c) {
	return c == '0' || c == '1';
}

enum BytePart : bool { Low, High };

enum ErrorCode {
	NoError,
	InvalidInstruction,
	UnexpectedToken,
	InvalidArgumentCount,
	LabelUsedButNotDefined,
	MultipleLabelDefinitions,
	FileNotFound,
	InvalidRange,
	InstructionTooShort,
	ClosingTokenNotFound,
	ErrorDirective,
	ErrorCode_End
};

struct Result {
	ErrorCode code = NoError;
	string str = "";

	Result(ErrorCode code_ = NoError, const string &str_ = "") : code(code_), str(str_) {
		if (0)
			if (code_ != NoError)
				throw; // DEBUG
	}

	string getErrorMessage() const {
		switch (code) {
		case InvalidInstruction:
			return "Invalid instruction: '" + str + "'.\n";

		case UnexpectedToken:
			return "Unexpected token: '" + str + "'.\n";

		case InvalidArgumentCount:
			return "Invalid argument count. Expected: " + str + ".\n";

		case LabelUsedButNotDefined:
			return "Label '" + str + "' used but not defined.\n";

		case MultipleLabelDefinitions:
			return "Label '" + str + "' defined multiple times.\n";

		case FileNotFound:
			return "File '" + str + "' not found.\n";

		case InvalidRange:
			return "Invalid range. Expected values: " + str + ".\n";

		case InstructionTooShort:
			return "Instruction too short for given parameters: '" + str + "'.\n";

		case ClosingTokenNotFound:
			return "Closing token for '" + str + "' not found.\n";

		case ErrorDirective:
			return str;

		default:
			return "";
		}
	}
};

#endif