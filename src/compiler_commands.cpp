#include "compiler_commands.hpp"


// %define ['global'] ['eval'] <macro>[([<params>...])] [value...]
Result defineMacro(const vector<Expression> &line_, MacroMap &rGlobalMacros_, MacroMap &rLocalMacros_, MacroRefMap &rMacroRefMap_) {

	if (line_.size() < 2) return { InvalidArgumentCount, "1 or more" }; // We don't count the command

	size_t offset = 0;

	bool global = line_[1].type == Expression::Identifier && line_[1].stringVal == "global";
	offset += global;

	bool evaluate = line_[1 + offset].type == Expression::Identifier && line_[1 + offset].stringVal == "eval";
	offset += evaluate;

	const Expression &macroName = line_[1 + offset];
	if (macroName.type != Expression::Identifier) return { UnexpectedToken, macroName.toString().stringVal };

	bool hasParams = line_[2 + offset].type == Expression::NestedExpression;
	if (hasParams) {
		for (auto &e : line_[2 + offset].expressions) // Arguments can be whatever we want, but parameters must be Identifiers
			if (e.type != Expression::Identifier) {
				hasParams = false;
				break;
			}
	}
	offset += hasParams;

	Expression macro;
	macro.type = Expression::NestedExpression;
	macro.expressions.push_back({ vector<Expression>(line_.begin() + 2 + offset, line_.end()) });
	if (hasParams) macro.expressions.push_back(line_[2 + offset - 1]);

	if (evaluate) {
		Result err = macro.replaceMacros(rMacroRefMap_);
		if (err.code != NoError) return err;
	}

	(global ? rGlobalMacros_ : rLocalMacros_)[macroName.stringVal] = macro;
	genFinalMacroMap(rMacroRefMap_, rLocalMacros_, rGlobalMacros_); // We generate a map of macros again, in case there was a reallocation
	// TODO // do this more efficiently

	return {};
}

// %undef ['global'] <macro>
Result undefMacro(const vector<Expression> &line_, MacroMap &rGlobalMacros_, MacroMap &rLocalMacros_, MacroRefMap &rMacroRefMap_) {

	bool global = line_[1].type == Expression::Identifier && line_[1].stringVal == "global";

	(global ? rGlobalMacros_ : rLocalMacros_).erase(line_[1 + global].stringVal);
	return { InvalidArgumentCount, "1 or 2" };

	genFinalMacroMap(rMacroRefMap_, rLocalMacros_, rGlobalMacros_);

	return {};
}

// %if <cond>
Result ifCondition(const vector<Expression> &line_, vector<Expression> &rScript_, int &rGlobalLineIdx_, int &rLocalLineIdx_, const MacroRefMap &macroRefMap_) {

	if (line_.size() != 2) return { InvalidArgumentCount, "1" };

	Expression cond = line_[1].toBool();
	if (cond.type == Expression::Invalid) return { UnexpectedToken, line_[1].toString().stringVal };
	if (cond.intVal != 0) return {};

	int loopDepth = 1;
	for (int fileEndIdx = rGlobalLineIdx_ + 1; fileEndIdx < rScript_.size(); fileEndIdx++) {

		if (!rScript_[fileEndIdx].expressions.empty() && rScript_[fileEndIdx].expressions[0].type == Expression::Identifier) {
			if (rScript_[fileEndIdx].expressions[0].stringVal == "%if") loopDepth++;
			else if (rScript_[fileEndIdx].expressions[0].stringVal == "%endif") loopDepth--;

			if (loopDepth == 0) {
				rLocalLineIdx_ += fileEndIdx - rGlobalLineIdx_;
				rGlobalLineIdx_ = fileEndIdx;
				return {};
			}

			rScript_[fileEndIdx].expressions.clear(); // We clear the lines since other modules don't process %if commands
		}
	}

	return { LabelUsedButNotDefined, line_[1].stringVal };
}

static void makePathWhole(string &rPath_, const ProcessedFile &lastFile_) {
	if (rPath_.size() > 1 && rPath_[1] == ':') return; // Idk how it works on other systems. i've used only windows.. :/

	bool fromRootDir = rPath_.size() > 1 && rPath_.front() == '/';
	
	if (fromRootDir) rPath_.erase(0, 1);
	rPath_ = fromRootDir ? rPath_ : lastFile_.location.path + rPath_;
}

