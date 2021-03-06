#include <ckv.hpp>
#include <fstream>
#include <regex>
#include <unordered_map>

bool tab_or_space(char ch)
{
	return (ch == '\t' || ch == ' ');
}

/**
 * Returns true if the character used in the key is
 * invalid.
 *
 * \param ch Character to check.
 * \return true if character is valid and false if not.
 */
bool is_char_invalid(char ch)
{
	return (!std::isalnum(ch) && ch != '_' && ch != '-');
}


/**
 * Opens file file_path in file_reader.
 */
void ckv::ConfigFile::open_file()
{
	if (!file_reader.is_open()) {
		file_reader.open(file_path);
		if (!file_reader) {
			err_line_no = 0;
			throw ckv::FileOpenFailed(file_path);
		}
	}
}

/**
 * Prints key value pair in a readable form.
 *
 * Basically it replaces all newlines in the value with a newline followed by a tab.
 * This is because ckv files require a tab to indicate value.
 *
 * \param out The output stream where key & value should be written.
 * \param key Key to output
 * \param value Value corresponding to the key
 */
void ckv::ConfigFile::print_key_val(std::ostream &out, std::string key, std::string value)
{
	std::string value_to_print = std::regex_replace(value, std::regex("\n"), "\n\t");
	out << key << " =\n";
	out << '\t' << value_to_print;
	out << std::endl;
}

/**
 * Parses untabbed lines.
 *
 * This parses the untabbed values and returns the key name.
 * Further in_block_parse() should be called to go
 * through the value of key.
 * This function checks the correctness of the file
 * only until the point it parses and throws exceptions
 * accordingly. For example, if the key it was looking for
 * has been found, it won't read the file further and exit.
 *
 * \throws EqualToWithoutAKey
 * \throws InvalidCharacter
 * \throws MissingEqualTo
 * \throws NoValueFoundForKey
 * \throws TrailingCharsAfterEqualTo
 * \throws ValueWithoutAKey
 *
 * \return
 *  It returns the next key found.
 *  If the key is empty, means there is
 *  no data further in file and in_block_parse()
 *  should NOT be called in such a case.
 */
std::string ckv::ConfigFile::out_block_parse()
{
	bool next_is_value_start = false;
	bool next_is_equal_to = false;
	unsigned char ch;
	std::string key;

	file_reader >> std::noskipws;

	while (file_reader >> ch) {

		if (ch == '\n') {

			if (next_is_value_start) {
				if (file_reader.peek() != '\t') {
					throw ckv::NoValueFoundForKey(key);
				}
				return key;
			} else if (!key.empty()) {
				throw ckv::MissingEqualTo();
			}

			err_line_no++;
		} else if (ch == '=') {
			next_is_equal_to = false;
			next_is_value_start = true;

			if (key.empty()) {
				throw ckv::EqualToWithoutAKey();
			}
		} else if (next_is_equal_to && !tab_or_space(ch)) {
			throw ckv::MissingEqualTo();
		} else if (next_is_value_start && !tab_or_space(ch)) {
			// this occurs after =
			throw ckv::TrailingCharsAfterEqualTo();
		} else if (!key.empty() && tab_or_space(ch) && !next_is_value_start) {
			// This cond is if a tab/space is used after the key
			next_is_equal_to = true;
		} else if (std::isspace(ch)) {
			continue;
		} else if (is_char_invalid(ch)) {
			// chars except 0-9, A-Z, a-z, _ and - are invalid.
			throw ckv::InvalidCharacter(ch);
		} else {
			// this is key's char, add it.
			key += ch;
		}
	}

	if (next_is_value_start) {
		throw ckv::NoValueFoundForKey(key);
	} else if (next_is_equal_to || !key.empty()) {
		throw ckv::MissingEqualTo();
	}

	return key;
}

/**
 * Parses tabbed lines or lines followed by '+'.
 *
 * This parses the tabbed values for a key.
 * Should strictly be called after out_block_parse() iff
 * key returned from it isn't empty.
 * It goes till EOF or the line not starting with tab or '+'.
 *
 * \return
 *  returns the value of key
 */
std::string ckv::ConfigFile::in_block_parse()
{
	unsigned char ch;
	std::string value;

	file_reader >> std::noskipws;

	file_reader >> ch;

	while (file_reader >> ch) {
		if (ch == '\n') {
			char next = file_reader.peek();
			if (next == '\t') {
				value += ch;
			} else if (next != '+') {
				return value;
			}
			err_line_no++;
			file_reader >> ch;
			continue;
		}
		value += ch;
	}

	return "";
}

/**
 * It returns the value for the param key.
 *
 * \throws EqualToWithoutAKey
 * \throws FileOpenFailed
 * \throws InvalidCharacter
 * \throws MissingEqualTo
 * \throws NoValueFoundForKey
 * \throws TrailingCharsAfterEqualTo
 * \throws ValueWithoutAKey
 *
 * \param key
 *  Key whose value should be returned.
 *
 * \return
 * 	value for key param1.
 */
std::string ckv::ConfigFile::get_value_for_key(std::string key)
{
	bool key_found = false;
	std::string cur_key;

	try {
		open_file();
	} catch(...) {
		throw;
	}

	// necessay cuz if some other func calls out_block_parse()
	// it may leave file_reader to EOF
	file_reader.seekg(0, std::ios::beg);
	err_line_no = 1;

	try {
		while (file_reader.peek() != EOF) {
			cur_key = out_block_parse();
			key_found = key == cur_key ? true : false;

			if (cur_key.empty()) {
				// no more keys left to read
				break;
			}
			if (!key_found) {
				in_block_parse();
			} else {
				return in_block_parse();
			}
		}
	} catch (...) {
		throw;
	}

	err_line_no = 0;
	throw ckv::KeyNotFound(key);
}

