// ==========================================================================
//                                  RNAMotif
// ==========================================================================
// Copyright (c) 2006-2013, Knut Reinert, FU Berlin
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Knut Reinert or the FU Berlin nor the names of
//       its contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL KNUT REINERT OR THE FU BERLIN BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.
//
// ==========================================================================
// Author: Benjamin Strauch
// ==========================================================================

// SeqAn headers
#include <seqan/basic.h>
#include <seqan/sequence.h>
#include <seqan/arg_parse.h>
#include <seqan/seq_io.h>

// openMP
#include <omp.h>

// C++ headers
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

// App headers
#include "folding_utils/RNAlib_utils.h"
#include "folding_utils/IPknot_utils.h"
#include "motif.h"

// reading the Stockholm format
#include "stockholm_file.h"
#include "stockholm_io.h"

// -----------

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/time.h>
#include <ctime>
#endif

/* Returns the amount of milliseconds elapsed since the UNIX epoch. Works on both
 * windows and linux. */

uint64_t GetTimeMs64()
{
#ifdef _WIN32
 /* Windows */
 FILETIME ft;
 LARGE_INTEGER li;

 /* Get the amount of 100 nano seconds intervals elapsed since January 1, 1601 (UTC) and copy it
  * to a LARGE_INTEGER structure. */
 GetSystemTimeAsFileTime(&ft);
 li.LowPart = ft.dwLowDateTime;
 li.HighPart = ft.dwHighDateTime;

 uint64_t ret = li.QuadPart;
 ret -= 116444736000000000LL; /* Convert from file time to UNIX epoch time. */
 ret /= 10000; /* From 100 nano seconds (10^-7) to 1 millisecond (10^-3) intervals */

 return ret;
#else
 /* Linux */
 struct timeval tv;

 gettimeofday(&tv, NULL);

 uint64_t ret = tv.tv_usec;
 /* Convert from micro seconds (10^-6) to milliseconds (10^-3) */
 ret /= 1000;

 /* Adds the seconds (10^0) after converting them to milliseconds (10^-3) */
 ret += (tv.tv_sec * 1000);

 return ret;
#endif
}

int toVal(char c){
	switch (c) {
		case 'A':
			return 0;
			break;
		case 'C':
			return 1;
			break;
		case 'G':
			return 2;
			break;
		case 'U':
			return 3;
			break;
		default:
			break;
	}

	return 0;
}

uint64_t hashString(std::string &rnaStr){
	uint64_t hash = 0;

	for (unsigned i=0; i < rnaStr.size(); ++i){
		if (rnaStr[i] == '-')
			continue;

		hash = (hash << 2) + toVal(rnaStr[i]);
	}

	return hash;
}


// ==========================================================================
// Classes
// ==========================================================================

// ==========================================================================
// Functions
// ==========================================================================

// --------------------------------------------------------------------------
// Function parseCommandLine()
// --------------------------------------------------------------------------

