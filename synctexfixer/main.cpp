#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <exception>
#include <numeric> // std::partial_sum
#include <boost/foreach.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/regex.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/regex.hpp>
#include <boost/algorithm/string.hpp> 

using std::string;
using std::cout;
using std::cerr;
using std::endl;
using boost::filesystem::path;

namespace fs = boost::filesystem;

const string texS = "tex", rnwS = "Rnw", synctexS = "synctex.gz", tmpS = "tmp", concordanceS = "-concordance";

inline void warn(string s){
	cerr << s << endl;
}
inline void log(string s){
#ifdef _DEBUG
	cout << s << endl;
#endif
}

string esc(string s){
	const boost::regex esc("[\\^\\.\\$\\|\\(\\)\\[\\]\\*\\+\\?\\/\\\\]");
	const string rep("\\\\\\1&");
	return boost::regex_replace(s, esc, rep, boost::match_default | boost::format_sed);
}

template<typename InputIterator, typename OutputIterator>
OutputIterator rleDecodeValues(InputIterator begin,
	InputIterator end,
	OutputIterator destBegin){
	while (begin != end){
		int count = *begin++;
		if (begin == end)
			throw std::invalid_argument("rleDecodeValues() requires even number of arguments!");
		int val = *begin++;
		for (int i = 0; i < count; i++)
			*destBegin++ = val;
	}
	return destBegin;
}

/* Concordance - stores concordance information between a .tex and a .Rnw file.
** Use mapping[i] to translate a .tex line number to a .Rnw line number;
*/
class Concordance{
public:
	path texF, rnwF;
	std::vector<int> mapping;
	Concordance(path texF, path rnwF, std::vector<int> mapping) : texF(texF), rnwF(rnwF), mapping(mapping){};
	Concordance(path texF, path rnwF) : texF(texF), rnwF(rnwF){};
};

std::map<string, Concordance> readConcordance(path concordanceF){
	path baseDirectory = concordanceF.parent_path();
	fs::ifstream fromFile(concordanceF.string());
	std::stringstream buffer;
	buffer << fromFile.rdbuf();
	string text = buffer.str();
	//istreambuf_iterator
	//                                             .tex-file  .rnw-file    offset  counts vals(diffs)
	boost::regex re("\\\\Sconcordance\\{concordance:([^:]*):([^\\%]*):\\%\\n(\\d+( \\d+ \\d+)*)\\}");

	const int subs[] = { 1, 2, 3 };
	boost::sregex_token_iterator i(text.begin(), text.end(), re, subs);
	boost::sregex_token_iterator j;
	std::map<string, Concordance> concordances;

	while (i != j){
		//new Concordance mapping from first to second regex group (.tex to .rnw)
		path texF((string)*i++);//path of the .txt file (prob. relative to .synctex.gz file)
		path rnwF((string)*i++);//path of the .Rnw file (prob. absolute)
		Concordance concordance(texF, rnwF);

		//split the third regex group (ofset ())
		std::vector<std::string> strValues;
		boost::algorithm::split(strValues,
			"1 " + (string)*i++, // 1 repetetition of first offest (first line) (to use partial sum later)
			boost::algorithm::is_space(),
			boost::algorithm::token_compress_on);

		std::vector<int> rleValues;
		std::transform(strValues.begin(),
			strValues.end(),
			std::back_inserter(rleValues),
			[](const std::string& str) { return std::stoi(str); });

		std::vector<int> diffs;
		rleDecodeValues(rleValues.begin(),
			rleValues.end(),
			std::back_inserter(diffs));
		concordance.mapping.resize(diffs.size());
		std::partial_sum(diffs.begin(), diffs.end(), concordance.mapping.begin());

		//add this concordance to map of concordances
		string key = fs::canonical(concordance.texF).string();
		boost::algorithm::to_lower(key);
		concordances.insert(std::pair<string, Concordance>(key, concordance));

		log(concordance.texF.string() + " maps to " + concordance.rnwF.string() + ": {");
		for (std::vector<int>::size_type i = 0; i != concordance.mapping.size(); i++) {
			log(std::to_string(i + 1) + " -> " + std::to_string(concordance.mapping[i]));
		}
		log("}");
	}
	if (concordances.size() == 0)
		warn("No mappings found in " + concordanceF.string());

	return concordances;
}

