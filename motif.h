// ==========================================================================
//                                  motif.h
// ==========================================================================
// Copyright (c) 2006-2016, Knut Reinert, FU Berlin
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
// Author: Your Name <your.email@example.net>
// ==========================================================================

#ifndef APPS_RNAMOTIF_MOTIF_H_
#define APPS_RNAMOTIF_MOTIF_H_

#include "motif_structures.h"
#include <stack>

// ============================================================================
// Forwards
// ============================================================================

// ============================================================================
// Tags, Classes, Enums
// ============================================================================

// ============================================================================
// Metafunctions
// ============================================================================

// ============================================================================
// Functions
// ============================================================================

RNAProfileString addRNAProfile(StructureElement &structureElement, unsigned start, unsigned end, TAlign &align){
	typedef seqan::Row<TAlign>::Type TRow;

	RNAProfileString profileString;
	seqan::resize(profileString, end-start+1);
	StructureStatistics stats;

	// min and max length initialized with their most extreme possible values
	stats.min_length = end - start + 1;
	stats.max_length = 0;

	// store the profile of the alignment in [start,end]
	for (unsigned row=0; row < length(rows(align)); ++row){
		TRow & i_row = seqan::row(align,row);
		unsigned source_start = seqan::toSourcePosition(i_row, start);
		unsigned source_end = seqan::toSourcePosition(i_row, end);
		unsigned seqLength = source_end -  source_start+1;

		// check if we only had gaps in the region (end-start+1 doesn't work then)
		if (source_start == source_end && seqan::isGap(i_row, end))
			seqLength = 0;

		if (seqLength < stats.min_length){
			stats.min_length = seqLength;
		}

		if (seqLength > stats.max_length)
			stats.max_length = seqLength;

		// create profile of bases in this column
		for (unsigned i=0; i < seqan::length(profileString); ++i){
			int index = i+start;
			profileString[i].count[seqan::ordValue(seqan::row(align, row)[index])] += 1;
		}
	}

	if (structureElement.type == StructureType::HAIRPIN && stats.min_length < 3)
		stats.min_length = 3;

	structureElement.components.push_back(profileString);
	structureElement.statistics.push_back(stats);

	return profileString;
}

TStemLoopRegions findStemLoops(Motif &motif){
	TStemLoopRegions stemLoops;
	TInteractionPairs &consensus = motif.consensusStructure;

	std::pair<int, int > lastHairpin;
	std::stack<int> pairStack;

	// locate the stem loops
	for (size_t i = 0; i < consensus.size(); ++i){
		int bracket = consensus[i];

		// skip unmatched regions
		if (bracket == -1)
			continue;

		// opening bracket, save previous stem-loop if there was one
		if (bracket > i){
			// if we found a open/close match before, save that and reset
			if (lastHairpin.second > 0){
				stemLoops.push_back(lastHairpin);

				// clear stack and reset last hairpin found
				std::stack<int>().swap(pairStack);
				lastHairpin = std::pair<int, int>();
			}

			// save the opening bracket on the stack
			pairStack.push(i);
		}
		// closing bracket
		else {
			if (!pairStack.empty() && bracket == pairStack.top()){
				lastHairpin.first = bracket;
				lastHairpin.second = i;
				pairStack.pop();
			}
		}
	}

	// save last stem-loop if there is one
	if (lastHairpin.second > 0)
		stemLoops.push_back(lastHairpin);

	return stemLoops;
}

