/**
 * Unify.h
 *
 * Utilities for unifying atoms.
 *
 * Copyright (C) 2016 OpenCog Foundation
 * All Rights Reserved
 * Author: Nil Geisweiller
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _OPENCOG_UNIFY_UTILS_H
#define _OPENCOG_UNIFY_UTILS_H

#include <boost/operators.hpp>

#include <opencog/atoms/base/Handle.h>
#include <opencog/atoms/base/atom_types.h>
#include <opencog/atoms/base/Quotation.h>
#include <opencog/atoms/core/VariableList.h>
#include <opencog/atoms/pattern/BindLink.h>
#include <opencog/atomutils/TypeUtils.h>

namespace opencog {

class Unify
{
public:
	// Mapping from partition blocks to type
	typedef std::map<OrderedHandleSet, Handle> Partition;
	typedef Partition::value_type Block;
	typedef std::set<Partition> Partitions;

	// TODO: the type of a typed block is currently a handle of the
	// variable or ground it is exists, instead of an actual type.
	struct SolutionSet :
		public boost::equality_comparable<SolutionSet>
	{
		// Default ctor
		SolutionSet(bool s=true, const Partitions& p=Partitions());

		// Whether the unification satisfiable. Not that satisfiable is
		// different than empty. An empty solution set may still be
		// satisfiable, that would be the case of two candidates that
		// match but have no variables.
		bool satisfiable;

		// Set of typed partitions
		Partitions partitions;

		bool operator==(const SolutionSet& other) const;
	};

	// Subtitution values and their corresponding variable declaration
	// (cause some values will be variables).
	typedef std::map<HandleMap, Handle> TypedSubstitutions;
	typedef TypedSubstitutions::value_type TypedSubstitution;

	/**
	 * Generate typed substitution rules, given a UnificationSolutionSet
	 * and the term from which to select the variables as values in case
	 * multiple choices are possible.
	 *
	 * Remarks: lhs_vardecl and rhs_vardecl are passed by copy because
	 * they will be filled with free variables in case they are empty.
	 */
	TypedSubstitutions typed_substitutions(const SolutionSet& sol,
	                                       const Handle& pre,
	                                       const Handle& lhs=Handle::UNDEFINED,
	                                       const Handle& rhs=Handle::UNDEFINED,
	                                       Handle lhs_vardecl=Handle::UNDEFINED,
	                                       Handle rhs_vardecl=Handle::UNDEFINED) const;

	/**
	 * If the quotations are useless or harmful, which might be the
	 * case if they deprive a ScopeLink from hiding supposedly hidden
	 * variables, consume them.
	 *
	 * Specifically this code makes 2 assumptions
	 *
	 * 1. LocalQuotes in front root level And, Or or Not links on the
	 *    pattern body are not consumed because they are supposedly
	 *    used to avoid interpreting them as pattern matcher
	 *    connectors.
	 *
	 * 2. Quote/Unquote are used to wrap scope links so that their
	 *    variable declaration can pattern match grounded or partially
	 *    grounded scope links.
	 *
	 * No other use of quotation is assumed besides the 2 above.
	 */
	BindLinkPtr consume_ill_quotations(BindLinkPtr bl) const;
	Handle consume_ill_quotations(const Variables& variables, Handle h,
	                              Quotation quotation=Quotation(),
	                              bool escape=false /* ignore the next
	                                                 * quotation
	                                                 * consumption */) const;

	/**
	 * Return true iff the variable declaration of local_scope is a
	 * variable of variables wrapped in a UnquoteLink.
	 */
	bool is_bound_to_ancestor(const Variables& variables,
	                          const Handle& local_scope) const;

	/**
	 * Return true iff the handle or type correspond to a pattern
	 * matcher connector.
	 */
	bool is_pm_connector(const Handle& h) const;
	bool is_pm_connector(Type t) const;

	/**
	 * Given a typed substitution, perform the substitution over a scope
	 * link (for now only BindLinks are supported).
	 */
	Handle substitute(BindLinkPtr bl, const TypedSubstitution& ts) const;

	/**
	 * This algorithm perform unification by recursively
	 *
	 * 1. Generate all equality partitions
	 *
	 * 2. Decorate partition blocks with types
	 *
	 * 3. Check that each partition is satisfiable
	 *
	 * For now the types in 2. are represented by the substitutions, for
	 * instance the typed block {{X, A}, A} means that X is A. Later one
	 * we will replace that by deep types as to represents things like
	 * {{X, Y}, ConceptNode} meaning that X and Y must be concept nodes is
	 * order to be satisfiable, of course the deep will still need to
	 * capture grounds such as {{X, A}, A}, it's not clear excatly how,
	 * maybe Linas deep type implementation can already do that.
	 *
	 * To solve 3, for each partition block, it computes the type that
	 * intersects all its elements and repeat till a fixed point is
	 * reached. To do that efficiently we would need to build a dependency
	 * DAG, but at first we can afford to compute type intersections is
	 * random order.
	 *
	 * Also, permutations are supported though very slow.
	 *
	 * Examples:
	 *
	 * 1.
	 *
	 * unify((Variable "$X"), (Concept "A"))
	 * ->
	 * {{<{(Variable "$X"), (Concept "A")}, (Concept "A")>}}
	 *
	 * meaning that the partition block {(Variable "$X"), (Concept "A")}
	 * has type (Concept "A"), and there is only one partition in the
	 * solution set.
	 *
	 * 2.
	 *
	 * unify((Concept "A"), (Concept "$X"))
	 * ->
	 * {{<{(Variable "$X"), (Concept "A")}, (Concept "A")>}}
	 *
	 * 3.
	 *
	 * unify((Inheritance (Concept "A") (Concept "B")), (Variable "$X"))
	 * ->
	 * {{<{(Variable "$X"), (Inheritance (Concept "A") (Concept "B"))},
	 *    (Inheritance (Concept "A") (Concept "B"))>}}
	 *
	 * 4.
	 *
	 * unify((Inheritance (Concept "A") (Variable "$Y")),
	 *       (Inheritance (Variable "$X") (Concept "B"))
	 * ->
	 * {{<{(Variable "$X"), (Concept "A")}, (Concept "A")>,
	 *   <{(Variable "$Y"), (Concept "B")}, (Concept "B")>}}
	 *
	 * 5.
	 *
	 * unify((And (Concept "A") (Concept "B")),
	 *       (And (Variable "$X") (Variable "$Y"))
	 * ->
	 * {
	 *  {<{(Variable "$X"), (Concept "A")}, (Concept "A")>,
	 *   <{(Variable "$Y"), (Concept "B")}, (Concept "B")>},
	 *
	 *  {<{(Variable "$X"), (Concept "B")}, (Concept "B")>,
	 *   <{(Variable "$Y"), (Concept "A")}, (Concept "A")>}
	 * }
	 *
	 * mean that the solution set has 2 partitions, one where X unifies to
	 * A and Y unifies to B, and another one where X unifies to B and Y
	 * unifies to A.
	 *
	 * TODO: take care of Un/Quote and Scope links.
	 */
	SolutionSet operator()(const Handle& lhs, const Handle& rhs,
	                       const Handle& lhs_vardecl=Handle::UNDEFINED,
	                       const Handle& rhs_vardecl=Handle::UNDEFINED,
	                       Quotation lhs_quotation=Quotation(),
	                       Quotation rhs_quotation=Quotation());

