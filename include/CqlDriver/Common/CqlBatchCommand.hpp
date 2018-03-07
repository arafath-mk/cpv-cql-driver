#pragma once
#include <cstdint>
#include <utility>
#include <vector>
#include <core/sstring.hh>
#include "./Utility/CqlObject.hpp"
#include "CqlCommonDefinitions.hpp"
#include "CqlColumnTrait.hpp"

namespace cql {
	/** Defines members of CqlBatchCommand */
	class CqlBatchCommandData;

	/** Class represents multiple cql commands for execute */
	class CqlBatchCommand {
	public:
		/** Check whether this is a valid command (will be false if moved) */
		bool isValid() const;

		/**
		 * Set the consistency level of this batch
		 * For more information see this page:
		 * https://docs.datastax.com/en/cassandra/2.1/cassandra/dml/dml_config_consistency_c.html
		 */
		CqlBatchCommand&& setConsistencyLevel(CqlConsistencyLevel consistencyLevel) &&;

		/** Add a new query to this batch */
		CqlBatchCommand&& addQuery(seastar::sstring&& query) &&;

		/** Add a new query to this batch */
		CqlBatchCommand&& addQuery(const char* query, std::size_t size) &&;

		/** Add a new query to this batch */
		template <std::size_t Size>
		CqlBatchCommand&& addQuery(const char(&query)[Size]) && {
			static_assert(Size > 0, "check size");
			return std::move(*this).addQuery(static_cast<const char*>(query), Size-1);
		}

		/** Open a new parameter set explicitly of the last query */
		CqlBatchCommand&& openParameterSet() &&;

		/**
		 * Add single query parameter bound by position to the last parameter set.
		 * The position is incremental, when this function is called.
		 */
		template <class T>
		CqlBatchCommand&& addParameter(T&& parameter) && {
			CqlColumnTrait<std::decay_t<T>>::encode(
				std::forward<T>(parameter), getMutableParameters());
			++getMutableParameterCount();
			return std::move(*this);
		}

		/**
		 * Add multiple query parameters bound by position to the last parameter set.
		 * The position is incremental, when this function is called.
		 */
		template <class... Args>
		CqlBatchCommand&& addParameters(Args&&... parameters) && {
			addParametersEncode(getMutableParameters(), std::forward<Args>(parameters)...);
			getMutableParameterCount() += sizeof...(Args);
			return std::move(*this);
		}

		/** Get the consistency level of this batch */
		CqlConsistencyLevel getConsistencyLevel() const;

		/** Get how many queries in this batch */
		std::size_t getQueryCount() const;

		/** Get the query string by index */
		std::pair<const char*, std::size_t> getQuery(std::size_t index) const&;

		/** Get the parameter sets by index */
		using ParameterSetsType = std::vector<std::pair<std::size_t, seastar::sstring>>;
		const ParameterSetsType& getParameterSets(std::size_t index) const&;

		/** Constructor */
		CqlBatchCommand();

	private:
		/** Get the mutable count of parameters of the last parameter set */
		std::size_t& getMutableParameterCount() &;

		/** Get the mutable encoded parameters of the last parameter set */
		seastar::sstring& getMutableParameters() &;

		/** Encode implementation of addParameters */
		static void addParametersEncode(seastar::sstring&) { }
		template <class Head, class... Rest>
		static void addParametersEncode(seastar::sstring& data, Head&& head, Rest&&... rest) {
			CqlColumnTrait<std::decay_t<Head>>::encode(std::forward<Head>(head), data);
			addParametersEncode(data, std::forward<Rest>(rest)...);
		}

	private:
		CqlObject<CqlBatchCommandData> data_;
	};
}