int patchSynctex(path &texF, path &syncF){
	log(".tex file: " + texF.string());
	current_path(fs::absolute(syncF).parent_path());

	path concF = texF.parent_path() / path(texF.stem().string() + concordanceS + texF.extension().string());
	//if (!fs::exists(concF))
		//throw ConcordanceFileNotFoundError(concF);
	path tmpF = path(syncF.string() + "." + tmpS);

	std::map<string, Concordance> concordances = readConcordance(concF);
	std::map<string, std::vector<int>> concByTags;

	bool searching = true;
	int foundInputs = 0, foundLinks = 0;
	{
		fs::ifstream fromFile(syncF, std::ios_base::in | std::ios_base::binary);
		fromFile.exceptions(fs::ifstream::failbit | fs::ifstream::badbit);
		boost::iostreams::filtering_istream in;
		in.push(boost::iostreams::gzip_decompressor());
		in.push(fromFile);

		fs::ofstream toFile(tmpF, std::ios_base::out | std::ios_base::binary);
		boost::iostreams::filtering_ostream out;
		out.push(boost::iostreams::gzip_compressor());
		out.push(toFile);

		boost::regex findInput("^Input:([^:]+):(.*" + esc(texF.filename().string()) + ")$");
		boost::regex findPostamble("^Postamble:.*");
		const string preS = "^([xkgvh\\$\\(\\[])(", postS = ")\\,(\\d+)([\\,:].*)";
		string tagsS = "";
		boost::regex findLinks;
		boost::sregex_token_iterator i, j;
		for (string str; std::getline(in, str);)
		{
			if (searching){
				if (foundInputs){
					i = boost::sregex_token_iterator(str.begin(), str.end(), findLinks, { 1, 2, 3, 4 });
				}
				boost::match_results<string::const_iterator> results;
				if (i != j){
					out << *i++; // ^([xkgvh\\$\\(\\[])
					string tag = *i++;
					int line = std::stoi(*i++);
					int patchedLine = (*concByTags.find(tag)).second[line - 1];
					out << tag << "," << patchedLine << *i++ << "\n";

					log(std::to_string(line) + "--->" + std::to_string(patchedLine));
					foundLinks++;
				}
				else if (boost::regex_search(str, results, findInput)){
					log("found maybe input: " + results[2].str());
					string filename = fs::canonical(results[2].str()).string();
					boost::algorithm::to_lower(filename);
					auto concordance = concordances.find(filename);
					if (concordance != concordances.end()){
						string tag = results[1].str();
						log("found input: " + str + " Tag:" + tag);
						concByTags.insert(std::make_pair(tag, (*concordance).second.mapping));
						tagsS = ((tagsS.empty()) ? "" : tagsS + "|") + esc(tag);
						findLinks = boost::regex(preS + tagsS + postS);
						log("new search expression: " + findLinks.str());

						out << boost::regex_replace(str, boost::regex(esc(texS) + "$"), rnwS) << "\n";

						foundInputs++;
					}
				}
				else{
					out << str << '\n';
					if (boost::regex_match(str, findPostamble))
						searching = false;
				}
			}
			else{
				out << str << '\n';
			}
		}
	}//close the streams

	if (!foundInputs){
		remove(tmpF);
		warn("No .tex file with concordance information specified in \"" + concF.string() + 
			"\" was found as Input in \""+ syncF.string() +"\".\nMaybe it was allready patched?");
	}
	else {
		//replace original with pached file
		remove(syncF);
		rename(tmpF, syncF);
		if (!foundInputs)
			warn("No links replaced in \"" + syncF.string() + "\"!");
	}
	return foundLinks;
}

int main(int argc, char* argv[])
{
	const string S_USAGE = "usage: patchSynctex <.tex file> [.synctex.gz file]\n";
	try{
		if (argc < 2)
			throw std::invalid_argument("Invalid Number of Arguments: No .tex file supplied!");
		std::vector<std::string> params(argv, argv + argc);

		path texF(params[1]);
		path syncF = (argc > 2) ? path(params[1]) : path(texF).replace_extension(synctexS);

		if (!fs::exists(syncF))
			throw "The .synctex.gz file \"" + syncF.string()+ "\" could not be found.\n";		

		int replacements = patchSynctex(texF, syncF);

		if (replacements)
			log("Sucessfuly replaced " + std::to_string(replacements) + " links.");
	}
	catch (const std::invalid_argument& e){
		cerr << e.what() << "\n\n";		
		cerr << S_USAGE << endl;
		return EXIT_FAILURE;
	}
	catch (const boost::filesystem::filesystem_error& e)
	{
		cerr << e.code().message() << std::endl;
		cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	catch (string lazy){
		cerr << lazy;
		cerr << "Did you set \"opts_knit$set(concordance = TRUE);\" before using knitr?" << endl;
		if (argc < 3)
			cerr << "Or try to specify its path:\n" << S_USAGE << endl;
	}
	catch (const std::exception& e){
		cerr << "here"<< e.what() << std::endl;
		throw;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
