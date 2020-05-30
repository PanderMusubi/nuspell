/* Copyright 2018-2020 Dimitrij Mijoski, Sander van Geloven
 *
 * This file is part of Nuspell.
 *
 * Nuspell is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Nuspell is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Nuspell.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <nuspell/dictionary.hxx>
#include <nuspell/finder.hxx>
#include <nuspell/utils.hxx>

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>

#include <boost/locale.hpp>

// manually define if not supplied by the build system
#ifndef PROJECT_VERSION
#define PROJECT_VERSION "unknown.version"
#endif
#define PACKAGE_STRING "nuspell " PROJECT_VERSION

#if defined(__MINGW32__) || defined(__unix__) || defined(__unix) ||            \
    (defined(__APPLE__) && defined(__MACH__))
#include <getopt.h>
#include <unistd.h>
#endif

#include <hunspell/hunspell.hxx>

using namespace std;
using namespace nuspell;

enum Mode {
	DEFAULT_MODE /**< verification test */,
	HELP_MODE /**< printing help information */,
	VERSION_MODE /**< printing version information */,
	ERROR_MODE /**< where the arguments used caused an error */
};

struct Args_t {
	Mode mode = DEFAULT_MODE;
	string program_name = "verify";
	string dictionary;
	string encoding;
	vector<string> other_dicts;
	vector<string> files;
	bool print_false = false;
	bool sugs = false;
	string corrections;

	Args_t() = default;
	Args_t(int argc, char* argv[]) { parse_args(argc, argv); }
	auto parse_args(int argc, char* argv[]) -> void;
};

auto Args_t::parse_args(int argc, char* argv[]) -> void
{
	if (argc != 0 && argv[0] && argv[0][0] != '\0')
		program_name = argv[0];
// See POSIX Utility argument syntax
// http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap12.html
#if defined(_POSIX_VERSION) || defined(__MINGW32__)
	int c;
	// The program can run in various modes depending on the
	// command line options. The mode is FSM state, this while loop is FSM.
	const char* shortopts = ":d:i:c:fshv";
	const struct option longopts[] = {
	    {"version", 0, nullptr, 'v'},
	    {"help", 0, nullptr, 'h'},
	    {nullptr, 0, nullptr, 0},
	};
	while ((c = getopt_long(argc, argv, shortopts, longopts, nullptr)) !=
	       -1) {
		switch (c) {
		case 'd':
			if (dictionary.empty())
				dictionary = optarg;
			else
				cerr << "WARNING: Detected not yet supported "
				        "other dictionary "
				     << optarg << '\n';
			other_dicts.emplace_back(optarg);

			break;
		case 'i':
			encoding = optarg;

			break;
		case 'c':
			if (corrections.empty())
				corrections = optarg;
			else
				cerr << "WARNING: Ignoring additional "
				        "suggestions TSV file "
				     << optarg << '\n';

			break;
		case 'f':
			print_false = true;

			break;
		case 's':
			sugs = true;
			break;
		case 'h':
			if (mode == DEFAULT_MODE)
				mode = HELP_MODE;
			else
				mode = ERROR_MODE;

			break;
		case 'v':
			if (mode == DEFAULT_MODE)
				mode = VERSION_MODE;
			else
				mode = ERROR_MODE;

			break;
		case ':':
			cerr << "Option -" << static_cast<char>(optopt)
			     << " requires an operand\n";
			mode = ERROR_MODE;

			break;
		case '?':
			cerr << "Unrecognized option: '-"
			     << static_cast<char>(optopt) << "'\n";
			mode = ERROR_MODE;

			break;
		}
	}
	files.insert(files.end(), argv + optind, argv + argc);
#endif
}

/**
 * @brief Prints help information to standard output.
 *
 * @param program_name pass argv[0] here.
 */
