#include "preprocessor.hpp"

Result preprocessor(vector<Expression> &rScript_, vector<ProcessedFile> &rFileStack_) {

	Result result = {};

	unordered_map<string, vector<Expression>> files;

	MacroMap globalMacros;
	MacroRefMap macroMap;

	rFileStack_.front().macros["%argn"] = Expression(vector<Expression>{ Expression(vector<Expression>{ Expression(0) }) });
	rFileStack_.front().macros["%path"] = Expression(vector<Expression>{ Expression(vector<Expression>{ Expression::makeString(rFileStack_.front().location.path) }) });
	rFileStack_.front().macros["%name"] = Expression(vector<Expression>{ Expression(vector<Expression>{ Expression::makeString(rFileStack_.front().location.name) }) });

	for (auto &l : rFileStack_.back().macros)
		macroMap[l.first] = &l.second;

	for (int l = 0; l < rScript_.size(); l++, rFileStack_.back().line++) {

		Expression &rThisExpr = rScript_[l];
		vector<Expression> &line = rThisExpr.expressions;
		
		if (line.empty()) continue;

		if (line[0].type != Expression::Identifier) return { UnexpectedToken, line[0].toString().stringVal }; // There always should be an identifier at the beginning
		
		if (line.size() == 2 && line[1].type == Expression::Invalid && line[1].stringVal == ":") continue;

		if (line[0].stringVal != "%define" && line[0].stringVal != "%undef")
			rThisExpr.replaceMacros(macroMap); // [!] There may be an reallocation

		if (line[0].stringVal.front() != '%') {
			if (line[0].stringVal.front() != '_') return { UnexpectedToken, line[0].toString().stringVal };
			else continue;
		}

		const string &command = line[0].stringVal;

		if (line.empty()) continue;

		if (command == "%define") { // %define ['global'] ['eval'] <macro> [value...]
			result = defineMacro(line, globalMacros, rFileStack_.back().macros, macroMap);
		}
		else if (command == "%undef") { // %undef ['global'] <macro>
			result = undefMacro(line, globalMacros, rFileStack_.back().macros, macroMap);
		}
		else if (command == "%include") { // %include <file> [args...]
			result = includeFile(line, rScript_, l, rFileStack_, files);
		}
		else if (command == "%if") { // %if <cond>
			result = ifCondition(line, rScript_, l, rFileStack_.back().line, macroMap);
		}
		else if (command == "%endif") { // %if <cond>
			// Do nothing
		}
		else if (command == "%file_def") { // %file_def <name>
			result = defineFile(line, rScript_, l, rFileStack_, files);
		}
		else if (command == "%file_end") { // %file_end
			if (line.size() != 1)
				return { InvalidArgumentCount, "0" };
		}
		else if (command == "%file_push") { // %file_push <path> [args...]
			result = pushFile(line, rFileStack_, globalMacros, macroMap);
		}
		else if (command == "%file_pop") { // %file_pop
			if (line.size() != 1)
				return { InvalidArgumentCount, "0" };

			if (!rFileStack_.empty()) {
				rFileStack_.pop_back();
				genFinalMacroMap(macroMap, rFileStack_.back().macros, globalMacros);
			}
		}
		else if (command == "%marker") { // %marker <text>
			// To prevent UnknownInstruction error
		}
		else if (command == "%error") { // %error [code] <text>
			result = errorDirective(line);
		}
		else {
			return { InvalidInstruction, command };
		}

		if (result.code != NoError) break;
	}

	return result;
}