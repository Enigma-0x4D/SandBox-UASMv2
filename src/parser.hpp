#ifndef PARSER_HPP
#define PARSER_HPP

#include "common.hpp"

inline bool strStartsWith(const string &str_, const string &starts_, size_t off_ = 0) {
	if (starts_.size() > str_.size() + off_) return false;
	for (size_t i = 0; i < starts_.size(); i++)
		if (str_[i + off_] != starts_[i]) return false;
	return true;
}

inline bool isPartOfName(char ch_) {
	return
		(ch_ >= 'a' && ch_ <= 'z') ||
		(ch_ >= 'A' && ch_ <= 'Z') ||
		(ch_ >= '0' && ch_ <= '9') ||
		ch_ == '_' || ch_ == '.' || ch_ == '%';
		// '.' for file extensions and floats
		// '%' for compiler macros
}

inline bool isNameValid(const string &str_) {
	if (str_.empty()) return false;
	if (str_[0] >= '0' && str_[0] <= '9') return false;
	for (int i = 0; i < str_.size(); i++)
		if (!isPartOfName(str_[i]))  return false;
	return true;
}

inline bool isOneOf(const char char_, const char *list_, int checkEvery_ = 1) { // if strlen(list_) % checkEvery_ != 0 it will crash.
	for (const char *c = list_; *c != '\0'; c += checkEvery_)
		if (char_ == *c) return true;
	return false;
}

void removeComments(vector<string> &rScript_, const string &singleLineCommentBegin_ = "//", const string &multilineCommentBegin_ = "/*", const string &multilineCommentEnd_ = "*/");

void getNextToken(const char *&rTokBeg_, const char *&rTokEnd_, const char *end_, const char *delim_ = " \t", const char *range_ = "()[]{}<>", const char *string_ = "\"'");

vector<string> split(const char *begin_, const char *end_, const char *delim_ = " \t", const char *range_ = "()[]{}<>", const char *string_ = "\"'");

inline void prepareScript(vector<string> &rScript_) {
	if (rScript_.empty()) return;

	removeComments(rScript_);

	if (!rScript_.back().empty() && rScript_.back().back() == '\\') rScript_.back().pop_back();
	for (int i = rScript_.size() - 2; i >= 0; i--) {
		if (!rScript_[i].empty() && rScript_[i].back() == '\\') {
			rScript_[i].pop_back();
			rScript_[i] += rScript_[i + 1];
			rScript_[i + 1].clear();
		}
	}
};

template <typename T>
inline string numToStr(T val_, std::chars_format format_ = std::chars_format::fixed, int precision_ = 6, bool removeTrailingZeros_ = false) {
	char buf[64];

	std::to_chars_result result;
	if constexpr (std::is_floating_point_v<T>) {
		result = std::to_chars(buf, buf + sizeof(buf), val_, format_, precision_);
		if (removeTrailingZeros_ && format_ == std::chars_format::fixed) {
			while (*(result.ptr - 1) == '0') result.ptr--;
			if (*(result.ptr - 1) == '.') result.ptr--;
		}
	}
	else if constexpr (std::is_same_v<T, bool>) {
		return val_ ? "true" : "false";
	}
	else if constexpr (std::is_integral_v<T>) {
		result = std::to_chars(buf, buf + sizeof(buf), val_);
	}
	else {
		static_assert(false, "Invalid type passed to numToStr().");
	}

	return string(buf, result.ptr);
}

template <typename T>
bool strToNum(const string &str_, T &rVal_, size_t off_ = 0, bool allowPrefixes_ = true, std::chars_format fmt_ = std::chars_format::general) {

	T tmp;
	std::from_chars_result result;
	const char* begin = str_.data() + off_;
	const char* end = str_.data() + str_.size();

	if constexpr (std::is_integral_v<T>) {

		int base = 10;
		if (allowPrefixes_ && str_.size() > 2) {
			begin += 2;

			if (strStartsWith(str_, "0x")) base = 16;
			else if (strStartsWith(str_, "0b")) base = 2;
			else begin -= 2;
		}

		result = std::from_chars(begin, end, tmp, base);
	}
	else if constexpr (std::is_floating_point_v<T>) {
		result = std::from_chars(begin, end, tmp, fmt_);
	}
	else {
		static_assert(false, "Invalid type passed to strToNum().");
	}

	if (result.ptr != end || result.ec != std::errc{}) return 0;
	rVal_ = tmp;
	return 1;
}

enum MathOperEnum : unsigned int {
	ToThePowerOf, // **
	Times, DividedBy, Modulo, // * / %
	Plus, Minus, // + -
	BitShR, BitShL, // >> <<
	IsGreaterThan, IsLessThan, IsGreaterOrEqualTo, IsLessOrEqualTo, // > < >= <=
	IsEqualTo, IsNotEqualTo, // == !=
	BinAnd, // &
	BinXor, // ^
	BinOr, // |
	BinNot, // ~
	And, // &&
	Or, // ||
	Not, // !
	MakeInt, MakeFloat, MakeString, MakeIdenitifier, // int float string id