auto print_help(const string& program_name) -> void
{
	auto& p = program_name;
	auto& o = cout;
	o << "Usage:\n"
	     "\n";
	o << p << " [-d di_CT] [-i enc] [-c TSV] [-f] [-s] [FILE]...\n";
	o << p << " -h|--help|-v|--version\n";
	o << "\n"
	     "Verification testing spell check of each FILE.\n"
	     "Without FILE, check standard input.\n"
	     "\n"
	     "  -d di_CT      use di_CT dictionary. Only one dictionary is\n"
	     "                currently supported\n"
	     "  -i enc        input encoding, default is active locale\n"
	     "  -c TSV        TSV file with corrections to verify suggestions\n"
	     "  -f            print false negative and false positive words\n"
	     "  -s            also test suggestions (usable only in debugger)\n"
	     "  -h, --help    print this help and exit\n"
	     "  -v, --version print version number and exit\n"
	     "\n";
	o << "Example: " << p << " -d en_US /usr/share/dict/american-english\n";
	o << "\n"
	     "All words for which results differ with Hunspell are printed to"
	     "standard output. List available dictionaries: nuspell -D\n"
	     "\n"
	     "Then some statistics for correctness and "
	     "performance are printed to standard output, being:\n"
	     "  Total Words\n"
	     "  True Positives\n"
	     "  True Negatives\n"
	     "  False Positives\n"
	     "  False Negatives\n"
	     "  Accuracy\n"
	     "  Precision\n"
	     "  Tot. Duration Nuspell\n"
	     "  Tot. Duration Hunspell\n"
	     "  Min. Duration Nuspell\n"
	     "  Min. Duration Hunspell\n"
	     "  Ave. Duration Nuspell\n"
	     "  Ave. Duration Hunspell\n"
	     "  Max. Duration Nuspell\n"
	     "  Max. Duration Hunspell\n"
	     "  Speedup Rate\n"
	     "All durations are in nanoseconds. Even on the same machine,\n"
	     "timing can vary considerably in the second significant decimal!\n"
	     "Use only a production build executable with optimizations.\n"
	     "A speedup of 1.62 means Nuspell is 1.6x faster than Hunspell.\n"
	     "\n"
	     "Verification will be done on suggestions when a corrections\n";
	"file is provided. Each line contains an incorrect word, a tab\n";
	"character and the most desired correct suggestion.\n"
	"\n"
	"Please note, messages containing:\n"
	"  This UTF-8 encoding can't convert to UTF-16:"
	"are caused by Hunspell and can be ignored.\n";
}

/**
 * @brief Prints the version number to standard output.
 */
auto print_version() -> void
{
	cout << PACKAGE_STRING
	    "\n"
	    "Copyright (C) 2018-2020 Dimitrij Mijoski and Sander van Geloven\n"
	    "License LGPLv3+: GNU LGPL version 3 or later "
	    "<http://gnu.org/licenses/lgpl.html>.\n"
	    "This is free software: you are free to change and "
	    "redistribute it.\n"
	    "There is NO WARRANTY, to the extent permitted by law.\n"
	    "\n"
	    "Written by Dimitrij Mijoski, Sander van Geloven and others,\n"
	    "see https://github.com/nuspell/nuspell/blob/master/AUTHORS\n";
}

