
#pragma once

#include <languages/fstrips/base.hxx>
#include <fs_types.hxx>

namespace fs0 { class State; class Binding; class ProblemInfo; class SymbolData; }

namespace fs0 { namespace language { namespace fstrips {

class Axiom;

//! A logical term in FSTRIPS
class Term : public LogicalElement {
public:
	Term() = default;
	virtual ~Term() = default;

	Term* clone() const override = 0;
	
	//! Returns the value of the current term under the given (possibly partial) interpretation
	virtual ObjectIdx interpret(const PartialAssignment& assignment, const Binding& binding) const = 0;
	virtual ObjectIdx interpret(const State& state, const Binding& binding) const = 0;
	ObjectIdx interpret(const PartialAssignment& assignment) const;
	ObjectIdx interpret(const State& state) const;

	std::ostream& print(std::ostream& os, const ProblemInfo& info) const override;

	virtual bool operator==(const Term& other) const = 0;
	inline bool operator!=(const Term& rhs) const { return !this->operator==(rhs); }
	virtual std::size_t hash_code() const = 0;
};

//! A nested logical term in FSTRIPS, i.e. a term of the form f(t_1, ..., t_n)
//! The class is abstract and intended to have two possible subclasses, depending on whether
//! the functional symbol 'f' is fluent or not.
class NestedTerm : public Term {
public:
	LOKI_DEFINE_CONST_VISITABLE();

	NestedTerm(unsigned symbol_id, const std::vector<const Term*>& subterms)
		: _symbol_id(symbol_id), _subterms(subterms), _interpreted_subterms(subterms.size(), -1)
	{}

	~NestedTerm() {
		for (const Term* term:_subterms) delete term;
	}
	
	NestedTerm(const NestedTerm& term);
	
	//! Prints a representation of the object to the given stream.
	std::ostream& print(std::ostream& os, const fs0::ProblemInfo& info) const override;

	//! A helper to interpret a vector of terms
	template <typename T>
	static void
	interpret_subterms(const std::vector<const Term*>& subterms, const T& assignment, const Binding& binding, std::vector<ObjectIdx>& interpreted) {
		assert(interpreted.size() == subterms.size());
		for (unsigned i = 0, sz = subterms.size(); i < sz; ++i) {
			interpreted[i] = subterms[i]->interpret(assignment, binding);
		}
	}

	unsigned getSymbolId() const { return _symbol_id; }

	const std::vector<const Term*>& getSubterms() const { return _subterms; }

	bool operator==(const Term& other) const override;
	std::size_t hash_code() const override;


protected:
	//! The ID of the function or predicate symbol, e.g. in the state variable loc(A), the id of 'loc'
	unsigned _symbol_id;

	//! The tuple of fixed, constant symbols of the state variable, e.g. {A, B} in the state variable 'on(A,B)'
	// TODO This should be const
	std::vector<const Term*> _subterms;
	
	//! The last interpretation of the subterms (acts as a cache)
	mutable std::vector<ObjectIdx> _interpreted_subterms;
};


//! A nested term headed by a static functional symbol
class StaticHeadedNestedTerm : public NestedTerm {
public:
	LOKI_DEFINE_CONST_VISITABLE();
	
	StaticHeadedNestedTerm(unsigned symbol_id, const std::vector<const Term*>& subterms) : NestedTerm(symbol_id, subterms) {}
};

//! A statically-headed term that performs some arithmetic operation to its two subterms
class ArithmeticTerm : public StaticHeadedNestedTerm {
public:
	ArithmeticTerm(const std::vector<const Term*>& subterms);
	
	virtual ArithmeticTerm* clone() const override = 0;
	
	//! Creates an arithmetic term of the same type than the current one but with the given subterms
	// TODO - This is ATM somewhat inefficient because of the redundancy of cloning the whole array of subterms only to delete it.
	const Term* create(const std::vector<const Term*>& subterms) const {
		ArithmeticTerm* term = clone();
		for (const auto ptr:term->_subterms) delete ptr;
		term->_subterms = subterms;
		return term;
	}
};

//! A statically-headed term defined extensionally or otherwise by the concrete planning instance
class UserDefinedStaticTerm : public StaticHeadedNestedTerm {
public:
	LOKI_DEFINE_CONST_VISITABLE();
	