private:
	Handle _lhs_vardecl;
	Handle _rhs_vardecl;

	/**
	 * Unify lhs and rhs. _lhs_vardecl and _rhs_vardecl should be set
	 * prior to run this method.
	 */
	SolutionSet unify(const Handle& lhs, const Handle& rhs,
	                  Quotation lhs_quotation=Quotation(),
	                  Quotation rhs_quotation=Quotation()) const;

	/**
	 * Unify all elements of lhs with all elements of rhs, considering
	 * all permutations.
	 */
	SolutionSet unordered_unify(const HandleSeq& lhs, const HandleSeq& rhs,
	                            Quotation lhs_quotation=Quotation(),
	                            Quotation rhs_quotation=Quotation()) const;

	/**
	 * Unify all elements of lhs with all elements of rhs, in the
	 * provided order.
	 */
	SolutionSet ordered_unify(const HandleSeq& lhs, const HandleSeq& rhs,
	                          Quotation lhs_quotation=Quotation(),
	                          Quotation rhs_quotation=Quotation()) const;

	/**
	 * Unify all elements of lhs with all elements of rhs, considering
	 * all pairwise combinations.
	 */
	SolutionSet comb_unify(const OrderedHandleSet& lhs,
	                       const OrderedHandleSet& rhs,
	                       Quotation lhs_quotation=Quotation(),
	                       Quotation rhs_quotation=Quotation()) const;

	/**
	 * Return if the atom is an unordered link.
	 */
	bool is_unordered(const Handle& h) const;

	/**
	 * Return a copy of a HandleSeq with the ith element removed.
	 */
	HandleSeq cp_erase(const HandleSeq& hs, Arity i) const;

	/**
	 * Build elementary solution set between 2 atoms given that at least
	 * one of them is a variable.
	 */
	SolutionSet mkvarsol(const Handle& lhs, const Handle& rhs,
	                     Quotation lhs_quotation,
	                     Quotation rhs_quotation) const;