auto normal_loop(istream& in, ostream& out, Dictionary& dic, Hunspell& hun,
                 locale& hloc, string corrections, bool print_false = false,
                 bool test_sugs = false)
{
	using chrono::high_resolution_clock;
	using chrono::nanoseconds;
	auto word = string();
	auto wide_word = wstring();
	auto narrow_word = string();

	// verify spelling
	auto total = 0;
	auto true_pos = 0;
	auto true_neg = 0;
	auto false_pos = 0;
	auto false_neg = 0;
	auto duration_hun = nanoseconds();
	auto duration_nu = duration_hun;
	auto duration_nu_min = nanoseconds(nanoseconds::max());
	auto duration_hun_min = duration_nu_min;
	auto duration_nu_max = nanoseconds();
	auto duration_hun_max = duration_nu_max;
	auto in_loc = in.getloc();
	// need to take the entire line here, not `in >> word`
	while (getline(in, word)) {
		auto tick_a = high_resolution_clock::now();
		auto res_nu = dic.spell(word);
		auto tick_b = high_resolution_clock::now();
		to_wide(word, in_loc, wide_word);
		to_narrow(wide_word, narrow_word, hloc);
		auto res_hun = hun.spell(narrow_word);
		auto tick_c = high_resolution_clock::now();
		auto b_a = tick_b - tick_a;
		auto c_b = tick_c - tick_b;
		duration_nu += b_a;
		duration_hun += c_b;
		if (b_a < duration_nu_min)
			duration_nu_min = b_a;
		if (c_b < duration_hun_min)
			duration_hun_min = c_b;
		if (b_a > duration_nu_max)
			duration_nu_max = b_a;
		if (c_b > duration_hun_max)
			duration_hun_max = c_b;
		if (res_hun) {
			if (res_nu) {
				++true_pos;
			}
			else {
				++false_neg;
				if (print_false)
					out << "FalseNegativeWord   " << word
					    << '\n';
			}
		}
		else {
			if (res_nu) {
				++false_pos;
				if (print_false)
					out << "FalsePositiveWord   " << word
					    << '\n';
			}
			else {
				++true_neg;
			}
		}
		++total;

		// usable only in debugger
		if (test_sugs && !res_nu && !res_hun) {
			auto nus_sugs = vector<string>();
			auto hun_sugs = vector<string>();
			dic.suggest(word, nus_sugs);
			hun.suggest(narrow_word);
		}
	}

	// verify suggesions
	auto total_cor = 0;
	auto sug_nu = 0;
	auto sug_hun = 0;
	auto total_sug_equal_first = 0;
	auto total_sug_equal = 0;
	auto total_sug_more_hun = 0;
	auto total_sug_more_nu = 0;
	auto total_sug_none = 0;
	auto total_sug_none_nu = 0;
	auto total_sug_none_hun = 0;
	auto total_sug_max = 0;
	auto total_sug_max_nu = 0;
	auto total_sug_max_hun = 0;
	auto duration_sug_hun = nanoseconds();
	auto duration_sug_nu = nanoseconds();
	auto duration_sug_hun_min = nanoseconds(nanoseconds::max());
	auto duration_sug_nu_min = nanoseconds(nanoseconds::max());
	auto duration_sug_hun_max = nanoseconds();
	auto duration_sug_nu_max = nanoseconds();
	if (!corrections.empty()) {
		ifstream cor(corrections.c_str());
		if (cor.is_open()) {
			auto line = string();
			while (getline(cor, line)) {
				auto word_correction = vector<string>();
				split_on_any_of(line, '\t',
				                back_inserter(word_correction));
				word = word_correction[0];
				auto correction = word_correction[1];
				auto tick_a = high_resolution_clock::now();
				auto res_nu = dic.spell(word);
				auto tick_b = high_resolution_clock::now();
				auto res_hun = hun.spell(
				    to_narrow(to_wide(word, in_loc), hloc));
				auto tick_c = high_resolution_clock::now();
				auto b_a = tick_b - tick_a;
				auto c_b = tick_c - tick_b;
				duration_nu += b_a;
				duration_hun += c_b;
				if (b_a < duration_nu_min)
					duration_nu_min = b_a;
				if (c_b < duration_hun_min)
					duration_hun_min = c_b;
				if (b_a > duration_nu_max)
					duration_nu_max = b_a;
				if (c_b > duration_hun_max)
					duration_hun_max = c_b;
				if (res_hun) {
					if (res_nu) {
						++true_pos;
					}
					else {
						++false_neg;
						if (print_false)
							out << "FalseNegativeWo"
							       "rd   "
							    << word << '\n';
					}
				}
				else {
					if (res_nu) {
						++false_pos;
						if (print_false)
							out << "FalsePositiveWo"
							       "rd   "
							    << word << '\n';
					}
					else {
						++true_neg;
					}
				}
				++total;
				++total_cor;

				auto sugs_nu = vector<string>();
				tick_a = high_resolution_clock::now();
				dic.suggest(word, sugs_nu);
				tick_b = high_resolution_clock::now();
				auto sugs_hun = hun.suggest(
				    to_narrow(to_wide(word, in_loc), hloc));
				tick_c = high_resolution_clock::now();
				b_a = tick_b - tick_a;
				c_b = tick_c - tick_b;
				duration_sug_nu += b_a;
				duration_sug_hun += c_b;
				if (b_a < duration_sug_nu_min)
					duration_sug_nu_min = b_a;
				if (c_b < duration_sug_hun_min)
					duration_sug_hun_min = c_b;
				if (b_a > duration_sug_nu_max)
					duration_sug_nu_max = b_a;
				if (c_b > duration_sug_hun_max)
					duration_sug_hun_max = c_b;
				// expected suggestion found
				for (auto& sug : sugs_nu) {
					if (sug == correction) {
						++sug_nu;
						break;
					}
				}
				for (auto& sug : sugs_hun) {
					if (sug == correction) {
						++sug_hun;
						break;
					}
				}
				if (sugs_nu.size() > 0 && sugs_hun.size() > 0 &&
				    sugs_nu[0] == sugs_hun[0])
					++total_sug_equal_first;
				if (sugs_nu.size() == sugs_hun.size())
					++total_sug_equal;
				if (sugs_nu.size() < sugs_hun.size())
					++total_sug_more_hun;
				if (sugs_nu.size() > sugs_hun.size())
					++total_sug_more_nu;
				if (sugs_nu.size() == 0 && sugs_hun.size() == 0)
					++total_sug_none;
				if (sugs_nu.size() == 0)
					++total_sug_none_nu;
				if (sugs_hun.size() == 0)
					++total_sug_none_hun;
				if (sugs_nu.size() == 15 &&
				    sugs_hun.size() == 15)
					++total_sug_max;
				if (sugs_nu.size() == 15)
					++total_sug_max_nu;
				if (sugs_hun.size() == 15)
					++total_sug_max_hun;
			}
		}
	}

	// prevent devision by zero
	if (total == 0) {
		cerr << "No input was provided\n";
		return;
	}
	if (duration_nu.count() == 0) {
		cerr << "Invalid duration of 0 nanoseconds for Nuspell\n";
		return;
	}

	// check counts
	auto pos_nu = true_pos + false_pos;
	auto pos_hun = true_pos + false_neg;
	auto neg_nu = true_neg + false_neg;
	auto neg_hun = true_neg + false_pos;
	// check rates
	auto true_pos_rate = true_pos * 1.0 / total;
	auto true_neg_rate = true_neg * 1.0 / total;
	auto false_pos_rate = false_pos * 1.0 / total;
	auto false_neg_rate = false_neg * 1.0 / total;
	auto accuracy = (true_pos + true_neg) * 1.0 / total;
	auto precision = 0.0;
	if (true_pos + false_pos != 0)
		precision = true_pos * 1.0 / (true_pos + false_pos);
	auto speedup = duration_hun.count() * 1.0 / duration_nu.count();

	// check reporting
	out << "Total Words             " << total << '\n';
	out << "Positives Nuspell       " << pos_nu << '\n';
	out << "Positives Hunspell      " << pos_hun << '\n';
	out << "Negatives Nuspell       " << neg_nu << '\n';
	out << "Negatives Hunspell      " << neg_hun << '\n';
	out << "True Positives          " << true_pos << '\n';
	out << "True Positive Rate      " << true_pos_rate << '\n';
	out << "True Negatives          " << true_neg << '\n';
	out << "True Negative Rate      " << true_neg_rate << '\n';
	out << "False Positives         " << false_pos << '\n';
	out << "False Positive Rate     " << false_pos_rate << '\n';
	out << "False Negatives         " << false_neg << '\n';
	out << "False Negative Rate     " << false_neg_rate << '\n';
	out << "Accuracy                " << accuracy << '\n';
	out << "Precision               " << precision << '\n';
	out << "Tot. Duration Nuspell   " << duration_nu.count() << '\n';
	out << "Tot. Duration Hunspell  " << duration_hun.count() << '\n';
	out << "Min. Duration Nuspell   " << duration_nu_min.count() << '\n';
	out << "Min. Duration Hunspell  " << duration_hun_min.count() << '\n';
	out << "Ave. Duration Nuspell   " << duration_nu.count() / total
	    << '\n';
	out << "Ave. Duration Hunspell  " << duration_hun.count() / total
	    << '\n';
	out << "Max. Duration Nuspell   " << duration_nu_max.count() << '\n';
	out << "Max. Duration Hunspell  " << duration_hun_max.count() << '\n';
	out << "Speedup Rate            " << speedup << '\n';

	if (total_cor != 0 and duration_sug_nu.count() != 0) {
		// suggestion rates
		auto speedup_sug = 0.0;
		auto cor_rate_nu = sug_nu * 1.0 / total_cor;
		auto cor_rate_hun = sug_hun * 1.0 / total_cor;
		speedup_sug =
		    duration_sug_hun.count() * 1.0 / duration_sug_nu.count();

		// suggestion reporting
		out << "Total Corrections               " << total_cor << '\n';
		out << "Satisfied Suggestions Nuspell   " << sug_nu << '\n';
		out << "Satisfied Suggestions Hunspell  " << sug_hun << '\n';
		out << "Correction Rate Sat.S. Nuspell  " << cor_rate_nu
		    << '\n';
		out << "Correction Rate Sat.S. Hunspell " << cor_rate_hun
		    << '\n';
		out << "Correction Improvement Rate     "
		    << (cor_rate_hun != 0 ? cor_rate_nu / cor_rate_hun : 0)
		    << '\n';
		out << "Cor. No Suggestions Both        " << total_sug_none
		    << '\n';
		out << "Cor. No Suggestions Nuspell     " << total_sug_none_nu
		    << '\n';
		out << "Cor. No Suggestions Hunspell    " << total_sug_none_hun
		    << '\n';
		out << "Cor. With Suggestions Both      "
		    << total_cor - total_sug_none << '\n';
		out << "Cor. With Suggestions Nuspell   "
		    << total_cor - total_sug_none_nu << '\n';
		out << "Cor. With Suggestions Hunspell  "
		    << total_cor - total_sug_none_hun << '\n';
		out << "Cor. Equal # Sug. Both          " << total_sug_equal
		    << '\n';
		out << "Cor. More # Sug. Nuspell        " << total_sug_more_nu
		    << '\n';
		out << "Cor. More # Sug. Hunspell       " << total_sug_more_hun
		    << '\n';
		out << "Cor. Maximum # Sug. Both        " << total_sug_max
		    << '\n';
		out << "Cor. Maximum # Sug. Nuspell     " << total_sug_max_nu
		    << '\n';
		out << "Cor. Maxumum # Sug. Hunspell    " << total_sug_max_hun
		    << '\n';
		out << "Cor. Equal First Sug. Both      "
		    << total_sug_equal_first << '\n';
		out << "Cor. Equal First Sug. Both Rate "
		    << total_sug_equal_first * 1.0 / total_cor << '\n';
		out << "Tot. Duration Sug. Nuspell      "
		    << duration_sug_nu.count() << '\n';
		out << "Tot. Duration Sug. Hunspell     "
		    << duration_sug_hun.count() << '\n';
		out << "Min. Duration Sug. Nuspell      "
		    << duration_sug_nu_min.count() << '\n';
		out << "Min. Duration Sug. Hunspell     "
		    << duration_sug_hun_min.count() << '\n';
		out << "Ave. Duration Sug. Nuspell      "
		    << duration_sug_nu.count() / total_cor << '\n';
		out << "Ave. Duration Sug. Hunspell     "
		    << duration_sug_hun.count() / total_cor << '\n';
		out << "Max. Duration Sug. Nuspell      "
		    << duration_sug_nu_max.count() << '\n';
		out << "Max. Duration Sug. Hunspell     "
		    << duration_sug_hun_max.count() << '\n';
		out << "Suggestion Speedup Rate         " << speedup_sug
		    << '\n';
	}
}