seqan::ArgumentParser::ParseResult
parseCommandLine(AppOptions & options, int argc, char const ** argv)
{
    // Setup ArgumentParser.
    seqan::ArgumentParser parser("RNAMotif");
    // Set short description, version, and date.
    setShortDescription(parser, "RNA motif generator");
    setVersion(parser, "0.1");
    setDate(parser, __DATE__);

    // Define usage line and long description.
    addUsageLine(parser, "[\\fIOPTIONS\\fP] <\\fISEED ALIGNMENT\\fP> <\\fIGENOME FILE\\fP>");
    addDescription(parser, "Generate a searchable RNA motif from a seed alignment.");

    // We require one argument.
    addArgument(parser, seqan::ArgParseArgument(seqan::ArgParseArgument::STRING, "INPUT FILE"));
    addArgument(parser, seqan::ArgParseArgument(seqan::ArgParseArgument::STRING, "GENOME FILE"));

    addOption(parser, seqan::ArgParseOption("r", "reference", "Reference file with ground-truth table.", seqan::ArgParseOption::STRING));

    addOption(parser, seqan::ArgParseOption("ml", "max-length", "Maximum sequence length to fold", seqan::ArgParseOption::INTEGER));
    setDefaultValue(parser, "max-length", 1000);

    addOption(parser, seqan::ArgParseOption("t", "threads", "Number of threads to use for motif extraction.", seqan::ArgParseOption::INTEGER));
    setDefaultValue(parser, "threads", 1);

    addOption(parser, seqan::ArgParseOption("f", "freq", "Frequency threshold (% as integer values).", seqan::ArgParseOption::INTEGER));
    setDefaultValue(parser, "freq", 0);

    addOption(parser, seqan::ArgParseOption("m", "match-length", "Seed length.", seqan::ArgParseOption::INTEGER));

    addOption(parser, seqan::ArgParseOption("ps", "pseudoknot", "Predict structure with IPknot to include pseuoknots."));
    addOption(parser, seqan::ArgParseOption("co", "constrain", "Constrain individual structures with the seed consensus structure."));
    addOption(parser, seqan::ArgParseOption("q", "quiet", "Set verbosity to a minimum."));
    addOption(parser, seqan::ArgParseOption("v", "verbose", "Enable verbose output."));
    addOption(parser, seqan::ArgParseOption("vv", "very-verbose", "Enable very verbose output."));

    // Add Examples Section.
    addTextSection(parser, "Examples");
    addListItem(parser, "\\fBRNAMotif\\fP \\fB-v\\fP \\fItext\\fP",
                "Call with \\fITEXT\\fP set to \"text\" with verbose output.");

    // Parse command line.
    seqan::ArgumentParser::ParseResult res = seqan::parse(parser, argc, argv);

    // Only extract  options if the program will continue after parseCommandLine()
    if (res != seqan::ArgumentParser::PARSE_OK)
        return res;

    options.constrain  = isSet(parser, "constrain");
    options.pseudoknot = isSet(parser, "pseudoknot");

    // Extract option values.
    if (isSet(parser, "quiet"))
        options.verbosity = 0;
    if (isSet(parser, "verbose"))
        options.verbosity = 2;
    if (isSet(parser, "very-verbose"))
        options.verbosity = 3;
    seqan::getArgumentValue(options.rna_file, parser, 0);
    seqan::getArgumentValue(options.genome_file, parser, 1);
    getOptionValue(options.fold_length, parser, "max-length");
    getOptionValue(options.threads, parser, "threads");
    getOptionValue(options.match_len, parser, "match-length");
    getOptionValue(options.reference_file, parser, "reference");

    int freq;
    getOptionValue(freq, parser, "freq");
    options.freq_threshold = ((double)freq)/100.0;

    return seqan::ArgumentParser::PARSE_OK;
}