// * a run of opening brackets is the left side of a stem
// * any unpaired bases in between are interior loops
// 		* if no corresponding unpaired bases: bulge
// * innermost unpaired bases are the hairpin
void partitionStemLoop(Motif &motif, std::pair<int, int > stemLoopRegion){
	TInteractionPairs &consensus = motif.consensusStructure;
	TStructure stemStructure;

	size_t i = stemLoopRegion.first;
	do {
		int pos = i;
		int right = consensus[pos];

		// count the row of opening brackets
		if (right > pos){					// an opening bracket
			while (right > pos){			// while it's a series of opening brackets
				// check if the corresponding closing bracket follows
				// or if there is a bulge on the right side
				if ((consensus[pos+1] > pos+1) && consensus[right-1] == -1){
					std::cout << pos << " " << consensus[pos+1] << "\n";
					// there is one run of unpaired bases from right+1
					int unpaired = right+1;
					while (consensus[unpaired]==-1) ++unpaired;
					std::cout << "Left bulge in [" << right+1 << "," << unpaired-1 << "]\n";

					StructureElement bulge;

					bulge.type = LOOP;
					RNAProfileString bulgeProfile = addRNAProfile(bulge, right+1, unpaired-1, motif.seedAlignment);
					stemStructure.push_back(bulge);
				}

				++pos;
				right = consensus[pos];
			}

			StructureElement stem;

			std::cout << "Stem: [" << i << "," << pos-1 << " " << pos-i << "] ; [" << consensus[pos-1] << "," << consensus[i] << " " << consensus[i] - consensus[pos-1]+1 << "]\n";

			stem.type = STEM;
			RNAProfileString leftProfile  = addRNAProfile(stem, i, pos-1, motif.seedAlignment);
			RNAProfileString rightProfile = addRNAProfile(stem, consensus[pos-1], consensus[i], motif.seedAlignment);

			stemStructure.push_back(stem);
		}
		// if unpaired, count the length of the loop and check for a bulge
		else if (right == -1){
			StructureElement structure;

			// get right border bracket of other half of loop (->(...(..)...)<=)
			int run = pos;
			int rb = consensus[pos-1];
			while (consensus[run] == -1) ++run;

			// get partner of end bracket ((...->(..)<=...))
			int lb = consensus[run];

			// Left bulge
			if (rb - lb == 1){
				std::cout << "Left bulge in [" << pos << "," << run-1 << " " << run-pos << "]\n";

				structure.type = LOOP;
				RNAProfileString bulgeProfile = addRNAProfile(structure, pos, run-1, motif.seedAlignment);
			}
			// Hairpin (stop the outer loop here since all structures found)
			else if (rb == run){
				std::cout << "Hairpin in [" << pos << "," << run-1 << " " << run-pos << "]\n";

				structure.type = HAIRPIN;
				RNAProfileString hairpinProfile = addRNAProfile(structure, pos, run-1, motif.seedAlignment);
				break;
			}
			// Interior loop with left and right side
			else{
				std::cout << "Left loop: " << "[" << pos << "," << run-1 << "]" << " " << run-pos << " ; " << "Right loop: " << lb+1 << "," << rb-1 << " " << rb-1-lb << "\n";

				structure.type = LOOP;
				RNAProfileString leftProfile  = addRNAProfile(structure, pos, run-1, motif.seedAlignment);
				RNAProfileString rightProfile = addRNAProfile(structure, lb+1, rb-1, motif.seedAlignment);
			}

			stemStructure.push_back(structure);

			pos = run;
			right = consensus[pos];
		}
		else
			pos = pos + 1;

		i = pos;

	} while (i <= stemLoopRegion.second);

	motif.profile.push_back(stemStructure);
}

// Example: (((((((.......((((((((..(((((..(((((.....((((((....((((...))))))))))...((((....((.....))....))))))))).)))))....))).))))))))))))........((((((.....)))))).................
// take a structure table and determine the structural elements (stem, bulge, internal loop, hairpin)
void structurePartition(Motif &motif){
	TStemLoopRegions stemLoops = findStemLoops(motif);

	int pos = -8;
	for (auto pair : stemLoops){
		// visualize stem loops for debugging
		std::cout << std::string(pair.first-pos, ' ');
		std::cout << std::string(pair.second-pair.first+1, '+');
		pos += (pair.first-pos) + (pair.second-pair.first+1);
	}

	std::cout << "\n";

	// after locating stem loops, separate structural elements
	for (auto pair : stemLoops){
		// find structural elements
		partitionStemLoop(motif, pair);
	}

	return;
}

#endif  // #ifndef APPS_RNAMOTIF_MOTIF_H_
