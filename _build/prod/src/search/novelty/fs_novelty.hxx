
#pragma once

#include <memory>
#include <cassert>

#include <lapkt/novelty/evaluators.hxx>

namespace fs0 {
	class Atom;
	class AtomIndex;
}

namespace fs0 { namespace bfws {

//! An auxiliary class to inject into novelty evaluators in order to convert 
//! atoms into atom indexes. Good to keep the evaluators agnostic wrt the data
//! structures needed to perform this indexing operation
//! Works only for "state-variable" novelty features, i.e. features that exactly
//! represent the value of some state variable
class FSAtomValuationIndexer {
public:
	FSAtomValuationIndexer(const AtomIndex& atom_index) : 
		_atom_index(atom_index)
	{}
	
	FSAtomValuationIndexer(const FSAtomValuationIndexer&) = default;
	
	unsigned num_indexes() const;
	
	unsigned to_index(unsigned variable, int value) const;
	
	const Atom& to_atom(unsigned index) const;
	
protected:
	const AtomIndex& _atom_index;
};


//! Interfaces for both binary and multivalued novelty evaluators
using FSBinaryNoveltyEvaluatorI = lapkt::novelty::NoveltyEvaluatorI<bool>;
using FSMultivaluedNoveltyEvaluatorI = lapkt::novelty::NoveltyEvaluatorI<int>;


} } // namespaces
