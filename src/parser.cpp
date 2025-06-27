#include "parser.hpp"

static bool removeCommentsInLine(string &rStr_, const string &multilineCommentBegin_, const string &multilineCommentEnd_) {
	size_t multilineCommBeg = 0, multilineCommEnd = 0;

	while (true) {
		multilineCommBeg = rStr_.find(multilineCommentBegin_, multilineCommBeg);
		multilineCommEnd = rStr_.find(multilineCommentEnd_, multilineCommBeg);

		if (multilineCommBeg != string::npos) {
			if (multilineCommEnd != string::npos) {
				rStr_.erase(rStr_.begin() + multilineCommBeg, rStr_.begin() + multilineCommEnd + multilineCommentBegin_.size());
			}
			else {
				rStr_.erase(rStr_.begin() + multilineCommBeg, rStr_.end());
				return 1;
			}
		}
		else return 0;
	}
}

void removeComments(vector<string> &rScript_, const string &singleLineCommentBegin_, const string &multilineCommentBegin_, const string &multilineCommentEnd_) {

	for (int l = 0; l < rScript_.size(); l++) {
		size_t singleLineCommBeg = rScript_[l].find(singleLineCommentBegin_);
		if (singleLineCommBeg != string::npos) rScript_[l].erase(singleLineCommBeg);

		bool inMultilineComment = removeCommentsInLine(rScript_[l], multilineCommentBegin_, multilineCommentEnd_);

		if (inMultilineComment) {
			while (true) {
				if (++l >= rScript_.size()) return;

				size_t multilineCommEnd = rScript_[l].find(multilineCommentEnd_);
				if (multilineCommEnd != string::npos) {
					rScript_[l--].erase(0, multilineCommEnd + multilineCommentEnd_.size());
					break;
				}

				rScript_[l].clear();
			}
		}
	}
}

void getNextToken(const char *&rTokBeg_, const char *&rTokEnd_, const char *end_, const char *delim_, const char *range_, const char *string_) {

	// [ ] token
	while (true) {
		if (rTokBeg_ == end_) {
			rTokEnd_ = rTokBeg_;
			return;
		}

		if (!isOneOf(*rTokBeg_, delim_)) break;
		rTokBeg_++;
	}
	// [t]oken

	rTokEnd_ = rTokBeg_;

	for (const char *strInd = string_; *strInd != '\0'; strInd++)
	if (*rTokBeg_ == *strInd) { // ["]some(string)"
		rTokEnd_ = rTokBeg_ + 2; // "s[o]me(string)"  // ""[ ]
		while (true) {
			if (rTokEnd_ >= end_) {
				rTokEnd_ = end_;  // aaa" [ ]  ->  aaa"[ ]
				return;
			}
			if (*(rTokEnd_ - 1) == *strInd) return;
			rTokEnd_++;
		}
	} // "some(string)"[ ]

	for (const char *rangeInd = range_; *rangeInd != '\0'; rangeInd += 2)
		if (*rTokBeg_ == rangeInd[0]) { // [(]Ala ma (kota ))
			int nestNum = 1;

			rTokEnd_ = rTokBeg_ + 2; // (A[l]a ma (kota ))
			while (true) {
				if (rTokEnd_ >= end_) {
					rTokEnd_ = end_;
					return;
				}
				if (*(rTokEnd_ - 1) == rangeInd[0]) nestNum++;
				else if (*(rTokEnd_ - 1) == rangeInd[1]) nestNum--;
				if (nestNum == 0) return;

				rTokEnd_++;
			}
		}

	bool isIdentifier = isPartOfName(*rTokBeg_);

	while (true) {
		if (rTokEnd_ == end_) return;
		if (isOneOf(*rTokEnd_, delim_) || isOneOf(*rTokEnd_, range_) || isOneOf(*rTokEnd_, string_)) return;
		if (isPartOfName(*rTokEnd_) != isIdentifier) return;
		rTokEnd_++;
	}
}

vector<string> split(const char *begin_, const char *end_, const char *delim_, const char *range_, const char *string_) {
	const char *begin = begin_, *end;
	vector<string> result;
	while (true) {
		getNextToken(begin, end, end_, delim_, range_, string_);
		if (begin == end_) return result;
		result.push_back(string(begin, end));
		begin = end;
	}
}

