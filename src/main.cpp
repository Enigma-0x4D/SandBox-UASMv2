#include "common.hpp"
#include "parser.hpp"
#include "assembler.hpp"
#include "files.hpp"
#include "compiler_commands.hpp"
#include "preprocessor.hpp"

struct CommandArguments {
	vector<string> args;
	unordered_map<string, string> longFlags;
	bool shortFlags[256]{};

	void parse(int argc, char* argv[]) {
		for (int i = 0; i < argc; i++) {
			if (argv[i][0] == '-') {
				if (argv[i][1] == '-') {
					char *valBegin = argv[i] + 4; // --x=v
					while (*valBegin != '\0' && *(valBegin - 1) != '=') valBegin++;
					longFlags[string(argv[i] + 2, valBegin - 1)] = string(valBegin);
				}
				else for (int c = 1; argv[i][c] != '\0'; c++) {
					if ((argv[i][c] >= 'a' && argv[i][c] <= 'z') || (argv[i][c] >= 'A' && argv[i][c] <= 'Z'))
						shortFlags[argv[i][c]] = true;
				}
			}
			else {
				args.push_back(argv[i]);
			}
		}
	}
};

int main(int argc, char* argv[]) {
	
	if (argc < 3) {
		fputs(
			"Usage:\n"
			"  .exe <src> <out> [--bytes=16] [-w] [-m] [-s]\n"
			"\n"
			"Arguments:\n"
			"  src      - Source file\n"
			"  out      - Output file\n"
			"\n"
			"Options:\n"
			"  --bytes  - Number of bytes per line (default: 16)\n"
			"  -w       - Do not split instructions into separate bytes\n"
			"  -m       - Add markers to the output code\n"
			"  -s       - Show include stack in error messages\n",
			stdout
		);
		return -1;
	}

	CommandArguments args;
	args.parse(argc, argv);

	bool splitInstructions = !args.shortFlags['w'];
	bool addMarkers = args.shortFlags['m'];
	bool showIncludeStack = args.shortFlags['s'];
	int bytesPerLine = 16;
	{
		auto it = args.longFlags.find("bytes");
		if (it != args.longFlags.end()) {
			int val;
			if (strToNum(it->second, val)) bytesPerLine = val;
		}
	}

	vector<ProcessedFile> fileStack;
	vector<Expression> tokScript;
	vector<Instruction> code;
	vector<Marker> markers;
	
	if (!readFile(tokScript, args.args[1])) {
		fputs("Error: Unable to open file.\n", stdout);
		return -2;
	}

	Result result = {};

	{
		ProcessedFile mainFile;
		mainFile.location = args.args[1];
		mainFile.line = 0;
		fileStack.push_back(mainFile);
	}

	result = preprocessor(tokScript, fileStack);
	if (result.code != NoError) goto end;

	/*
		Script now consists only of:
			ready instructions (_BrXiXnX ...)
			labels (main:)
			%marker, %file_push & %file_pop directives (for debugging), and %define, and other things that we don't care about
	*/

	result = assembleCode(tokScript, code, markers, addMarkers, fileStack);
	if (result.code != NoError) goto end;
	
	size_t byteNum;
	
	if (!saveCode(code, args.args[2], bytesPerLine, splitInstructions, markers, &byteNum)) {
		fputs("Error: Unable to open file.\n", stdout);
		return -2;
	}

end:

	if (result.code == NoError) {
		string outStr =
			"Compilation complete!\n"
			"Program takes " + numToStr(byteNum) + " bytes (" + numToStr(code.size()) + " instructions) of memory.\n";

		fputs(outStr.c_str(), stdout);
	}
	else {
		string errStr = "Compilation failed:\n";

		for (int i = 0; i < (showIncludeStack ? fileStack.size() : 1); i++) {
			for (int j = 0; j < i * 2; j++) errStr += ' ';
			errStr += "file: \"" + fileStack[i].location.name + "\", line: " + numToStr(fileStack[i].line + 1) + "\n";
		}
		
		errStr += "error: (" + numToStr((int)result.code) + ") " + result.getErrorMessage() + "\n";
		
		fputs(errStr.c_str(), stdout);
	}

	return result.code;
}