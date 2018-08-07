#ifndef CTRE_V2__CTRE__PARSER__HPP
#define CTRE_V2__CTRE__PARSER__HPP

#include "type-stack.hpp"

#define RULE static constexpr auto rule
#define NONTERM(name) struct name { }
#define START_NONTERM(name) struct name { }; using start = name
#define SUBJECT_TYPE(name) using subject_type = name

namespace ctre {

// basic formal language constructs

template <auto v> struct term {
	static constexpr auto value = v;
};

struct accept { constexpr explicit operator bool() noexcept { return true; } };

struct reject { constexpr explicit operator bool() noexcept { return false; } };

template <auto A, decltype(A) B> struct range {
	constexpr range() noexcept { };
	//template <auto V> constexpr range(term<V>) noexcept requires (A <= V) && (V <= B);
	template <auto V, typename = std::enable_if_t<(A <= V) && (V <= B)>> constexpr range(term<V>) noexcept;
};

template <auto... Def> struct set {
	constexpr set() noexcept { };
	//template <auto V> constexpr set(term<V>) noexcept requires ((Def == V) || ... || false);
	template <auto V, typename = std::enable_if_t<((Def == V) || ... || false)>> constexpr set(term<V>) noexcept;
};

template <auto... Def> struct neg_set {
	constexpr neg_set() noexcept { };
	//template <auto V> constexpr set(term<V>) noexcept requires ((Def == V) || ... || false);
	template <auto V, typename = std::enable_if_t<((Def != V) && ... && true)>> constexpr neg_set(term<V>) noexcept;
};

template <auto... Def> struct anything {
	constexpr anything() noexcept { };
	template <auto V> constexpr anything(term<V>) noexcept;
};

template <typename T> struct IsExplicitlyConvertibleToBool {
	template <typename Y> static constexpr auto test(Y * y) -> decltype(bool(*y), std::true_type{});
	template <typename> static constexpr auto test(...) -> std::false_type;
	static constexpr const bool value = decltype(test<T>(nullptr))();
};

template <typename T> struct IsStepForward {
	template <typename Y> static constexpr auto test(Y *) -> decltype(Y::move_forward(), std::true_type{});
	template <typename> static constexpr auto test(...) -> std::false_type;
	static constexpr const bool value = decltype(test<T>(nullptr))();
};

template <typename T, typename Subject> struct IsActionItem {
	template <typename Y> static constexpr auto test(Y * y) -> decltype((*y)(Subject()), std::true_type{});
	template <typename> static constexpr auto test(...) -> std::false_type;
	static constexpr const bool value = decltype(test<T>(nullptr))();
};

// everything else can be used as a nonterminal

template <typename Grammar> struct augment_grammar: public Grammar {
	using typename Grammar::start;
	using Grammar::rule; // Grammar rules should have same priority
	
	// default behaviour is reject if there is unexpected state
	static constexpr auto rule(...) -> reject;
	
	// if there are two same terms on top of the stack and current input, you should move forward
	template <auto A> static constexpr auto rule(term<A>, term<A>) -> pop_input;
	
	//template <auto A, auto B, auto V> static constexpr auto rule(range<A,B>, term<V>) -> pop_input requires ((A <= V) && (V <= B));
	template <auto A, auto B, auto V, typename = std::enable_if_t<((A <= V) && (V <= B))>> static constexpr auto rule(range<A,B>, term<V>) -> pop_input;
	
	
	//template <auto... Def, auto V> static constexpr auto rule(set<Def...>, term<V>) -> pop_input requires ((V == Def) || ... || false);
	template <auto... Def, auto V, typename = std::enable_if_t<((V == Def) || ... || false)>> static constexpr auto rule(set<Def...>, term<V>) -> pop_input;
	
	// empty stack and empty input means we are accepting
	static constexpr auto rule(epsilon, epsilon) -> accept;
};

struct empty_subject { };

template <typename Subject> struct parse_result {
	bool value;
	size_t steps;
	Subject subject;
	constexpr parse_result(bool value, size_t steps, Subject subject) noexcept: value{value}, steps{steps}, subject{subject} { }
};
	
//template <typename G, FixedString input> struct parser { // in c++20
template <typename G, const auto & input> struct parser {
	using grammar = augment_grammar<G>;
	static constexpr size_t size = input.size();
	template <size_t pos> static constexpr auto current() {
		if constexpr (pos < size) {
			return term<input[pos]>();
		} else {
			return epsilon();
		}
	}
	template <size_t pos = 0, typename Head> static constexpr auto get_move(Head head) {
		return decltype(grammar().rule(Head(), current<pos>()))();
	}
	template <typename Subject = empty_subject> static constexpr auto decide(const Subject subject = Subject{}) {
		return decide(list<typename G::start>(), 1, subject);
	}
	template <size_t pos = 0, typename Stack, typename Subject = empty_subject> static constexpr auto decide(Stack stack, unsigned step, const Subject subject) {
		auto next_item_on_stack = head(stack);
		
		if constexpr (IsActionItem<decltype(next_item_on_stack), Subject>::value) {
			// modify the subject
			return decide<pos>(pop(stack), step+1, next_item_on_stack(subject));
		}
		
		auto m = get_move<pos>(next_item_on_stack);
		if constexpr (IsExplicitlyConvertibleToBool<decltype(m)>::value) {
			// accept or reject state
			return parse_result<Subject>(bool(m), step, subject);
		} else {
			// move forward with parsing
			// if decltype(m) is pop_char then move a char forward
			return decide<pos+IsStepForward<decltype(m)>::value>(pop_and_push(m, stack), step+1, subject);
		}
	}
};

}

#endif