namespace std {
ostream& operator<<(ostream& out, const locale& loc)
{
	if (has_facet<boost::locale::info>(loc)) {
		auto& f = use_facet<boost::locale::info>(loc);
		out << "name=" << f.name() << ", lang=" << f.language()
		    << ", country=" << f.country() << ", enc=" << f.encoding();
	}
	else {
		out << loc.name();
	}
	return out;
}
} // namespace std

int main(int argc, char* argv[])
{
#ifdef INSPECT
	cerr << "INFO: Inspection is enabled" << '\n';
#endif

	// May speed up I/O. After this, don't use C printf, scanf etc.
	ios_base::sync_with_stdio(false);

	auto args = Args_t(argc, argv);
	if (args.mode == ERROR_MODE) {
		cerr << "Invalid (combination of) arguments, try '"
		     << args.program_name << " --help' for more information\n";
		return 1;
	}
	boost::locale::generator gen;
	auto loc = std::locale();
	try {
		if (args.encoding.empty())
			loc = gen("");

		else
			loc = gen("en_US." + args.encoding);
	}
	catch (const boost::locale::conv::invalid_charset_error& e) {
		cerr << e.what() << '\n';
#ifdef _POSIX_VERSION
		cerr << "Nuspell error: see `locale -m` for supported "
		        "encodings.\n";
#endif
		return 1;
	}
	cin.imbue(loc);
	cout.imbue(loc);

	switch (args.mode) {
	case HELP_MODE:
		print_help(args.program_name);
		return 0;
	case VERSION_MODE:
		print_version();
		return 0;
	default:
		break;
	}
	clog << "INFO: I/O locale " << loc << '\n';

	auto f = Finder::search_all_dirs_for_dicts();

	if (args.dictionary.empty()) {
		// infer dictionary from locale
		auto& info = use_facet<boost::locale::info>(loc);
		args.dictionary = info.language();
		auto c = info.country();
		if (!c.empty()) {
			args.dictionary += '_';
			args.dictionary += c;
		}
	}
	if (args.dictionary.empty()) {
		cerr << "No dictionary provided and can not infer from OS "
		        "locale\n";
	}
	auto filename = f.get_dictionary_path(args.dictionary);
	if (filename.empty()) {
		cerr << "Dictionary " << args.dictionary << " not found\n";
		return 1;
	}
	clog << "INFO: Pointed dictionary " << filename << ".{dic,aff}\n";
	auto dic = Dictionary();
	try {
		dic = Dictionary::load_from_path(filename);
	}
	catch (const Dictionary_Loading_Error& e) {
		cerr << e.what() << '\n';
		return 1;
	}
	dic.imbue(loc);

	auto aff_name = filename + ".aff";
	auto dic_name = filename + ".dic";
	Hunspell hun(aff_name.c_str(), dic_name.c_str());
	auto hun_loc = gen(
	    "en_US." + Encoding(hun.get_dict_encoding()).value_or_default());

	if (args.files.empty()) {
		normal_loop(cin, cout, dic, hun, hun_loc, args.corrections,
		            args.print_false, args.sugs);
	}
	else {
		for (auto& file_name : args.files) {
			ifstream in(file_name);
			if (!in.is_open()) {
				cerr << "Can't open " << file_name << '\n';
				return 1;
			}
			in.imbue(cin.getloc());
			normal_loop(in, cout, dic, hun, hun_loc,
			            args.corrections, args.print_false);
		}
	}
	return 0;
}