void tokenizeScript(vector<string> &rScript_, vector<Expression> &rTokens_) {
	rTokens_.reserve(rScript_.size());
	for (auto &s : rScript_)
		rTokens_.push_back(Expression(split(s.data(), s.data() + s.size(), " \t")));
}

void genFinalMacroMap(MacroRefMap &rMacroMap_, const MacroMap &local_, const MacroMap &global_) {

	rMacroMap_.clear();
	for (auto &l : local_) {
		rMacroMap_[l.first] = &l.second;
	}

	for (auto &g : global_) {
		if (rMacroMap_.find(g.first) == rMacroMap_.end())
			rMacroMap_.emplace(g.first, &g.second);
	}
}

bool Expression::parse(const char *begin_, const char *end_) {
	
	type = Expression::Type::Invalid;

	vector<string> vec = split(begin_, end_, " \t", "()");
	if (vec.empty()) return 0;

	if (vec.size() != 1) {
		type = Expression::Type::NestedExpression;

		for (auto &v : vec)
			expressions.push_back(Expression(v));
		
		return 1;
	}

	string &str = vec[0];

	if (str.front() == '(' && str.back() == ')') { // (((...(x)...))) will be reduced to (x), but that's fine ig
		type = Expression::Type::NestedExpression;
		Expression expr = Expression(str.data() + 1, str.data() + str.size() - 1);
		
		if (expr.type == Invalid && expr.stringVal.empty()) return 1; // for cases like () or (   )
		
		if (expr.type == NestedExpression) *this = expr;
		else expressions.push_back(expr);

		return 1;
	}
	
	if (str.front() == '"' && str.back() == '"') {
		type = Expression::Type::String;
		stringVal = str.substr(1, str.size() - 2);
		return 1;
	}

	if (str.size() == 3 && str.front() == '\'' && str.back() == '\'') { // TODO // Add support for special characters
		type = Expression::Type::Integer;
		intVal = str[1];
		return 1;
	}
	
	if (isDec(str[0])) { // Starts with digit -> Number
		enum Base { Dec = 10, Hex = 16, Bin = 2 } base = Dec;
		if (str.size() > 2 && str[0] == '0') {
			if (str[1] == 'x') base = Hex;
			if (str[1] == 'b') base = Bin;
		}

		type = Expression::Type::Integer;
		std::from_chars_result result = std::from_chars(str.data() + (base != Dec) * 2, str.data() + str.size(), intVal, base);
		if (result.ec == std::errc{} && result.ptr == str.data() + str.size()) return 1;
		
		if (base == Dec) {
			type = Expression::Type::Float;
			std::from_chars_result result = std::from_chars(str.data(), str.data() + str.size(), floatVal);
			if (result.ec == std::errc{} && result.ptr == str.data() + str.size()) return 1;
		}

		type = Expression::Type::Invalid;
		stringVal = str;
		return 0;
	}

	MathOperEnum oper = getOperEnum(str);
	if (oper != MathOperEnum::InvalidOper) {
		type = Expression::Type::Operator;
		operVal = oper;
		return 1;
	}
	else if (isNameValid(str)) {
		type = Expression::Type::Identifier;
		stringVal = str;
		return 1;
	}
	else {
		type = Expression::Type::Invalid;
		stringVal = str;
		return 0;
	}
}

void Expression::replaceMacroArguments(const MacroRefMap &argMap_) { // Here we store just the name and value of the parameter. No additional data like in normal macros
	// It will not be possible to do things like:
	//	%define macro(a b) a * b
	//	macro(x (y + z)) -> x * y + z
	// but I doubt anyone will notice let alone use this, so i'll leave it like this

	if (type == NestedExpression) {
		for (int e = 0; e < expressions.size(); e++)
			expressions[e].replaceMacroArguments(argMap_);
	}
	else if (type == Identifier) {
		auto it = argMap_.find(stringVal);
		if (it != argMap_.end())
			*this = *(it->second);
	}
}