void outputStats(std::vector<Motif*> &motifs){
	std::ofstream fout;
	fout.open("output_stats.txt", std::ios_base::app | std::ios_base::out);
	//fout.open("output_stats.txt");

	StructureElement tmpStruc;

	int c = 0;
	for (auto motif : motifs){


		c += 1;

		if (motif == 0)
			continue;

		int aln_len = seqan::length(seqan::row(motif->seedAlignment,0));

		//fout << aln_len;

		double h_none;

		for (TStructure &structure : motif->profile){
			//fout << structure.prob << "\t" << structure.suboptimal << "\n";

			//fout << "\t";
			//fout << (structure.pos.second - structure.pos.first) << ":" << structure.elements.size() << ":" << structure.prob;
			//fout << (2-loopEntropy(structure.elements.back().loopComponents)) << ":" << seqan::length(structure.elements.back().loopComponents[0]);
			double h_stem  = 0;
			double h_hair  = 0;
			double h_bulge = 0;
			double h_loop  = 0;

			double gap_min_hair  = 0, gap_med_hair  = 0, gap_max_hair  = 0;
			double gap_min_loop  = 0, gap_med_loop  = 0, gap_max_loop  = 0;
			double gap_min_bulge = 0, gap_med_bulge = 0, gap_max_bulge = 0;
			double gap_min_stem  = 0, gap_med_stem  = 0, gap_max_stem  = 0;

			int n_stem  = 0;
			int n_hair  = 0;
			int n_bulge = 0;
			int n_loop  = 0;

			h_none = (seqan::length(motif->externalBases) > 0) ? loopEntropy(motif->externalBases) : -1;

			bool a1=false,a2=false,a3=false,a4=false;

			//std::cout << "(" << structure.pos.first << "," << structure.pos.second << ")";

			for (StructureElement &element: structure.elements){
				double elemLen = seqan::length(element.loopComponents);

				//std::cout << element.statistics[0].min_length << "- min\n";
				//std::cout << element.statistics[0].mean_length << "- mean\n";
				//std::cout << element.statistics[0].max_length << "- max\n";
				//std::cout << elemLen << "- N\n";

				double min  = element.statistics.min_length /elemLen;
				double mean = element.statistics.mean_length/elemLen;
				double max  = element.statistics.max_length /elemLen;

				/*
				if (element.statistics.size() > 1){
					double elemLen2 = seqan::length(element.loopComponents[1]);
					min  += element.statistics[1].min_length /elemLen2;
					mean += element.statistics[1].mean_length/elemLen2;
					max  += element.statistics[1].max_length /elemLen2;

					min  = min/2;
					mean = mean/2;
					max  = max/2;
				}
				*/

				switch (element.type) {
					case StructureType::HAIRPIN:
						++n_hair;
						h_hair += loopEntropy(element.loopComponents);
						a2 = true;

						gap_min_hair += min;
						gap_med_hair += mean;
						gap_max_hair += max;
						break;
					case StructureType::LOOP:
						++n_loop;
						h_loop += loopEntropy(element.loopComponents);
						a3 = true;

						gap_min_loop += min;
						gap_med_loop += mean;
						gap_max_loop += max;
						break;
					case StructureType::LBULGE:
					case StructureType::RBULGE:
						++n_bulge;
						h_bulge += loopEntropy(element.loopComponents);
						a4 = true;

						gap_min_bulge += min;
						gap_med_bulge += mean;
						gap_max_bulge += max;
						break;
					case StructureType::STEM:
						++n_stem;
						h_stem += stemEntropy(element.stemProfile);
						a1 = true;

						gap_min_stem += min;
						gap_med_stem += mean;
						gap_max_stem += max;
						break;
					default:
						break;
				}
			}

			//std::cout << (gap_min_hair ) << ":" << (gap_med_hair  ) << ":" << (gap_max_hair  ) << "\t"
			//	 << (gap_min_loop ) << ":" << (gap_med_loop  ) << ":" << (gap_max_loop  ) << "\t"
			//	 << (gap_min_bulge) << ":" << (gap_med_bulge) << ":" << (gap_max_bulge) << "\t"
			//	 << (gap_min_stem  ) << ":" << (gap_med_stem  ) << ":" << (gap_max_stem  ) << "\n";

			//std::cout << n_hair << "\t" << n_loop << "\t" << n_bulge << "\t" << n_stem << "\n";

			//fout << (a2 ? (2-h_hair/n_hair) : -1) << ":" << (a1 ? (3-h_stem/n_stem) : -1) << ":" << (a3 ? (2-h_loop/n_loop) : -1) << ":" << (a4 ? (2-h_bulge/n_bulge) : -1);

			//fout << gap_min_hair /n_hair  << ":" << gap_med_hair /n_hair << ":" << gap_max_hair /n_hair << "|"
			//	 << gap_min_loop /n_loop  << ":" << gap_med_loop /n_loop << ":" << gap_max_loop /n_loop << "|"
			//	 << gap_min_bulge/n_bulge << ":" << gap_med_bulge/n_bulge<< ":" << gap_max_bulge/n_bulge<< "|"
			//	 << gap_min_stem /n_stem << ":" << gap_med_stem /n_stem << ":" << gap_max_stem /n_stem;

			//fout << (a2 ? (gap_min_hair /n_hair ) : -1) << ":" << (a2 ? (gap_med_hair /n_hair ) : -1) << ":" << (a2 ? (gap_max_hair /n_hair ) : -1) << "|"
			//	 << (a3 ? (gap_min_loop /n_loop ) : -1) << ":" << (a3 ? (gap_med_loop /n_loop ) : -1) << ":" << (a3 ? (gap_max_loop /n_loop ) : -1) << "|"
			//	 << (a4 ? (gap_min_bulge/n_bulge) : -1) << ":" << (a4 ? (gap_med_bulge/n_bulge) : -1) << ":" << (a4 ? (gap_max_bulge/n_bulge) : -1) << "|"
			//	 << (a1 ? (gap_min_stem /n_stem ) : -1) << ":" << (a1 ? (gap_med_stem /n_stem ) : -1) << ":" << (a1 ? (gap_max_stem /n_stem ) : -1) << "\t";
		}

		//fout << "\t" << (2-h_none) << "\n";
		//fout << motif->mcc << "\n";
	}
}