	InvalidOper
};

static constexpr int operPrecedence[]{
	2,
	1, 1, 1,
	0, 0,
	-1, -1,
	-2, -2, -2, -2,
	-3, -3,
	-4,
	-5,
	-6,
	3, // <-- it won't be used anyway, but idk, i'll just leave it there
	-7,
	-8,
	3,
	3, 3, 3
};

struct Expression;
using MacroRefMap = unordered_map<string, const Expression*>;
using MacroMap = unordered_map<string, Expression>;

struct Expression {
public:

	/*
	
	"ala ma kota! + (a ** 2 + 7.1) * (11 + (0x05 + 11)) - var"

	(
		"ala" - variable
		"ma" - variable
		"kota!" - invalid
		(
			"a" - variable
			"**" - oper
			"2" - integer
			"+" - oper
			"7.1" - float
		)
		"*" - oper
		(
			"11" - integer
			"+" - oper
			(
				"0x05" - integer
				"+" - oper
				"11" - integer
			)
		)
		"-" - oper
		"var" - variable
	)
	
	
	*/

	enum Type {
		Integer, // 1 39 92
		Float, // 1.453465
		Identifier, // variable_name
		String, // "ala ma kota"
		NestedExpression, // (+variable +(6 * 90^2) -...)
		Operator, // + - || >= !
		Invalid // 123abcz #aaa my:variable
	} type;

	vector<Expression> expressions;
	string stringVal;
	union {
		int intVal = 0;
		float floatVal;
		MathOperEnum operVal;
	};

	Expression(const char *begin_, const char *end_) {
		parse(begin_, end_);
	}
	Expression(const string &str_) {
		parse(str_.data(), str_.data() + str_.size());
	}

	static Expression makeIdentifier(const string &stringVal_) {
		Expression expr;
		expr.stringVal = stringVal_;
		expr.type = Identifier;
		return expr;
	}

	static Expression makeString(const string &stringVal_) {
		Expression expr;
		expr.stringVal = stringVal_;
		expr.type = String;
		return expr;
	}

	Expression(const vector<Expression> &exprs_) : type(NestedExpression), expressions(exprs_) {}
	Expression(const vector<string> &strs_) : type(NestedExpression) {
		for (auto &s : strs_)
			expressions.push_back(s);
	}

	Expression(const string &str_, const vector<Expression> &exprs_) : type(Identifier), expressions(exprs_), stringVal(str_) {}

	Expression() : type(Expression::Type::Invalid), intVal(0) {}

	Expression(float f_) : type(Expression::Type::Float), floatVal(f_) {}
	Expression(int i_) : type(Expression::Type::Integer), intVal(i_) {}

	Expression(const Expression &a_, const Expression &b_, MathOperEnum &oper_) { *this = operation(a_, oper_, b_); }
	Expression(const Expression &a_, const Expression &b_, const string &oper_) { *this = operation(a_, getOperEnum(oper_), b_); }

	Expression toString(bool removeTrailingZeros_ = true) const;
	Expression toFloat() const;
	Expression toInt() const;
	Expression toBool() const;
	Expression toInvalid() const;
	
	bool isValid() const {
		if (type == NestedExpression) {
			for (auto &e : expressions)
				if (!e.isValid()) return false;
			return true;
		}
		else {
			return type != Expression::Type::Invalid;
		}
	};
	
	//int replaceMacroNoParams(const Expression &macro_);
	Result replaceMacros(const MacroRefMap &macroMap_);
	void replaceMacroArguments(const MacroRefMap &argMap_);
	bool simplify(bool keepOperationOrder_ = true);

	static Expression operation(const Expression &a_, MathOperEnum oper_, const Expression &b_);

	static inline MathOperEnum getOperEnum(const string &str_) {
		auto it = operStrToEnum.find(str_);
		if (it == operStrToEnum.end()) return MathOperEnum::InvalidOper;
		return it->second;
	}

private:

	static const unordered_map<string, MathOperEnum> operStrToEnum;

	static Expression calcIntInt(int a_, MathOperEnum oper_, int b_);
	static Expression calcFloatFloat(float a_, MathOperEnum oper_, float b_);
	static Expression calcStrStr(const string &a_, MathOperEnum oper_, const string &b_);

	bool parse(const char *begin_, const char *end_);

};

void tokenizeScript(vector<string> &rScript_, vector<Expression> &rTokens_);

void genFinalMacroMap(MacroRefMap &rMacroMap_, const MacroMap &local_, const MacroMap &global_);

inline void flattenNestedExpr(Expression &rExpr_) {
	while (rExpr_.type == Expression::NestedExpression && rExpr_.expressions.size() == 1)
		rExpr_ = Expression(rExpr_.expressions[0]);
}

#endif