Result Expression::replaceMacros(const MacroRefMap &macroMap_) {
	
	if (type == NestedExpression) {
		for (int e = expressions.size() - 1; e >= 0; e--) {
			
			if (expressions[e].type == NestedExpression) {
				Result err = expressions[e].replaceMacros(macroMap_);
				if (err.code != NoError) return err;
			}
			else if (expressions[e].type == Identifier) {
				auto it = macroMap_.find(expressions[e].stringVal);
				if (it != macroMap_.end()) {
					const Expression &macro = *(it->second);

					bool hasParams = macro.expressions.size() == 2;
					int expectedArgNum = hasParams ? macro.expressions[1].expressions.size() : 0;

					Expression macroValue = macro.expressions[0];

					macroValue.replaceMacros(macroMap_);
					// Since we read the tokens from the end of the line, the macros in arguments will already be replaced

					if (expectedArgNum != 0) {
						if (e == expressions.size() - 1 || expressions[e + 1].expressions.size() != expectedArgNum) {
							return { InvalidArgumentCount, numToStr(expectedArgNum) };
						}

						if (expressions[e + 1].type != NestedExpression) {
							return { UnexpectedToken, expressions[e + 1].toString().stringVal };
						}
					
						const Expression &paramNames = macro.expressions[1];

						MacroRefMap argMacros;
						for (int a = 0; a < expectedArgNum; a++) {
							argMacros[paramNames.expressions[a].stringVal] = &expressions[e + 1].expressions[a];
						}

						macroValue.replaceMacroArguments(argMacros);

						expressions.erase(expressions.begin() + e, expressions.begin() + e + 2);
					}
					else {
						expressions.erase(expressions.begin() + e);
					}
					
					expressions.insert(expressions.begin() + e, macroValue.expressions.begin(), macroValue.expressions.end());
				}
			}			
		}
	}

	return NoError;
}

//	!! We don't replace macros in this function !!
//			  It should be done before

bool Expression::simplify(bool keepOperationOrder_) {
	if (type == Invalid || type == Identifier) return false;

	if (type == NestedExpression) {
		if (expressions.empty()) return false;

		if (expressions.size() == 1) {
			*this = Expression(expressions[0]);
			return simplify(keepOperationOrder_);
		}

		for (int e = expressions.size() - 2; e >= 0; e--) {
			if (expressions[e].type == Operator) {
				switch (expressions[e].operVal) {
				case MakeInt:
					expressions[e + 1] = expressions[e + 1].toInt();
					expressions.erase(expressions.begin() + e);
					break;

				case MakeFloat:
					expressions[e + 1] = expressions[e + 1].toFloat();
					expressions.erase(expressions.begin() + e);
					break;

				case MakeString:
					expressions[e + 1] = expressions[e + 1].toString();
					expressions.erase(expressions.begin() + e);
					break;
				}
			}
		}

		bool canBeSimplified = true;
		for (auto &e : expressions) {
			canBeSimplified &= e.simplify(keepOperationOrder_);

			if (e.type == Invalid) { // This can lead to some weird things, like the expression becoming invalid after simplifying. It's just important to be aware of that.
				*this = {};
				return false;
			}
		}

		if (!canBeSimplified) return false;

		for (int e = expressions.size() - 2; e >= 0; e--) { // ! and - are processed separately before other opers // idk if there are any other 1-operand operators
			if (expressions[e].type == Operator && (e == 0 || expressions[e - 1].type == Operator)) {
				switch (expressions[e].operVal) {
				case Not:
					expressions[e + 1] = expressions[e + 1].toBool();
					break;

				case BinNot:
					if (expressions[e + 1].type != Integer) {
						*this = {};
						return false;
					}
					expressions[e + 1] = Expression(~expressions[e + 1].intVal);
					break;

				case Minus:
					switch (expressions[e + 1].type) {
					case Integer:
						expressions[e + 1].intVal = -expressions[e + 1].intVal;
						break;
					case Float:
						expressions[e + 1].floatVal = -expressions[e + 1].floatVal;
						break;
					default:
						*this = {};
						return false;
					}
					break;
				}

				expressions.erase(expressions.begin() + e);
			}
		}

		// VVV We are left with an insimplifable list of floats, ints and opers, ... and other things, ... but we can do operation on all of them, so it's fine VVV

		if ((expressions.size() & 1) == 0) { // A x B x C ... D   -> size() is odd
			*this = {}; // Since we're left with a list of numbers that cannot be simplified, the expression is invalid.
			return false;
		}

		for (int e = 0; e < expressions.size(); e++) {
			if ((e % 2 == 0 && expressions[e].type == Operator) ||
				(e % 2 == 1 && (expressions[e].type == Integer || expressions[e].type == Float || expressions[e].type == String))) {
				*this = {}; // Same as above
				return false;
			}
		}

		if (keepOperationOrder_) {

			while (expressions.size() != 1) { // This can be done more efficiently, but i'm too lazy/tired/dumb to do this ;)
				int e = 0;
				while (e + 3 < expressions.size() && operPrecedence[expressions[e + 1].operVal] < operPrecedence[expressions[e + 3].operVal]) e += 2;
				expressions[e] = operation(expressions[e], expressions[e + 1].operVal, expressions[e + 2]);
				expressions.erase(expressions.begin() + e + 1, expressions.begin() + e + 3);
			}
			
		}
		else {
			for (int e = 0; e + 2 < expressions.size(); e += 2)
				expressions[0] = operation(expressions[0], expressions[e + 1].operVal, expressions[e + 2]);
		}

		*this = Expression(expressions[0]);
	}

	return true;
}

