/*
 * Copyright (c) 2013-2014, Roland Bock
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 *   Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SQLPP_CTE_H
#define SQLPP_CTE_H

#include <sqlpp11/union_data.h>
#include <sqlpp11/select_flags.h>
#include <sqlpp11/result_row.h>
#include <sqlpp11/statement_fwd.h>
#include <sqlpp11/type_traits.h>
#include <sqlpp11/parameter_list.h>
#include <sqlpp11/expression.h>
#include <sqlpp11/interpret_tuple.h>
#include <sqlpp11/interpretable_list.h>
#include <sqlpp11/logic.h>

namespace sqlpp
{
	template<typename AliasProvider, typename Statement, typename... FieldSpecs>
		struct cte_t;

	template<typename FieldSpec>
		struct cte_column_spec_t
		{
			using _alias_t = typename FieldSpec::_alias_t;

			using _traits = make_traits<value_type_of<FieldSpec>, 
						tag::must_not_insert, 
						tag::must_not_update,
						tag_if<tag::can_be_null, column_spec_can_be_null_t<FieldSpec>::value>
							>;
		};

	template<typename AliasProvider, typename Statement, typename ResultRow>
		struct make_cte_impl
		{
			using type = void;
		};

	template<typename AliasProvider, typename Statement, typename... FieldSpecs>
		struct make_cte_impl<AliasProvider, Statement, result_row_t<void, FieldSpecs...>>
		{
			using type = cte_t<AliasProvider, Statement, FieldSpecs...>;
		};

	template<typename AliasProvider, typename Statement>
		using make_cte_t = typename make_cte_impl<AliasProvider, Statement, get_result_row_t<Statement>>::type;

	template<typename AliasProvider, typename Statement, typename... FieldSpecs>
		struct cte_t: public member_t<cte_column_spec_t<FieldSpecs>, column_t<AliasProvider, cte_column_spec_t<FieldSpecs>>>...
		{
			using _traits = make_traits<no_value_t, tag::is_cte, tag::is_table>; // FIXME: is table? really?
			struct _recursive_traits
			{
				using _required_ctes = detail::make_joined_set_t<required_ctes_of<Statement>, detail::make_type_set_t<AliasProvider>>;
				using _provided_ctes = detail::type_set<>;
				using _required_tables = detail::type_set<>;
				using _provided_tables = detail::type_set<AliasProvider>;
				using _provided_outer_tables = detail::type_set<>;
				using _extra_tables = detail::type_set<>;
				using _parameters = parameters_of<Statement>;
				using _tags = detail::type_set<>;
			};
			using _alias_t = typename AliasProvider::_alias_t;

			using _column_tuple_t = std::tuple<column_t<AliasProvider, cte_column_spec_t<FieldSpecs>>...>;

			template<typename... T>
				using _check = logic::all_t<is_statement_t<T>::value...>;

			template<typename Rhs>
				auto union_distinct(Rhs rhs) const
				-> typename std::conditional<_check<Rhs>::value, cte_t<AliasProvider, union_data_t<void, distinct_t, Statement, Rhs>, FieldSpecs...>, bad_statement>::type
				{
					static_assert(is_statement_t<Rhs>::value, "argument of union call has to be a statement");
					static_assert(has_policy_t<Rhs, is_select_t>::value, "argument of union call has to be a select");
					static_assert(has_result_row_t<Rhs>::value, "argument of a union has to be a (complete) select statement");

					using _result_row_t = result_row_t<void, FieldSpecs...>;
					static_assert(std::is_same<_result_row_t, get_result_row_t<Rhs>>::value, "both select statements in a union have to have the same result columns (type and name)");

					return _union_impl<void, distinct_t>(_check<Rhs>{}, rhs);
				}

			template<typename Rhs>
				auto union_all(Rhs rhs) const
				-> typename std::conditional<_check<Rhs>::value, cte_t<AliasProvider, union_data_t<void, all_t, Statement, Rhs>, FieldSpecs...>, bad_statement>::type
				{
					static_assert(is_statement_t<Rhs>::value, "argument of union call has to be a statement");
					static_assert(has_policy_t<Rhs, is_select_t>::value, "argument of union call has to be a select");
					static_assert(has_result_row_t<Rhs>::value, "argument of a union has to be a (complete) select statement");

					using _result_row_t = result_row_t<void, FieldSpecs...>;
					static_assert(std::is_same<_result_row_t, get_result_row_t<Rhs>>::value, "both select statements in a union have to have the same result columns (type and name)");

					return _union_impl<void, all_t>(_check<Rhs>{}, rhs);
				}

		private:
			template<typename Database, typename Flag, typename Rhs>
				auto _union_impl(const std::false_type&, Rhs rhs) const
				-> bad_statement;

			template<typename Database, typename Flag, typename Rhs>
				auto _union_impl(const std::true_type&, Rhs rhs) const
				-> cte_t<AliasProvider, union_data_t<void, Flag, Statement, Rhs>, FieldSpecs...>
				{
					return union_data_t<Database, Flag, Statement, Rhs>{_statement, rhs};
				}

		public:

			cte_t(Statement statement): _statement(statement){}
			cte_t(const cte_t&) = default;
			cte_t(cte_t&&) = default;
			cte_t& operator=(const cte_t&) = default;
			cte_t& operator=(cte_t&&) = default;
			~cte_t() = default;

			Statement _statement;
		};

	template<typename Context, typename AliasProvider, typename Statement, typename... ColumnSpecs>
		struct serializer_t<Context, cte_t<AliasProvider, Statement, ColumnSpecs...>>
		{
			using _serialize_check = serialize_check_of<Context, Statement>;
			using T = cte_t<AliasProvider, Statement, ColumnSpecs...>;

			static Context& _(const T& t, Context& context)
			{
				context << name_of<T>::char_ptr() << " AS (";
				serialize(t._statement, context);
				context << ")";
				return context;
			}
		};


// The cte_t is displayed as AliasProviderName except within the with:
//    - the with needs the 
//      AliasProviderName AS (ColumnNames) (select/union)
// The result row of the select should not have dynamic parts
	template<typename AliasProvider>
		struct pre_cte_t
		{
			using _traits = make_traits<no_value_t, tag::is_cte, tag::is_table>; // FIXME: is table? really?
			struct _recursive_traits
			{
				using _required_ctes = detail::make_type_set_t<AliasProvider>;
				using _provided_ctes = detail::type_set<>;
				using _required_tables = detail::type_set<>;
				using _provided_tables = detail::type_set<AliasProvider>;
				using _provided_outer_tables = detail::type_set<>;
				using _extra_tables = detail::type_set<>;
				using _parameters = std::tuple<>;
				using _tags = detail::type_set<>;
			};

			using _alias_t = typename AliasProvider::_alias_t;

			template<typename Statement>
				auto as(Statement statement)
				-> make_cte_t<AliasProvider, Statement>
				{
					static_assert(required_tables_of<Statement>::size::value == 0, "common table expression must not use unknown tables");
					static_assert(not detail::is_element_of<AliasProvider, required_ctes_of<Statement>>::value, "common table expression must not self-reference in the first part, use union_all/union_distinct for recursion");
					static_assert(is_static_result_row_t<get_result_row_t<Statement>>::value, "ctes must not have dynamically added columns");

					return { statement };
				}
		};

	template<typename Context, typename AliasProvider>
		struct serializer_t<Context, pre_cte_t<AliasProvider>>
		{
			using _serialize_check = consistent_t;
			using T = pre_cte_t<AliasProvider>;

			static Context& _(const T& t, Context& context)
			{
				context << name_of<T>::char_ptr();
				return context;
			}
		};

	template<typename AliasProvider>
		auto cte(const AliasProvider&)
		-> pre_cte_t<AliasProvider>
		{
			return {};
		}

}

#endif