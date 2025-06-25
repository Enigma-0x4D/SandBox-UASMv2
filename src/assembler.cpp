#include "assembler.hpp"

static void replaceLabels(Expression &rExpr_, const unordered_map<string, unsigned int> &labels_) {
	if (rExpr_.type == Expression::Identifier) {
		auto it = labels_.find(rExpr_.stringVal);
		if (it != labels_.end())
			rExpr_ = Expression((int)it->second);
	}
	else if (rExpr_.type == Expression::NestedExpression) {
		for (auto &e : rExpr_.expressions)
			replaceLabels(e, labels_);
	}
}

static bool parseParam(const char *&rpStr_, const char *pLast_, vector<ParamTemplate> &rParams_, size_t &rLastParamBegin_) {
	ParamTemplate param;

	switch (*rpStr_) {
	case 'i': param.type = ParamType::Number; break;
	case 'r': param.type = ParamType::Register; break;
	case 'n': param.type = ParamType::Invalid; break;
	default: return 0;
	}

	std::from_chars_result result = std::from_chars(rpStr_ + 1, pLast_, param.bits);

	if (result.ec != std::errc{}) return 0;
	rpStr_ = result.ptr;

	param.begin = rLastParamBegin_;
	rLastParamBegin_ += param.bits;

	if (param.type != ParamType::Invalid)
		rParams_.push_back(param);

	return 1;
}

static ErrorCode generateInstructionTemplate(const string &str_, InstructionTemplate &rTemplate_) {
	if (str_.size() < 3 || str_[0] != '_') return InvalidInstruction;

	const char *ptr = str_.data() + 1;
	const char *last = str_.data() + str_.size();

	std::from_chars_result result = std::from_chars(ptr, last, rTemplate_.byteNum);
	ptr = result.ptr;
	if (result.ec != std::errc{}) return InvalidInstruction;

	size_t lastParamBegin = 0;

	while (ptr != last) {
		if (!parseParam(ptr, last, rTemplate_.params, lastParamBegin)) return InvalidInstruction;
		if (!rTemplate_.params.empty() && (rTemplate_.params.back().begin + rTemplate_.params.back().bits > rTemplate_.byteNum * 8)) return InstructionTooShort;
	}

	return NoError;
}

struct PreprInst {
	InstructionTemplate templ;
	vector<Expression> params;
};

static Result assembleInstruction(const InstructionTemplate &template_, const vector<Expression> &args_, uint64_t &rInst_) {
			
	if (args_.size() != template_.params.size())
		return { InvalidArgumentCount, numToStr(template_.params.size()) };
	
	uint64_t inst = 0;
	
	for (int i = 0; i < args_.size(); i++) {
		
		InstrucitonParam thisParam(args_[i]);
		
		if (thisParam.type != template_.params[i].type) {
			return { UnexpectedToken, args_[i].toString().stringVal };
		}
				
		int64_t maxVal = (1LL << template_.params[i].bits) - 1; // (1 << 8) - 1 = 255
		int64_t minVal = template_.params[i].type == Register ? 0 : - (1LL << template_.params[i].bits) / 2; // -(1 << 8) / 2 = -128
		if (thisParam.value < minVal || thisParam.value > maxVal) {
			return { InvalidRange, '[' + numToStr(minVal) + ", " + numToStr(maxVal) + ']' };
		}
		
		int offset = (int)sizeof(inst) * 8 - template_.params[i].begin - template_.params[i].bits;
		
		inst |= (thisParam.value & maxVal) << offset;

		/*
						   [****] line_[intVal].value
			   [________________] inst

		  [****]                  line_[intVal].value >>= -(sizeof(inst) * 8)
			   [________________] inst

			   [****]             line_[intVal].value >>= params[intVal].bits
			   [________________] inst

					 [****]       line_[intVal].value >>= params[intVal].begin
			   [________________] inst
		*/
	}
	
	rInst_ = inst;

	return {};
}

Result assembleCode(vector<Expression> &rScript_, vector<Instruction> &rCode_, vector<Marker> &rMarkers_, bool addMarkers_, vector<ProcessedFile> &rFileStack_) {
	Result result = {};
	
	vector<InstructionTemplate> templs;

	unordered_map<string, unsigned int> labels;

	int processedBytes = 0;
	for (int l = 0; l < rScript_.size(); l++) { // Get all label addresses (and templates bc why not)

		vector<Expression> &line = rScript_[l].expressions;

		if (line.empty()) continue;
		
		for (int i = 1; i < line.size(); i++)
			line[i].simplify();

		if (line[0].type == Expression::Identifier) {

			const string &command = line[0].stringVal;
			if (command == "%file_push") {
				rFileStack_.push_back({ line[1].stringVal, -1 });
			}
			else if (command == "%file_pop") {
				if (!rFileStack_.empty()) rFileStack_.pop_back();
			}
			else {
				if (line.size() == 2 && line[1].type == Expression::Invalid && line[1].stringVal == ":") { // : is not an operator, so it will be Expression::Invalid. but it will work
					const string &labelName = line[0].stringVal;
					if (!isNameValid(labelName))
						return { UnexpectedToken, labelName };

					auto it = labels.find(labelName);
					if (it == labels.end())
						labels.emplace(labelName, processedBytes);
					else
						return { MultipleLabelDefinitions, labelName };

				}
				else if (line[0].stringVal.front() == '_') {

					InstructionTemplate newInstTempl;
					if (ErrorCode err = generateInstructionTemplate(line[0].stringVal, newInstTempl))
						return { err, line[0].stringVal };

					processedBytes += newInstTempl.byteNum;
					templs.push_back(newInstTempl);
				}
			}
		}
	}

	for (int l = 0, instIdx = 0; l < rScript_.size(); l++) {

		vector<Expression> &line = rScript_[l].expressions;

		if (line.empty()) continue;


		if (line[0].type == Expression::Identifier) {

			if (line.size() == 2 && line[1].type == Expression::Invalid && line[1].stringVal == ":") continue;

			const string &command = line[0].stringVal;
			if (command == "%file_push") {
				rFileStack_.push_back({ line[1].stringVal, -1 });
			}
			else if (command == "%file_pop") {
				if (!rFileStack_.empty()) rFileStack_.pop_back();
			}
			else if (command == "%marker") {
				if (line.size() != 2) return { InvalidArgumentCount, "1" };
				if (addMarkers_) rMarkers_.push_back({ line[1].stringVal, rCode_.size() });
			}
			else {
				if (line[0].stringVal.front() == '_') {
					replaceLabels(rScript_[l], labels);

					for (int i = 1; i < line.size(); i++) {
						line[i].simplify(); // <-- we don't check the result because registers can't be simplified
						if (line[i].type == Expression::NestedExpression) return { UnexpectedToken, line[i].toString().stringVal };
					}

					InstructionBytes newInst;
					result = assembleInstruction(templs[instIdx], vector<Expression>(line.begin() + 1, line.end()), newInst);
					if (result.code != NoError) break;
					rCode_.push_back({ newInst, templs[instIdx].byteNum });
					instIdx++;
				}
			}
			
		}
		else return { UnexpectedToken, line[0].toString().stringVal };
	}

	return result;
}