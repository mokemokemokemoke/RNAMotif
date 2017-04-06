// ==========================================================================
//                               motif_search.h
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

#ifndef APPS_RNAMOTIF_MOTIF_SEARCH_H_
#define APPS_RNAMOTIF_MOTIF_SEARCH_H_

#include "motif_structures.h"
#include "motif.h"

// ============================================================================
// Forwards
// ============================================================================

// ============================================================================
// Tags, Classes, Enums
// ============================================================================

/* ------------------------------------------------------- */
/*!
 * @class ProfileCharIter
 *
 * @brief Abstract base class of the ProfileChar iterator.
 *
 *	Allows us to use different subclass iterators for different ProfileChar
 *	Alphabet types while searching. (Mainly for Nucleotides and BiNucleotides)
 */

class ProfileCharIter{
public:
	bool inner  = false;
	bool gapped = false;

	virtual int getNextChar() = 0;
	virtual bool atEnd() = 0;
	virtual void setEnd() = 0;
	virtual ~ProfileCharIter() {};
};

/*!
 * @class ProfileCharIterImpl
 *
 * @brief Implement the ProfileCharIter for all types of ProfileChars.
 *
 *	Templated implementation for ProfileChar types of different alphabets.
 *	Return the next characters as for all as int for consistency.
 */

template <typename TProfileChar>
class ProfileCharIterImpl : public ProfileCharIter{
	typedef typename seqan::SourceValue<TProfileChar>::Type TProfileAlphabet;
	TProfileChar c;
	const int char_size = seqan::ValueSize<TProfileChar>::VALUE;

	int state = 0;
	std::vector<int> idx;
	int total;

public:
	ProfileCharIterImpl (TProfileChar c) : c(c), idx(char_size), total(seqan::totalCount(c)){
		// fill index vector with indies [0,1,..,N-1]
		std::iota(idx.begin(), idx.end(), 0);

		// sort index by comparing the entries in tmp to get order
		std::sort(idx.begin(), idx.end(),
			[&] (int i1, int i2) {
				return (c.count[i1] > c.count[i2]);
			}
		);
	}

	int getNextChar(){
		if (atEnd())
			return -1;
		return TProfileAlphabet(idx[state++]);
	}

	// end if all chars that occurred were returned
	bool atEnd(){
		return ((state == char_size) || (c.count[idx[state]] == 0));
	}

	void setEnd(){
		state = char_size;
	}
};

class StructureIterator{
	typedef ProfileCharIterImpl<TAlphabetProfile> TSinglePointer;
	typedef ProfileCharIterImpl<TBiAlphabetProfile> TPairPointer;
	typedef std::shared_ptr<ProfileCharIter> ProfilePointer;

	std::vector<StructureElement> structure_elements;
	int element;
	int elem_length;
	int pos;
	uint64_t sum;
	ProfilePointer prof_ptr;

public:
	uint64_t count;
	std::stack<ProfilePointer> state;
	std::pair<int,int> end;


	StructureIterator(std::vector<StructureElement> &structure_elements) : end(-1,-1) {
		sum = 1;
		for (StructureElement elem : structure_elements){
			for (int i=0; i < seqan::length(elem.loopComponents[0]); ++i){
				int nonzero = 0;

				if (elem.type != StructureType::STEM){
					TAlphabetProfile &pchar = elem.loopComponents[0][i];
					int count = std::count_if(std::begin(pchar.count), std::end(pchar.count), [&] (int x) {return (x > 0);});
					nonzero = count;
					std::cout << "No stem: " << count << "\n";
				}
				else {
					TBiAlphabetProfile &pchar = elem.stemProfile[i];
					int count = std::count_if(std::begin(pchar.count), std::end(pchar.count), [&] (int x) {return (x > 0);});
					nonzero = count;
					std::cout << "Stem   : " << count << "\n";
				}

				sum *= nonzero;
				//std::cout << sum <<"\n";
			}
		}

		std::cout << "Number of sequences: " << sum << "\n";

		this->structure_elements = structure_elements;
		this->element = structure_elements.size()-1;
		this->elem_length = seqan::length(structure_elements[element].loopComponents[0]);
		this->pos = 0;
		this->count = 0;

		auto &hairpin = structure_elements[element].loopComponents[0];

		prof_ptr = ProfilePointer(new TSinglePointer(hairpin[pos]));
		//state.push(prof_ptr);
	}

	void printState(){

	}

