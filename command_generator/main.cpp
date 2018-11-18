/**
 * main.cpp
 * @author wherewindblow
 * @date   Jun 10, 2018
 */

#include <string>
#include <vector>
#include <set>
#include <map>
#include <fstream>

#include <lights/format.h>
#include <lights/exception.h>
#include <foundation/basics.h>


using CommandTable = std::vector<std::pair<int, std::string>>;
using MatchPatterns = std::vector<std::string>;

CommandTable generate_commands(const std::string& proto_filename, const MatchPatterns& match_patterns, int next_cmd = 1)
{
	std::ifstream proto_file(proto_filename);

	CommandTable cmd_table;
	std::set<std::string> name_set;

	bool is_message_name = false;
	std::string word;
	while (proto_file >> word)
	{
		std::string message_key = "message";
		if (word == message_key)
		{
			is_message_name = true; // Next word is message name.
			continue;
		}

		if (!is_message_name)
		{
			continue;
		}

		// Is message name.
		std::size_t end_pos = word.find("{");
		if (end_pos != std::string::npos)
		{
			word = word.substr(0, end_pos);
		}

		bool match = false;
		for (auto& pattern : match_patterns)
		{
			if (word.find(pattern) == 0) // Must start with pattern.
			{
				match = true;
				break;
			}
		}

		if (match)
		{
			auto itr = name_set.find(word);
			LIGHTS_ASSERT(itr == name_set.end() && "Repeat name");

			cmd_table.push_back(std::make_pair(next_cmd, word));
			name_set.insert(word);
			++next_cmd;
		}

		is_message_name = false;
	}

	return cmd_table;
}


int main(int argc, const char* argv[])
{
	std::string proto_filename = argv[1];
	std::string txt_cmd_filename = argv[2];
	std::string cpp_cmd_filename = argv[3];

	// Generate cmd.
	MatchPatterns match_patterns = { "Req", "Rsp" };
	int next_cmd = static_cast<int>(spaceless::BuildinCommand::MAX); // Avoid using buildin cmd.
	CommandTable cmd_table = generate_commands(proto_filename, match_patterns, next_cmd);

	// Save cmd in txt.
	std::ofstream txt_cmd_file(txt_cmd_filename);
	for (auto& pair: cmd_table)
	{
		txt_cmd_file << lights::format("{} {}\n", lights::pad(pair.first, ' ', 5), pair.second);
	}

	// Save cmd in cpp file.
	std::ofstream cpp_cmd_file(cpp_cmd_filename);
	cpp_cmd_file << "namespace spaceless {\n";
	cpp_cmd_file << "namespace protocol {\n";
	cpp_cmd_file << "namespace details {\n";
	cpp_cmd_file << "\n";
	cpp_cmd_file << "const std::map<int, std::string> default_command_name_map = {\n";
	for (auto& pair: cmd_table)
	{
		cpp_cmd_file << lights::format("    {{}, \"{}\"},\n", lights::pad(pair.first, ' ', 5), pair.second);
	}
	cpp_cmd_file << "};\n";
	cpp_cmd_file << "\n";
	cpp_cmd_file << "} // namespace details\n";
	cpp_cmd_file << "} // namespace protocol\n";
	cpp_cmd_file << "} // namespace spaceless\n";
}