/**
 * It sets the value for the param key to param value.
 * It outputs the resulting file ouput to param out.
 *
 * \param key
 *  Key whose value needs to be changes
 *
 * \param new_value
 * 	new value for param1
 *
 * \param out
 * 	ostream to output the updated contents
 *
 * \throws EqualToWithoutAKey
 * \throws FileOpenFailed
 * \throws InvalidCharacter
 * \throws InvalidOutputStream
 * \throws MissingEqualTo
 * \throws NoValueFoundForKey
 * \throws TrailingCharsAfterEqualTo
 * \throws ValueWithoutAKey
 */
void ckv::ConfigFile::set_value_for_key(std::string key, std::string new_value, std::ostream &out)
{
	bool key_exists = false;

	try {
		open_file();
	} catch(...) {
		throw;
	}

	if (!out) {
		err_line_no = 0;
		throw ckv::InvalidOutputStream();
	}

	try {
		auto key_vals = import_to_map();

		for (auto &pair : key_vals) {
			if (pair.first == key) {
				print_key_val(out, key, new_value);
				key_exists = true;
			} else {
				print_key_val(out, pair.first, pair.second);
			}

			out << std::endl;
		}

		if (!key_exists) {
			print_key_val(out, key, new_value);
		}
	} catch (...) {
		throw;
	}

}

/**
 * It sets the value for the param key to param value
 * in the same file.
 *
 * \param key
 * 	Key whose value needs to be changes
 *
 * \param new_value
 * 	New value for param1
 *
 * \throws EqualToWithoutAKey
 * \throws FileOpenFailed
 * \throws InvalidCharacter
 * \throws MissingEqualTo
 * \throws NoValueFoundForKey
 * \throws TrailingCharsAfterEqualTo
 * \throws ValueWithoutAKey
 */
void ckv::ConfigFile::set_value_for_key(std::string key, std::string new_value)
{
	bool key_exists = false;
	bool file_open_failed = false;
	std::unordered_map<std::string, std::string> key_vals;

	try {
		open_file();
	} catch(...) {
		file_open_failed = true;
	}

	try {
		if (!file_open_failed) {
			key_vals = import_to_map();
			file_reader.close();
		}

		std::ofstream out(file_path);

		if (!out) {
			err_line_no = 0;
			throw ckv::FileOpenFailed(file_path);
		}

		out.seekp(0, std::ios::beg);

		for (auto &pair : key_vals) {
			if (pair.first == key) {
				print_key_val(out, key, new_value);
				key_exists = true;
			} else {
				print_key_val(out, pair.first, pair.second);
			}

			out << std::endl;
		}

		if (!key_exists) {
			print_key_val(out, key, new_value);
		}
	} catch (...) {
		throw;
	}

}

/**
 * It removes the param key.
 * It outputs the resulting file ouput to param out.
 *
 * \param key
 * 	Key to remove
 *
 * \param out
 * 	ostream to output the updated contents
 *
 * \throws EqualToWithoutAKey
 * \throws FileOpenFailed
 * \throws InvalidCharacter
 * \throws InvalidOutputStream
 * \throws MissingEqualTo
 * \throws NoValueFoundForKey
 * \throws TrailingCharsAfterEqualTo
 * \throws ValueWithoutAKey
 */
void ckv::ConfigFile::remove_key(std::string key, std::ostream &out)
{
	try {
		open_file();
	} catch(...) {
		throw;
	}

	if (!out) {
		err_line_no = 0;
		throw ckv::InvalidOutputStream();
	}

	try {
		auto key_vals = import_to_map();

		for (auto &pair : key_vals) {
			if (pair.first != key) {
				print_key_val(out, pair.first, pair.second);
				out << std::endl;
			}
		}
	} catch (...) {
		throw;
	}
}

/**
 * It removes the param key in the same file itself.
 *
 * \param key
 * 	Key to remove
 *
 * \throws EqualToWithoutAKey
 * \throws FileOpenFailed
 * \throws InvalidCharacter
 * \throws MissingEqualTo
 * \throws NoValueFoundForKey
 * \throws TrailingCharsAfterEqualTo
 * \throws ValueWithoutAKey
 */
void ckv::ConfigFile::remove_key(std::string key)
{
	try {
		open_file();
	} catch(...) {
		throw;
	}

	try {
		auto key_vals = import_to_map();

		file_reader.close();

		std::ofstream out(file_path);

		if (!out) {
			err_line_no = 0;
			throw ckv::FileOpenFailed(file_path);
		}

		out.seekp(0, std::ios::beg);

		for (auto &pair : key_vals) {
			if (pair.first != key) {
				print_key_val(out, pair.first, pair.second);
				out << std::endl;
			}
		}
	} catch (...) {
		throw;
	}
}

/**
 * It goes though all the keys in the ckv file
 * and store the key value pairs in an unordered_map
 * and returns.
 *
 * \throws EqualToWithoutAKey
 * \throws FileOpenFailed
 * \throws InvalidCharacter
 * \throws MissingEqualTo
 * \throws NoValueFoundForKey
 * \throws TrailingCharsAfterEqualTo
 * \throws ValueWithoutAKey
 */
std::unordered_map<std::string, std::string> ckv::ConfigFile::import_to_map()
{
	std::unordered_map<std::string, std::string> imported_map;
	std::string key, value;

	try {
		open_file();
	} catch(...) {
		throw;
	}

	file_reader.seekg(0, std::ios::beg);
	err_line_no = 1;

	try {
		while (file_reader.peek() != EOF) {
			key = out_block_parse();

			if (key.empty()) {
				// no more keys left to read
				break;
			}

			value = in_block_parse();

			imported_map.insert({key, value});
		}
	} catch (...) {
		throw;
	}

	return imported_map;
}
