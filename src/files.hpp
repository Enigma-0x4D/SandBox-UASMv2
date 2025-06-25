#ifndef FILES_HPP
#define FILES_HPP

#include "common.hpp"
#include "parser.hpp"

struct FilePathAndName {
	string name;
	string path;

	FilePathAndName() {}

	FilePathAndName(const string &str_) {

		size_t lastSlash = str_.find_last_of("\\/");

		if (lastSlash != string::npos) {
			name = str_.substr(lastSlash + 1);
			path = str_.substr(0, lastSlash + 1);
		}
		else {
			name = str_;
			path = "";
		}
	}
};

struct ProcessedFile {
	FilePathAndName location;
	int line = 0;
	MacroMap macros;
};

struct Marker {
	string str;
	size_t pos;
};

bool readFile(vector<Expression> &rTokScript_, const string &fileName_);
bool saveCode(const vector<Instruction> &code_, const string &fileName_, size_t bytesPerLine_ = 16, bool splitInstructions_ = true, const vector<Marker> &markers_ = {}, size_t *pByteNum_ = nullptr);

#endif