template<typename Out>
void split(const std::string &s, char delim, Out result) {
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        *(result++) = item;
    }
}


std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, std::back_inserter(elems));
    return elems;
}


std::unordered_map<std::string, std::vector<RfamBenchRecord> > read_reference(seqan::CharString file, bool exclude_rev = true){
	std::unordered_map<std::string, std::vector<RfamBenchRecord> > result;

	std::ifstream infile(seqan::toCString(file));
	std::string ID, ref, seq, start, end;
	while (infile >> ID >> ref >> seq >> start >> end)
	{
		RfamBenchRecord record;

		std::vector<std::string> idsplit = split(ID, '/');
		record.ID = idsplit[0];
		record.seq_nr = std::stoi(idsplit[1]);

		std::vector<std::string> refsplit = split(ref, 'k');
		record.ref_nr = std::stoi(refsplit[1]);

		record.seq_name = seq;
		record.start = std::stoi(start)-2;
		record.end = std::stoi(end)-2;

		record.reverse = record.start > record.end;

		if (exclude_rev && record.reverse){
			continue;
		}

		result[record.ID].push_back(record);
		//std::cout << ID << "\t" << ref << "\t" << seq << "\t" << start << "\t" << end << "\n";
		//std::cout << record.ID << "\t" << record.seq_nr << "\t" << record.ref_nr << "\t" << record.seq_name << "\t" << record.start << "\t" << record.end << "\n";
	}

	return result;
}

// --------------------------------------------------------------------------
// Function main()
// --------------------------------------------------------------------------

// Program entry point.

