#include "files.hpp"

bool readFile(vector<Expression> &rTokScript_, const string &fileName_) {

	vector<string> script;

	std::ifstream ifs(fileName_);
	if (!ifs.is_open()) return 0;
	
	std::string line;
	while (std::getline(ifs, line))
		script.push_back(line);

	prepareScript(script);
	tokenizeScript(script, rTokScript_);

	return 1;
}

bool saveCode(const vector<Instruction> &code_, const string &fileName_, size_t bytesPerLine_, bool splitInstructions_, const vector<Marker> &markers_, size_t *pByteNum_) {


	std::ofstream ofs(fileName_, std::ios::trunc | std::ios::binary);
	if (!ofs.is_open()) return 0;

	vector<char> outStr;

	size_t lastMarkerIdx = 0;

	int column = 0;
	size_t byteNum = 0;

	for (size_t i = 0; i < code_.size(); i++) {

		constexpr char hexDigits[] { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };

		if (lastMarkerIdx != markers_.size() && i == markers_[lastMarkerIdx].pos) {
			string str = "<" + markers_[lastMarkerIdx].str + "> ";
			outStr.insert(outStr.end(), str.begin(), str.end());
			lastMarkerIdx++;
		}

		InstructionBytes inst = code_[i].bytes << (((int)sizeof(code_[i].bytes) - code_[i].byteNum) * 8);
		for (int j = 0; j < code_[i].byteNum; j++) {
			outStr.push_back(hexDigits[inst >> (sizeof(inst) * 8 - 4)]);
			inst <<= 4;
			outStr.push_back(hexDigits[inst >> (sizeof(inst) * 8 - 4)]);
			inst <<= 4;

			byteNum++;
			column++;

			if (splitInstructions_ || j == code_[i].byteNum - 1) {
				if (column >= bytesPerLine_) {
					column = 0;
					outStr.push_back('\n');
				}
				else {
					outStr.push_back(' ');
				}
			}
		}
	}

	ofs.write(outStr.data(), outStr.size());

	if (pByteNum_ != nullptr) *pByteNum_ = byteNum;

	return 1;
}