	std::pair<int, int> get_next_char(){
		std::pair<int, int> ret;

		// get previous state (backtrack to valid state if necessary)
		while (prof_ptr->atEnd()) {
			// if no more characters can be generated
			if (state.size() == 0){
				std::cout << "ENDING\n";
				return this->end;
			}

			--pos;

			// check if the substructure has to be changed again
			if (pos < 0) {
				//std::cout << "Going up\n";
				++element;
				elem_length = seqan::length(structure_elements[element].loopComponents[0]);
				pos = elem_length - 1;
			}

			prof_ptr = state.top();
			state.pop();
		}

		//std::cout << pos << " " << elem_length << " " << element << "\n";

		// advance to the next character in the active state
		int next_char_val = prof_ptr->getNextChar();

		// return one or two characters to search, depending on the substructure
		if (typeid(prof_ptr) == typeid(TSinglePointer)){
			if (structure_elements[element].loopLeft == false)
				ret = std::make_pair(-1, next_char_val);
			else
				ret = std::make_pair(next_char_val, -1);

		}
		else{
			int lchar = next_char_val / AlphabetSize;
			int rchar = next_char_val % AlphabetSize;

			ret = std::make_pair(lchar, rchar);
		}

		// advance in the structure elements if possible
		if (pos < (elem_length-1)) {
			++pos;
		}
		// go to next substructure if necessary
		else if ((pos == (elem_length-1)) && (element > 0)){
			//std::cout << "Changing structures\n";
			--element;
			elem_length = seqan::length(structure_elements[element].loopComponents[0]);
			pos = 0;
		}
		// element == 0, searched everything
		else {
			//std::cout << "This is the end..\n";
			++count;
			if (count % 1000000 == 0){
				std::cout << state.size() << " - " << count << " / " << sum << "\n";
				std::stack<ProfilePointer> stateCopy = state;

				while (!stateCopy.empty()){
					std::cout << (int)stateCopy.top()->atEnd();
					stateCopy.pop();
				}

				std::cout << "\n";

				//std::cout << "Lastchar: " << next_char_val << "\n";
			}

			return ret;
		}

		state.push(prof_ptr); // save last character state

		// update character pointer
		if (structure_elements[element].type == StructureType::STEM) {
			prof_ptr = ProfilePointer(new TPairPointer(structure_elements[element].stemProfile[pos]));
		}
		else {
			prof_ptr = ProfilePointer(new TSinglePointer(structure_elements[element].loopComponents[0][pos]));
		}

		return ret;
	}

	// do not extend the current word further
	void reset_char(){
		prof_ptr->setEnd();
	}
};

/* ------------------------------------------------------- */

template <typename TBidirectionalIndex>
class MotifIterator{
	typedef typename seqan::Iterator<TBidirectionalIndex, seqan::TopDown<seqan::ParentLinks<> > >::Type TIterator;
	typedef typename seqan::SAValue<TBidirectionalIndex>::Type THitPair;
	typedef seqan::String< THitPair > TOccurenceString;

	StructureIterator structure_iter;
	TIterator it;
	bool cont = true;

	// threshold: stop expanding when below this likelihood for the sequence
	unsigned min_match;

	bool setEnd(){
		return (cont = false);
	}

	bool stepIterator(int next_char, StructureType stype){
		bool success = false;

		if (stype == STEM){
			// partition stem char into components (using / and % instead
			// of bit shifting since AlphabetSize might not be power of 2)
			int lchar = next_char / AlphabetSize;
			int rchar = next_char % AlphabetSize;

			//std::cout << TBaseAlphabet(lchar) << " " << TBaseAlphabet(rchar) << " " << stem_char << "\n";

			// needs to extend into both directions:
			bool wentLeft  = seqan::goDown(it, lchar, seqan::Fwd());
			bool wentRight = seqan::goDown(it, rchar, seqan::Rev());

			success = wentLeft && wentRight;

			if (wentLeft ^ wentRight)
				seqan::goUp(it);

		}
		else{
			success = goDown(it, next_char, seqan::Rev());
		}

		return success;
	}

public:
	MotifIterator(TStructure &structure, TBidirectionalIndex &index, double min_match)
		: structure_iter(structure.elements), it(index), min_match(min_match){
	}

	// next() returns true as long as the motif is not exhausted.
	// Only 'valid' matches are iterated: exclude those who do not match
	// or do not represent the family well (prob. below threshold)
	bool next(){
		if (!cont){
			return false;
		}

		// get the next characters to search for (either one or two, depending on the search direction)
		std::pair<int, int> n_char = structure_iter.get_next_char();

		if (n_char == structure_iter.end){
			cont = false;
			return false;
		}

		int lchar, rchar;
		std::tie(lchar, rchar) = n_char;

		bool wentLeft  = (lchar == -1) ? false : seqan::goDown(it, lchar, seqan::Fwd());
		bool wentRight = (rchar == -1) ? false : seqan::goDown(it, rchar, seqan::Rev());

		// one-directional extension (lchar or rchar -1)
		if (lchar * rchar < 0) {

		}



		// could not extend in one of the directions - reset
		if (wentLeft ^ wentRight){
			seqan::goUp(it);
		}

		// cannot progress any further in the given directions
		if (!wentLeft && !wentRight)
			return true;


		return true;
	}

