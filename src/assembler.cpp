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

static Result assembleInstruction(const InstructionTemplate &template_, const vector<Expression> &args_, InstructionBytes &rInst_) {
			
	if (args_.size() != template_.params.size())
		return { InvalidArgumentCount, numToStr(template_.params.size()) };
	
	InstructionBytes inst = 0;
	
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
	
	inst >>= ((int)sizeof(inst) - template_.byteNum) * 8;

	rInst_ = inst;

	return {};
}

Result assembleCode(vector<Expression> &rScript_, vector<Instruction> &rCode_, vector<Marker> &rMarkers_, bool addMarkers_, vector<ProcessedFile> &rFileStack_, int &rInstructionCount_) {
	Result result = {};
	
	ProcessedFile mainFile = rFileStack_.front();

	vector<InstructionTemplate> templs;

	unordered_map<string, unsigned int> labels;
	
	int processedBytes = 0;
	for (int l = 0; l < rScript_.size(); l++, rFileStack_.back().line++) { // Get all label addresses (and templates bc why not)

		vector<Expression> &line = rScript_[l].expressions;

		if (line.empty()) continue;
		
		if (line[0].type == Expression::Identifier) {

			const string &command = line[0].stringVal;
			if (command == "%file_push") {
				rFileStack_.push_back({ line[1].stringVal, -1 });
			}
			else if (command == "%file_pop") {
				if (rFileStack_.size() > 1) rFileStack_.pop_back();
			}
			else if (command == "%skip_to") { // %skip_to <byte>
				if (line.size() != 2) {
					return { InvalidArgumentCount, "1" };
				}

				if (line[1].type != Expression::Integer)
					return { UnexpectedToken, line[1].toString().stringVal };

				if (line[1].intVal < processedBytes)
					return { InvalidRange, ">=" + numToStr(processedBytes)};

				processedBytes = line[1].intVal;
			}
			else if (command == "%align") { // %align <num_of_bytes>
				if (line.size() != 2) {
					return { InvalidArgumentCount, "1" };
				}

				if (line[1].type != Expression::Integer)
					return { UnexpectedToken, line[1].toString().stringVal };

				int nextMultiple = ((line[1].intVal - processedBytes) % line[1].intVal);
				if (nextMultiple < 0) nextMultiple += line[1].intVal; // a%b can be < 0 for some reason, so we need to add b again
				nextMultiple += processedBytes;
				
				processedBytes = nextMultiple;
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

					for (int i = 1; i < line.size(); i++) line[i].simplify();
					
					InstructionTemplate newInstTempl;
					if (ErrorCode err = generateInstructionTemplate(line[0].stringVal, newInstTempl))
						return { err, line[0].stringVal };

					processedBytes += newInstTempl.byteNum;
					templs.push_back(newInstTempl);
				}
			}
		}
		else if (line[0].type == Expression::String && line.size() == 1) {
			processedBytes += line[0].stringVal.size();
		}
	}

	rFileStack_.clear();
	rFileStack_.push_back(mainFile);

	processedBytes = 0;
	for (int l = 0, instIdx = 0; l < rScript_.size(); l++, rFileStack_.back().line++) {

		vector<Expression> &line = rScript_[l].expressions;

		if (line.empty()) continue;

		if (line[0].type == Expression::Identifier) {

			if (line.size() == 2 && line[1].type == Expression::Invalid && line[1].stringVal == ":") continue;

			const string &command = line[0].stringVal;
			if (command == "%file_push") {
				rFileStack_.push_back({ line[1].stringVal, -1 });
			}
			else if (command == "%file_pop") {
				if (rFileStack_.size() > 1) rFileStack_.pop_back();
			}
			else if (command == "%marker") {
				if (line.size() != 2) return { InvalidArgumentCount, "1" };
				if (addMarkers_) rMarkers_.push_back({ line[1].stringVal, rCode_.size() });
			}
			else if (command == "%skip_to") { // %skip_to <byte>
				size_t addedBytes = line[1].intVal - processedBytes;
				rCode_.reserve(rCode_.size() + addedBytes);
				for (int i = 0; i < addedBytes; i++)
					rCode_.push_back(Instruction{ 0x00, 1 });

				processedBytes = line[1].intVal;
			}
			else if (command == "%align") { // %align <num_of_bytes>
				int nextMultiple = ((line[1].intVal - processedBytes) % line[1].intVal);
				if (nextMultiple < 0) nextMultiple += line[1].intVal;
				nextMultiple += processedBytes;

				size_t addedBytes = nextMultiple - processedBytes;
				rCode_.reserve(rCode_.size() + addedBytes);
				for (int i = 0; i < addedBytes; i++)
					rCode_.push_back(Instruction{ 0x00, 1 });

				processedBytes += addedBytes;
			}
			else {
				if (line[0].stringVal.front() == '_') {
					replaceLabels(rScript_[l], labels);

					for (int i = 1; i < line.size(); i++) {
						line[i].simplify(); // <-- the result isn't checked because registers can't be simplified, and simplify() returns 0 when it encounters one.
						if (line[i].type != Expression::Identifier && line[i].type != Expression::Integer)
							return { UnexpectedToken, line[i].toString().stringVal };
					}

					InstructionBytes newInst;
					result = assembleInstruction(templs[instIdx], vector<Expression>(line.begin() + 1, line.end()), newInst);
					if (result.code != NoError) break;
					rCode_.push_back({ newInst, templs[instIdx].byteNum });
					
					processedBytes += templs[instIdx].byteNum;
					instIdx++;
				}
			}
		}
		else if (line[0].type == Expression::String && line.size() == 1) {
			rCode_.reserve(rCode_.size() + line[0].stringVal.size());
			for (uint8_t c : line[0].stringVal)
				rCode_.push_back(Instruction{ c, 1 });
			processedBytes += line[0].stringVal.size();
		}
		else return { UnexpectedToken, line[0].toString().stringVal };
	}

	rInstructionCount_ = templs.size();

	return result;
}