public:                         // TODO: being friend with UnifyUTest
                                // somehow doesn't work
	friend class UnifyUTest;

	/**
	 * Join 2 solution sets. Generate the product of all consistent
	 * solutions (with partitions so that all blocks are typed with a
	 * defined Handle).
	 */
	SolutionSet join(const SolutionSet& lhs, const SolutionSet& rhs) const;

private:
	/**
	 * Join a satisfiable partition sets with a satisfiable partition.
	 */
	Partitions join(const Partitions& lhs, const Partition& rhs) const;

	/**
	 * Join 2 partitions. The result can be set of partitions (see
	 * join(const Partition&, const Block&) for explanation).
	 */
	Partitions join(const Partition& lhs, const Partition& rhs) const;

	/**
	 * Join a block with a partition set. The partition set is assumed
	 * non empty and satisfiable.
	 */
	Partitions join(const Partitions& partitions, const Block& block) const;

	/**
	 * Join a partition and a block. If the block has no element in
	 * common with any block of the partition, merely insert
	 * it. Otherwise fuse the blocks with common elements into
	 * one. During this fusion new unification problems may arise
	 * (TODO: explain why) thus possibly multiple partitions will be
	 * returned.
	*/
	Partitions join(const Partition& partition, const Block &block) const;

	/**
	 * Join a block to a partition to form a single block. It is
	 * assumed that all blocks have elements in common.
	*/
	Block join(const std::vector<Block>& common_blocks, const Block& block) const;

	/**
	 * Join 2 blocks (supposedly satisfiable).
	 *
	 * That is compute their type intersection and if defined, then build
	 * the block as the union of the 2 blocks, typed with their type
	 * intersection.
	 */
	Block join(const Block& lhs, const Block& rhs) const;

	/**
	 * Unify all terms that are not in the intersection of block and
	 * each block of common_blocks.
	 *
	 * TODO: should probably support quotation.
	 */
	SolutionSet subunify(const std::vector<Block>& common_blocks,
	                     const Block& block) const;

	/**
	 * Unify all terms that are not in the intersection of blocks lhs
	 * and rhs.
	 *
	 * TODO: should probably support quotation.
	 */
	SolutionSet subunify(const Block& lhs, const Block& rhs) const;

	/**
	 * Return true if a unification block is satisfiable. A unification
	 * block is non satisfiable if it's type is undefined (bottom).
	 */
	bool is_satisfiable(const Block& block) const;
};