	TOccurenceString getOccurrences(){
		// FIXME: getting occurrences directly via
		// occs = seqan::getOccurrences(it) doesn't work for some reason
		TOccurenceString occs;
		for (THitPair test: seqan::getOccurrences(it))
			seqan::appendValue(occs, test);

		return occs;
	}

	unsigned countOccurrences(){
		return seqan::countOccurrences(it);
	}
};

// ============================================================================
// Metafunctions
// ============================================================================

// ============================================================================
// Functions
// ============================================================================

template <typename TBidirectionalIndex>
//std::vector<TProfileInterval> getStemloopPositions(TBidirectionalIndex &index, Motif &motif){
void getStemloopPositions(TBidirectionalIndex &index, Motif &motif){
	//typedef typename seqan::SAValue<TBidirectionalIndex>::Type THitPair;
	typedef seqan::String< TIndexPosType > TOccurenceString;

	//std::vector<TOccurenceString> result(profile.size());

	// create one interval tree for each contig in the reference genome
	//std::vector<TProfileInterval> intervals(seqan::countSequences(index));
	//std::vector<TProfileInterval> intervals;

	std::cout << motif.mcc << " HALLO\n";

	int id = 0;
	int n = seqan::length(motif.seedAlignment);
	int stems = motif.profile.size();

	for (TStructure &structure : motif.profile){
		// start of the hairpin in the whole sequence
		std::cout <<  structure.pos.first << " " << structure.pos.second << ": " << motif.profile.size() << "\n";

		int loc = motif.profile[id].elements.back().location;

		std::cout << "Elements back\n";

		MotifIterator<TBidirectionalIndex> iter(structure, index, 11);

		std::cout << "Starting iterator\n";

		while (iter.next()){
			TOccurenceString occs = iter.getOccurrences();

			std::cout << iter.countOccurrences() << "\n";

			/*

			for (TIndexPosType pos : occs){
				TProfileInterval &interval = intervals[pos.i1];

				seqan::String<TProfileCargo> hits;

				// check if the stem occurs in an already existing match region
				findIntervals(hits, interval, pos.i2);

				// if this stem isn't located in an existing match region
				if (seqan::length(hits) == 0){
					//std::cout << loc << " " << n << " " << pos.i2 << " " << pos.i2-loc << " " << pos.i2 + (n-loc) << "\n";

					std::shared_ptr<std::vector<bool> > stemSet(new std::vector<bool>(stems));
					(*stemSet)[id] = true;
					// add an interval around the matched stem
					seqan::addInterval(interval, pos.i2-loc, pos.i2+(n-loc), stemSet);
				}
				// else add to the list of hits
				else{
					for (unsigned i=0; i < seqan::length(hits); ++i){
						(*hits[i].cargo)[id] = true;
					}
				}
			}
			//std::cout << "\n";
			*/

		}

		//for (unsigned i=0; i < seqan::countSequences(index); ++i)
		//	std::cout << "After " << id << "," << i << ": " << intervals[i].interval_counter << "\n";

		//std::cout << seqan::length(result[id]) << "\n";
		//seqan::sort(result[id]);
		++id;
	}

	return;
}

void countHits(TProfileInterval positions, int window_size){
	seqan::String<TProfileCargo> hits;
	seqan::getAllIntervals(hits, positions);

	for (unsigned i=0; i < seqan::length(hits); ++i)
		// only report hits where all stems occurred in the region
		if (std::all_of(hits[i].cargo->begin(), hits[i].cargo->end(), [](bool v) { return v; }))
			std::cout << hits[i].i1 << " " << hits[i].i2 << "\n";
}


template <typename TStringType>
std::vector<seqan::Tuple<int, 3> > findFamilyMatches(seqan::StringSet<TStringType> &seqs, std::vector<Motif*> &motifs){
	std::vector<seqan::Tuple<int, 3> > results;

	TBidirectionalIndex index(seqs);

    for (Motif *motif : motifs){
    	std::cout << motif->header.at("ID") << "\n";
    	// find the locations of the motif matches
    	//std::cout << motif.header.at("AC") << "\n";
    	//std::vector<TProfileInterval> result = getStemloopPositions(index, *motif);
    	getStemloopPositions(index, *motif);

		// cluster results into areas (i.e. where hairpins of a given type cluster together)
    	//std::cout << result[1].interval_counter << "\n";
    	//countHits(result[1], motif.consensusStructure.size());
    	//for (TProfileInterval intervals : result)
    		//countHits(intervals, motif.consensusStructure.size());
    }

	return results;
}

#endif  // #ifndef APPS_RNAMOTIF_MOTIF_SEARCH_H_