	UserDefinedStaticTerm(unsigned symbol_id, const std::vector<const Term*>& subterms);

	UserDefinedStaticTerm* clone() const override { return new UserDefinedStaticTerm(*this); }

	ObjectIdx interpret(const PartialAssignment& assignment, const Binding& binding) const override;
	ObjectIdx interpret(const State& state, const Binding& binding) const override;
	
	const SymbolData& getFunction() const { return _function; }

protected:
	// The (static) logical function implementation
	const SymbolData& _function;
};

//! 
class AxiomaticTerm : public StaticHeadedNestedTerm {
public:
	AxiomaticTerm(unsigned symbol_id, const std::vector<const Term*>& subterms)
		 : StaticHeadedNestedTerm(symbol_id, subterms) {}

	AxiomaticTerm* clone() const override;
	virtual AxiomaticTerm* clone(const std::vector<const Term*>& subterms) const = 0;

	virtual std::string name() const = 0;
		
	ObjectIdx interpret(const PartialAssignment& assignment, const Binding& binding) const override { throw std::runtime_error("Not yet implemented"); }
	ObjectIdx interpret(const State& state, const Binding& binding) const override;
	
	//! This needs to be overriden by the particular implementation
	virtual ObjectIdx compute(const State& state, std::vector<ObjectIdx>& arguments) const = 0;
};



//! 
//! @deprecated This is to be replaced by the class AxiomaticAtom
class AxiomaticTermWrapper : public StaticHeadedNestedTerm {
public:
	LOKI_DEFINE_CONST_VISITABLE();

	AxiomaticTermWrapper(const Axiom* axiom, unsigned symbol_id, const std::vector<const Term*>& subterms)
		 : StaticHeadedNestedTerm(symbol_id, subterms), _axiom(axiom)
	{}
	AxiomaticTermWrapper(const AxiomaticTermWrapper& other);
	AxiomaticTermWrapper* clone() const override { return new AxiomaticTermWrapper(*this); }

	ObjectIdx interpret(const PartialAssignment& assignment, const Binding& binding) const override;
	ObjectIdx interpret(const State& state, const Binding& binding) const override;
	
	const Axiom* getAxiom() const { return _axiom; }
	
	std::ostream& print(std::ostream& os, const fs0::ProblemInfo& info) const override;
	
protected:
	const Axiom* _axiom;
};

//! A nested term headed by a fluent functional symbol
class FluentHeadedNestedTerm : public NestedTerm {
public:
	LOKI_DEFINE_CONST_VISITABLE();

	FluentHeadedNestedTerm(unsigned symbol_id, const std::vector<const Term*>& subterms)
		: NestedTerm(symbol_id, subterms) {}

	FluentHeadedNestedTerm* clone() const override { return new FluentHeadedNestedTerm(*this); }

	ObjectIdx interpret(const PartialAssignment& assignment, const Binding& binding) const override;
	ObjectIdx interpret(const State& state, const Binding& binding) const override;
};

//! A logical variable bound to some existential or universal quantifier
class BoundVariable : public Term {
public:
	LOKI_DEFINE_CONST_VISITABLE();
	
	BoundVariable(unsigned id, const std::string& name, TypeIdx type) : _id(id), _name(name), _type(type) {}

	BoundVariable* clone() const override { return new BoundVariable(*this); }
	
	TypeIdx getType() const { return _type; }

	//! Returns the unique quantified variable ID
	unsigned getVariableId() const { return _id; }
	
	//! Returns the name of the variable
	const std::string& getName() const { return _name; }

	ObjectIdx interpret(const PartialAssignment& assignment, const Binding& binding) const override;
	ObjectIdx interpret(const State& state, const Binding& binding) const override;