/**
 * Calculate type intersection. For example: say you have for a block
 * with
 *
 * X
 * ListLink(Y)
 * ListLink(Z)
 *
 * meaning that X is equal to ListLink Y which is equal to ListLink Z,
 * each having the following types at that point (i.e. not having
 * reached the fixed point yet)
 *
 * X:Atom
 * ListLink(Y):ListLink(Atom)
 * ListLink(Z):ListLink(Atom)
 *
 * then their type intersection will be
 *
 * ListLink(Atom)
 *
 * which is supposed to represent the set of all potential groundings
 * that may satisfy that block.
 *
 * TODO: this can be probably by optimized by using VariableListPtr
 *       instead of Handle, so we don't rebuild it every time.
 */
Handle type_intersection(const Handle& lhs, const Handle& rhs,
                         const Handle& lhs_vardecl=Handle::UNDEFINED,
                         const Handle& rhs_vardecl=Handle::UNDEFINED,
                         Quotation lhs_quotation=Quotation(),
                         Quotation rhs_quotation=Quotation());

/**
 * Return a simplification of a type union, by eliminating all types
 * that are redundant. For instance
 *
 * {Node, ConceptNode, ListLink}
 *
 * would return
 *
 * {Node, ListLink}
 *
 * As ConceptNode inherits Node.
 */
std::set<Type> simplify_type_union(std::set<Type>& type);

/**
 * Return the union type of a variable given its variable declaration.
 * If the variable declaration is empty (Handle::UNDEFINED) then the
 * union type is not empty, instead it contains the singleton
 * {ATOM}. An empty union type would instead mean the bottom type
 * (that nothing can inherit).
 */
std::set<Type> get_union_type(const Handle& h, const Handle& vardecl);

/**
 * Return true if lhs inherit rhs. If lhs is not a variable then it
 * relays that to VariableList::is_type, otherwise their type
 * declarations are compared.
 */
bool inherit(const Handle& lhs, const Handle& rhs,
             const Handle& lhs_vardecl, const Handle& rhs_vardecl,
             Quotation lhs_quotation=Quotation(),
             Quotation rhs_quotation=Quotation());

/**
 * Extreme crude version of the above when we have no variable
 * declarations. Basically 2 variables inherits each other and a non
 * variable inherits a variable. Everything else return false.
 */
bool inherit(const Handle& lhs, const Handle& rhs);

/**
 * Return true if lhs inherits rhs.
 */
bool inherit(Type lhs, Type rhs);

/**
 * Return true if a type inherits a type union.
 */
bool inherit(Type lhs, const std::set<Type>& rhs);

/**
 * Return true if lhs inherits rhs. That is if all elements of lhs
 * inherits rhs.
 */
bool inherit(const std::set<Type>& lhs, const std::set<Type>& rhs);

/**
 * Generate a VariableList of the free variables of a given atom h.
 */
VariableListPtr gen_varlist(const Handle& h);
Handle gen_vardecl(const Handle& h);

/**
 * Given an atom h and its variable declaration vardecl, turn the
 * vardecl into a VariableList if not already, and if undefined,
 * generate a VariableList of the free variables of h.
 */
VariableListPtr gen_varlist(const Handle& h, const Handle& vardecl);

/**
 * Merge two vardecls into one. If a variable is present in both
 * vardecls then the more restrictive one replaces the less
 * restrictive one.
 *
 * TODO: give example.
 */
Handle merge_vardecl(const Handle& lhs_vardecl, const Handle& rhs_vardecl);

std::string oc_to_string(const Unify::Partition& hshm);
std::string oc_to_string(const Unify::Block& ub);
std::string oc_to_string(const Unify::Partitions& par);
std::string oc_to_string(const Unify::SolutionSet& sol);
std::string oc_to_string(const Unify::TypedSubstitutions& tss);
std::string oc_to_string(const Unify::TypedSubstitution& ts);
	
} // namespace opencog

#endif // _OPENCOG_UNIFY_UTILS_H