Expression Expression::toString(bool removeTrailingZeros_) const {
	
	switch (type) {
	case Expression::Type::Integer: return Expression::makeString(numToStr(intVal, std::chars_format::fixed, removeTrailingZeros_));
	case Expression::Type::Float: return Expression::makeString(numToStr(floatVal, std::chars_format::fixed, removeTrailingZeros_));
	case Expression::Type::String:
	case Expression::Type::Identifier:
	case Expression::Type::Invalid: return Expression::makeString(this->stringVal);
	case Expression::Type::NestedExpression: return Expression::makeString("(...)"); // Sorry, I don't have time nor energy to implement that
	default: return {};
	}
}

Expression Expression::toFloat() const {
	switch (type) {
	case Expression::Type::Integer: return Expression((float)intVal);
	case Expression::Type::Float: return *this;
	case Expression::Type::String: {
		float val;
		if (!strToNum(stringVal, val)) return {};
		return val;
	}
	default: return {};
	}
}

Expression Expression::toInt() const {
	switch (type) {
	case Expression::Type::Integer: return intVal;
	case Expression::Type::Float: return (int)floatVal;
	case Expression::Type::String: {
		int val;
		if (!strToNum(stringVal, val)) return {};
		return val;
	}
	default: return {};
	}
}

Expression Expression::toBool() const {
	Expression result = *this;
	result.simplify();

	switch (result.type) {
	case Expression::Type::Float: return result.floatVal != 0.f;
	case Expression::Type::Integer: return result.intVal != 0;
	case Expression::Type::NestedExpression: // <-- Should be evaluated ealier. If it didn't, it means it contains an identifier.
	case Expression::Type::Identifier: return false; // This will be useful for checking if a macro is defined.
	case Expression::Type::String: return true; // why not?
	case Expression::Type::Invalid:
	case Expression::Type::Operator: return {};
	}
}

Expression Expression::operation(const Expression &a_, MathOperEnum oper_, const Expression &b_) {
	if (a_.type == Expression::Type::Invalid || b_.type == Expression::Type::Invalid) return Expression();

	if (oper_ == Plus && (a_.type == Expression::Type::String || b_.type == Expression::Type::String))
		return Expression::makeString(a_.toString().stringVal + b_.toString().stringVal);
	if (oper_ == Times && (a_.type == Expression::Type::String && b_.type == Expression::Type::Integer)) {
		Expression result;
		result.type = Expression::Type::String;
		for (int i = 0; i < b_.intVal; i++)
			result.stringVal += a_.stringVal;
		return result;
	}
		
	if (a_.type == Expression::Type::String && b_.type == Expression::Type::String)
		return calcStrStr(a_.stringVal, oper_, b_.stringVal);
	if (a_.type == Expression::Type::Integer && b_.type == Expression::Type::Integer)
		return calcIntInt(a_.toInt().intVal, oper_, b_.toInt().intVal);
	
	return calcFloatFloat(a_.toFloat().floatVal, oper_, b_.toFloat().floatVal);
}