int main(int argc, char const ** argv)
{
    // Parse the command line.
    seqan::ArgumentParser parser;
    AppOptions options;
    seqan::ArgumentParser::ParseResult res = parseCommandLine(options, argc, argv);

    // If there was an error parsing or built-in argument parser functionality
    // was triggered then we exit the program.  The return code is 1 if there
    // were errors and 0 if there were none.
    if (res != seqan::ArgumentParser::PARSE_OK)
        return res == seqan::ArgumentParser::PARSE_ERROR;

    std::cout << "RNA motif generator\n"
              << "===============\n\n";

    // Print the command line arguments back to the user.
    if (options.verbosity > 0)
    {
        std::cout << "__OPTIONS____________________________________________________________________\n"
                  << '\n'
                  << "VERBOSITY\t" << options.verbosity << '\n'
                  << "CONSTRAINT\t" << options.constrain << '\n'
				  << "PSEUDOKNOTS\t" << options.pseudoknot << '\n'
				  << "MAX LENGTH\t" << options.fold_length << '\n'
				  << "FREQUENCY\t" << options.freq_threshold << '\n'
				  << "RNA      \t" << options.rna_file << '\n'
				  << "REFERENCE\t" << options.reference_file << '\n'
                  << "TARGET   \t" << options.genome_file << "\n\n";

        std::cout << "Data types\n"
        		  << seqan::ValueSize<TBaseAlphabet>::VALUE << "\n"
				  << seqan::ValueSize<TAlphabet>::VALUE << "\n"
        		  << seqan::ValueSize<TBiAlphabet>::VALUE << "\n"
				  << seqan::ValueSize<TAlphabetProfile>::VALUE << "\n"
				  << seqan::ValueSize<TBiAlphabetProfile>::VALUE << "\n"
				  << "Cache size " << HashTabLength << "\n";
    }


    //std::vector<bool> testbool(5,true);
    //std::cout << "TEST" << std::accumulate(testbool.begin(), testbool.end(), 0) << "\n";

    omp_set_num_threads(options.threads);

    std::vector<seqan::StockholmRecord<TBaseAlphabet> > records;

    uint64_t start = GetTimeMs64();

    seqan::StockholmFileIn stockFileIn;
    seqan::open(stockFileIn, seqan::toCString(options.rna_file));

    while (!seqan::atEnd(stockFileIn)){
    	seqan::StockholmRecord<TBaseAlphabet> record;
		seqan::readRecord(record, stockFileIn);
		records.push_back(record);
    }

    std::cout << records.size() << " records read\n";
    std::cout << "Time: " << GetTimeMs64() - start << "ms \n";

	std::vector<Motif*> motifs(records.size());

	#pragma omp parallel for schedule(dynamic)
	for (size_t k=0; k < records.size(); ++k){
		seqan::StockholmRecord<TBaseAlphabet> const &record = records[k];


		std::cout << record.header.at("AC") << " : " << record.header.at("ID") << "\n";

		int seq_len = record.seqences.begin()->second.length();
		if (seq_len > options.fold_length){
			std::cout << "Alignment has length " << seq_len << " > " << options.fold_length << " .. skipping.\n";
			continue;
		}

		// convert Rfam WUSS structure to normal brackets to get a constraint
		char *constraint_bracket = NULL;
		if (options.constrain){
			constraint_bracket = new char[record.seqence_information.at("SS_cons").length() + 1];
			WUSStoPseudoBracket(record.seqence_information.at("SS_cons"), constraint_bracket);
		}

		/*
		char * rfam_bracket = WUSStoPseudoBracket(record.seqence_information.at("SS_cons"), constraint_bracket);
		TConsensusStructure rfam_inter;
		bracketToInteractions(rfam_bracket, rfam_inter);

		free(rfam_bracket);
		*/


		Motif* rna_motif = new Motif();
		rna_motif->header = record.header;
		rna_motif->seqence_information = record.seqence_information;
		rna_motif->seedAlignment = record.alignment;

		// create structure for the whole multiple alignment
		std::cout << "Rfam:   " << record.seqence_information.at("SS_cons") << "\n";
		//DEBUG_MSG("Rfam:   " << record.seqence_information.at("SS_cons"));

		//#pragma omp critical

		if (options.pseudoknot)
			getConsensusStructure(*rna_motif, record, constraint_bracket, IPknotFold());
		else
			getConsensusStructure(*rna_motif, record, constraint_bracket, RNALibFold());

		std::cout << "\n";

		motifs[k] = rna_motif;

		if (options.constrain)
			free(constraint_bracket);
	}
	std::unordered_map<std::string, std::vector<RfamBenchRecord> > reference_pos;
	//outputStats(motifs);
	if (options.reference_file != ""){
		reference_pos = read_reference(options.reference_file);
	}
	else{
		std::cout << "No reference pos file given.\n";
		return 0;
	}

	std::cout << "Searching for the motifs.\n";

	// possibly refactor this into separate program

	seqan::StringSet<seqan::CharString> ids;
    seqan::StringSet<seqan::String<TBaseAlphabet> > seqs;

	seqan::SeqFileIn seqFileIn(toCString(options.genome_file));
	readRecords(ids, seqs, seqFileIn);

	std::cout << "Read reference DB with " << seqan::length(seqs) << " records\n";

												  //7798, 8054
	//std::cout << seqan::infix(seqan::value(seqs,0), 7797, 8053) << " sequence \n";

	/*
	StructureIterator strucIter(motifs[0]->profile[3].elements, options.match_len, false);

	std::set<std::string> patSet;
	std::unordered_map<std::string, std::vector<std::pair<std::string, std::pair<int, THashType> > > > stringCollision;
	//1710720
	//18475776
	//20528640


	while(false && bla != strucIter.end && bla2 != strucIter2.end){
		//std::cout << test << "\n";
		int a,b,c;
		bla = strucIter.get_next_char();
		bla2 = strucIter2.get_next_char();
		//std::tie(a,b,c) = bla;

		std::string strString = strucIter.printPattern();
		std::string strString2 = strucIter2.printPattern();

		//std::cout << strString << " " << strucIter.patLen() << "\n";
		//std::cout << strString << " " << strucIter.patPos() << " " << strucIter.patLen() << " " << strucIter.patHash() << " / " << hashString(strString) << "\n";
		//std::cout << "Next: " << strucIter.prof_ptr-.> \n";

		if (strucIter.patLen() >= options.match_len){
			patSet.insert(strucIter.printPattern(false));
			++test;
		}

		if (strucIter2.patLen() >= options.match_len){
			patSet2.insert(strucIter2.printPattern(false));
			++test2;
		}

		std::cout << strString << " " << strucIter.patHash() << " - " << strString2 << " " << strucIter2.patHash() << "\n";
		std::cout << test << " : " << patSet.size() << " - " << test2 << " : " << patSet2.size() << "\n";

		if (strString != strString2){
			std::cout << "Catch up\n";
			// let iter1 catch up
			while (strString != strString2){
				strucIter.get_next_char();
				strString = strucIter.printPattern();

				if (strucIter.patLen() >= options.match_len){
					patSet.insert(strucIter.printPattern(false));
					++test;
				}

				std::cout << strString << " " << strucIter.patHash() << " - " << strString2 << " " << strucIter2.patHash() << "\n";
				std::cout << test << " : " << patSet.size() << " - " << test2 << " : " << patSet2.size() << "\n";

				if (patSet.size() > patSet2.size()){
					std::cout << "???\n";
					return 0;
				}
			}

			//std::cout << strString << " - " << strString2 << "\n";
		}
	}

	if (false){
		while(bla != strucIter.end){
			bla = strucIter.get_next_char();
			if (strucIter.patLen() >= options.match_len){
				//patSet.insert(strucIter.printPattern(false));

				++test;
			}
		}
	}

	std::set<std::string> patSet;
	std::unordered_map<std::string, std::pair<std::vector<int>,std::vector<std::string> > > patList;
	StructureIterator strucIter(motifs[0]->profile[0].elements, options.match_len, false, options.freq_threshold);
	std::tuple<int, int, int> bla;
	uint64_t test = 0;

	int max_len = 0;

	if (true){
		while(bla != strucIter.end){
			bla = strucIter.get_next_char();

			int backtracked, lchar, rchar;
			std::tie(backtracked, lchar, rchar) = bla;

			//std::cout << strucIter2.printPattern() << " - (" << strucIter2.patPos().first << "," << strucIter2.patPos().second << ")\n";

			if (strucIter.patLen() >= options.match_len){
				//std::cout << strucIter.printPattern() << " - (" << strucIter.patPos().first << "," << strucIter.patPos().second << ")\n";

				max_len = std::max(max_len, strucIter.patPos().second - strucIter.patPos().first);

				std::string pat = strucIter.printPattern(false);
				std::string gapped_pat = strucIter.printPattern();

				patList[pat].first.push_back(gapped_pat.size());
				patList[pat].second.push_back(gapped_pat);

				int start = patList[pat].first[0];

				//if (!std::is_sorted(patList[pat].first.begin(), patList[pat].first.end())){
				if (!std::all_of(patList[pat].first.begin()+1, patList[pat].first.end(), [=](int i) {return (start < i);} )){
					std::cout << "Wahh?\n";
					for (int j=0; j < patList[pat].first.size(); ++j){
						std::cout << patList[pat].second[j] << " " << patList[pat].first[j] << "\n";
					}
					std::cout << "\n";

					return 0;
				}

				//patSet.insert(pat);
				++test;
			}
		}

	std::cout << test << " : " << patSet.size() << " - " << max_len << "\n";

	return 0;
	}
	*/

/*
	for (auto elem1: motifs[0]->profile[2].elements){
		std::cout << elem1.type << "\n";
		int ic = 0;
		for (auto muhmap : elem1.gap_lengths){
			std::cout << "Pos " << (ic++) << "\n";
			for (auto key : muhmap){
				std::cout << key.first << " - " << key.second << "\n";
			}
		}

		std::cout << "\n";
	}

	std::cout << test << " " << patSet.size() << " SIZE\n";

	std::cout << strucIter.count << "\n";
	std::cout << strucIter.single << " " << strucIter.paired << "\n";
	 */

	//for (std::set<std::string>::iterator it=patSet.begin(); it!=patSet.end(); ++it){
	//	std::cout << *it << "\n";
	//}

	//searchProfile(seqs, motifs[0]->profile[5], options.match_len);

	findFamilyMatches(seqs, motifs, reference_pos, options);

	//TStructure &prof1 = motifs[0]->profile[2];

	//std::cout << prof1.pos.first << " " << prof1.pos.second << "\n";







	//std::cout << std::endl;

    return 0;
}