// %include <"['/']path/filename"> [args...]
Result includeFile(const vector<Expression> &line_, vector<Expression> &rScript_, int &rGlobalLineIdx_, vector<ProcessedFile> &rFileStack_, unordered_map<string, vector<Expression>> &rFiles_) {
	if (line_.size() < 2) return { InvalidArgumentCount, "1 or more" };

	Expression fileName = line_[1];
	fileName.simplify();
	if (fileName.type != Expression::String) return { UnexpectedToken, line_[0].toString().stringVal };

	string &pathStr = fileName.stringVal;
	makePathWhole(pathStr, rFileStack_.back());

	auto fileIt = rFiles_.find(pathStr); // We check if we have read this file before
	if (fileIt == rFiles_.end()) {
		fileIt = rFiles_.emplace(pathStr, vector<Expression>()).first;
		if (!readFile(fileIt->second, pathStr)) // If not, we read it from the folder
			return { FileNotFound, pathStr };
	}

	const vector<Expression> &includedScript = fileIt->second;

	{
		Expression filePushLine(vector<Expression>{ Expression::makeIdentifier("%file_push"), Expression::makeString(pathStr) });
		filePushLine.expressions.insert(filePushLine.expressions.end(), line_.begin() + 2, line_.end());

		rScript_[rGlobalLineIdx_] = filePushLine;
		rScript_.insert(rScript_.begin() + rGlobalLineIdx_ + 1, includedScript.begin(), includedScript.end());
		rScript_.insert(rScript_.begin() + rGlobalLineIdx_ + 1 + includedScript.size(), vector<Expression>{ Expression::makeIdentifier("%file_pop") });
	}

	rGlobalLineIdx_--; // We go back one line, because the %inlcude at script[l] got replaced

	return {};
}

// We define the contents of a file from the inside of another file. It makes libraries less messy.
// %file_def <"['/']path/filename">
Result defineFile(const vector<Expression> &line_, vector<Expression> &rScript_, int &rGlobalLineIdx_, vector<ProcessedFile> &rFileStack_, unordered_map<string, vector<Expression>> &rFiles_) {
	if (line_.size() != 2) return { InvalidArgumentCount, "2" };

	int loopDepth = 1;
	for (int fileEndIdx = rGlobalLineIdx_ + 1; fileEndIdx < rScript_.size(); fileEndIdx++) {

		if (!rScript_[fileEndIdx].expressions.empty() && rScript_[fileEndIdx].expressions[0].type == Expression::Identifier) {
			if (rScript_[fileEndIdx].expressions[0].stringVal == "%file_def") loopDepth++;
			else if (rScript_[fileEndIdx].expressions[0].stringVal == "%file_end") loopDepth--;
			
			if (loopDepth == 0) {
				string pathStr = line_[1].stringVal;
				makePathWhole(pathStr, rFileStack_.back());
				rFiles_[pathStr] = vector<Expression>(rScript_.begin() + rGlobalLineIdx_ + 1, rScript_.begin() + fileEndIdx);
				
				for (auto it = rScript_.begin() + rGlobalLineIdx_; it <= rScript_.begin() + fileEndIdx; it++) it->expressions.clear();

				rFileStack_.back().line += fileEndIdx - rGlobalLineIdx_; // TEST
				rGlobalLineIdx_ = fileEndIdx;
				return {};
			}
		}
	}

	return { ClosingTokenNotFound, line_[0].toString().stringVal };
}

// %file_push <path> [args...]
Result pushFile(const vector<Expression> &line_, vector<ProcessedFile> &rFileStack_, const MacroMap &globalMacros_, MacroRefMap &rMacroMap_) {
	if (line_.size() < 2) {
		return { InvalidArgumentCount, "1 or more" };
	}

	ProcessedFile newFile;
	newFile.line = -1; // We do l++ at the beginning of the next loop iteration
	newFile.location = line_[1].stringVal;

	for (size_t i = 2; i < line_.size(); i++) {
		newFile.macros["%arg" + numToStr(i - 2)] = Expression(vector<Expression>{ Expression(vector<Expression>{ line_[i] }) });
	}
		
	newFile.macros["%argn"] = Expression(vector<Expression>{ Expression(vector<Expression>{ Expression((int)line_.size() - 2) }) }); // This looks wrong, but that's how we store macros
	newFile.macros["%path"] = Expression(vector<Expression>{ Expression(vector<Expression>{ Expression::makeString(newFile.location.path) }) });
	newFile.macros["%name"] = Expression(vector<Expression>{ Expression(vector<Expression>{ Expression::makeString(newFile.location.name) }) });

	rFileStack_.back().line--;
	rFileStack_.push_back(newFile);

	genFinalMacroMap(rMacroMap_, rFileStack_.back().macros, globalMacros_);

	return {};
}

// %error [code] <text>
Result errorDirective(const vector<Expression> &line_) {
	if (line_.size() == 2) {
		return { ErrorDirective, line_[1].toString().stringVal };
	}
	else if (line_.size() == 3) {

		if (line_[1].type != Expression::Integer) {
			return { UnexpectedToken, line_[1].toString().stringVal };
		}

		if (line_[2].type != Expression::String) {
			return { UnexpectedToken, line_[2].toString().stringVal };
		}

		ErrorCode err = (ErrorCode)line_[1].intVal;
		if (line_[1].intVal < 0 || line_[1].intVal >= ErrorCode_End) err = ErrorDirective;
		return { err, line_[2].stringVal };
	}
	else {
		return { InvalidArgumentCount, "1 or 2" };
	}
}