	//! Prints a representation of the object to the given stream.
	std::ostream& print(std::ostream& os, const fs0::ProblemInfo& info) const override;

	bool operator==(const Term& other) const override;
	std::size_t hash_code() const override;

protected:
	//! The ID of the variable, which will be unique throughout the whole binding unit.
	unsigned _id;
	
	const std::string _name;
	
	TypeIdx _type;
};

//! A state variable is a term 'f(t)', where f is a fluent symbol and t is a tuple of fixed constant symbols.
//! 'loc(a)', with a being an object, for instance, is a state variable
class StateVariable : public Term {
public:
	LOKI_DEFINE_CONST_VISITABLE();
	
	StateVariable(VariableIdx variable_id, const FluentHeadedNestedTerm* origin) 
		: _variable_id(variable_id), _origin(origin)
	{}
	
	virtual ~StateVariable() {
		delete _origin;
	}

	StateVariable(const StateVariable& variable)
		: _variable_id(variable._variable_id), 
		  _origin(variable._origin->clone())
	{}

	StateVariable* clone() const override { return new StateVariable(*this); }
	
	//! Returns the index of the state variable
	VariableIdx getValue() const { return _variable_id; }

	ObjectIdx interpret(const PartialAssignment& assignment, const Binding& binding) const override { return assignment.at(_variable_id); }
	ObjectIdx interpret(const State& state, const Binding& binding) const override;

	const FluentHeadedNestedTerm* getOrigin() const { return _origin; }
	
	unsigned getSymbolId() const { return _origin->getSymbolId(); }
 
	virtual const std::vector<const Term*>& getSubterms() const { return _origin->getSubterms(); }

	//! Prints a representation of the object to the given stream.
	std::ostream& print(std::ostream& os, const fs0::ProblemInfo& info) const override;

	bool operator==(const Term& other) const override;
	std::size_t hash_code() const override;

protected:
	//! The ID of the state variable
	VariableIdx _variable_id;

	//! The originating symbol ID and subterms
	const FluentHeadedNestedTerm* _origin;
};


//! A simple constant term.
class Constant : public Term {
public:
	LOKI_DEFINE_CONST_VISITABLE();
	
	Constant(ObjectIdx value)  : _value(value) {}

	Constant* clone() const override { return new Constant(*this); }
	
	//! Returns the actual value of the constant
	ObjectIdx getValue() const { return _value; }

	// The value of a constant is independent of the assignment
	ObjectIdx interpret(const PartialAssignment& assignment, const Binding& binding) const override { return _value; }
	ObjectIdx interpret(const State& state, const Binding& binding) const override { { return _value; }}

	//! Prints a representation of the object to the given stream.
	std::ostream& print(std::ostream& os, const fs0::ProblemInfo& info) const override;

	bool operator==(const Term& other) const override;
	std::size_t hash_code() const override;

protected:
	//! The actual value of the constant
	ObjectIdx _value;
};


//! An integer constant
class IntConstant : public Constant {
public:
	IntConstant(ObjectIdx value)  : Constant(value) {}

	IntConstant* clone() const override { return new IntConstant(*this); }

	//! Prints a representation of the object to the given stream.
	std::ostream& print(std::ostream& os, const fs0::ProblemInfo& info) const override;
};


} } } // namespaces

// std specializations for terms and term pointers that will allow us to use them in hash-table-like structures
// NOTE that these specializations are necessary for any use of term pointers in std::map/std::unordered_maps,
// even if compilation will succeed without them as well.
namespace fs = fs0::language::fstrips;
namespace std {
    template<> struct hash<fs::Term> {
        std::size_t operator()(const fs::Term& term) const { return term.hash_code(); }
    };

    template<> struct hash<const fs::Term*> {
        std::size_t operator()(const fs::Term* term) const { return hash<fs::Term>()(*term); }
    };
	
    template<> struct equal_to<const fs::Term*> {
        std::size_t operator()(const fs::Term* t1, const fs::Term* t2) const { return equal_to<fs::Term>()(*t1, *t2); }
    };
}