Expression Expression::calcStrStr(const string &a_, MathOperEnum oper_, const string &b_) {
	switch (oper_) {
	case IsEqualTo:				return Expression(a_ == b_);
	case IsNotEqualTo:			return Expression(a_ != b_);
	case IsGreaterThan:			return Expression(a_ > b_); // I didn't even know it's a thing in c++...
	case IsLessThan:			return Expression(a_ < b_);
	case IsGreaterOrEqualTo:	return Expression(a_ >= b_);
	case IsLessOrEqualTo:		return Expression(a_ <= b_);
	}

	return Expression();
}

Expression Expression::calcIntInt(int a_, MathOperEnum oper_, int b_) {
	switch (oper_) {
	case Plus:					return Expression(a_ + b_);
	case Minus:					return Expression(a_ - b_);
	case Times:					return Expression(a_ * b_);
	case DividedBy:				return Expression(a_ / b_);
	case ToThePowerOf: {
		int result = 1;
		for (int i = 0; i < b_; i++)
			result *= a_;
		return result;
	}

	case IsEqualTo:				return Expression(a_ == b_);
	case IsNotEqualTo:			return Expression(a_ != b_);
	case IsGreaterThan:			return Expression(a_ > b_);
	case IsLessThan:			return Expression(a_ < b_);
	case IsGreaterOrEqualTo:	return Expression(a_ >= b_);
	case IsLessOrEqualTo:		return Expression(a_ <= b_);

	case Or:					return Expression(((a_ != 0) || (b_ != 0)));
	case And:					return Expression(((a_ != 0) && (b_ != 0)));

	case BinOr:				return Expression((int)a_ | (int)b_);
	case BinAnd:			return Expression((int)a_ & (int)b_);
	case BinXor:			return Expression((int)a_ ^ (int)b_);

	case BitShR:			return Expression(a_ >> b_);
	case BitShL:			return Expression(a_ << b_);
	}
	
	return Expression();
}

Expression Expression::calcFloatFloat(float a_, MathOperEnum oper_, float b_) {
	switch (oper_) {
	case Plus:					return Expression(a_ + b_);
	case Minus:					return Expression(a_ - b_);
	case Times:					return Expression(a_ * b_);
	case DividedBy:				return Expression(a_ / b_);
	case ToThePowerOf:			return Expression(pow(a_, b_));

	case IsEqualTo:				return Expression(a_ == b_);
	case IsNotEqualTo:			return Expression(a_ != b_);
	case IsGreaterThan:			return Expression(a_ > b_);
	case IsLessThan:			return Expression(a_ < b_);
	case IsGreaterOrEqualTo:	return Expression(a_ >= b_);
	case IsLessOrEqualTo:		return Expression(a_ <= b_);

	case Or:					return Expression(((a_ != 0) || (b_ != 0)));
	case And:					return Expression(((a_ != 0) && (b_ != 0)));
	}

	return Expression();
}

const unordered_map<string, MathOperEnum> Expression::operStrToEnum{
	{ "**", ToThePowerOf },
	
	{ "*", Times },
	{ "/", DividedBy },
	{ "%", Modulo },

	{ "+", Plus },
	{ "-", Minus },
	
	{ ">>", BitShR },
	{ "<<", BitShL },

	{ ">", IsGreaterThan },
	{ "<", IsLessThan },
	{ ">=", IsGreaterOrEqualTo },
	{ "<=", IsLessOrEqualTo },

	{ "==", IsEqualTo },
	{ "!=", IsNotEqualTo },

	{ "&", BinAnd },

	{ "^", BinXor },

	{ "|", BinOr },

	{ "~", BinNot },

	{ "&&", And },

	{ "||", Or },

	{ "!", Not },

	{ "int", MakeInt },
	{ "float", MakeFloat },
	{ "string", MakeString }
};