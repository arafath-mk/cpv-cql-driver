#pragma once
#include <CqlDriver/Common/CqlCommonDefinitions.hpp>
#include "../CqlLowLevelDefinitions.hpp"
#include "CqlProtocolConsistency.hpp"
#include "CqlProtocolByte.hpp"
#include "CqlProtocolInt.hpp"
#include "CqlProtocolLong.hpp"
#include "CqlProtocolString.hpp"
#include "CqlProtocolValueList.hpp"
#include "CqlProtocolValueMap.hpp"
#include "CqlProtocolBytes.hpp"

namespace cql {
	/**
	 * <consistency><flags>[<n>[name_1]<value_1>...[name_n]<value_n>][<result_page_size>][<paging_state>] ~
	 * ~ [<serial_consistency>][<timestamp>]
	 * [] mean optional, depends on <flags>.
	 * Check native_protocol_v4.spec section 4.1.4.
	 */
	class CqlProtocolQueryParameters {
	public:
		using NameAndValuesType = CqlProtocolValueMap::MapType;

		/** Reset to initial state */
		void reset();

		/** The [consistency] level for the operation */
		CqlConsistencyLevel getConsistency() const { return consistency_.get(); }
		void setConsistency(CqlConsistencyLevel consistency) { return consistency_.set(consistency); }

		/** Call setters below will alter flags to indicate which component is included */
		CqlQueryParametersFlags getFlags() const { return static_cast<CqlQueryParametersFlags>(flags_.get()); }

		/** If set, the result set returned as a response to the query will have the NO_METADATA flag */
		void setSkipMetadata(bool value);

		/** Named query parameters */
		const NameAndValuesType& getNameAndValues() const& { return nameAndValues_.get(); }
		NameAndValuesType& getNameAndValues() & { return nameAndValues_.get(); }
		void setNameAndValues(const NameAndValuesType& nameAndValues);
		void setNameAndValues(NameAndValuesType&& nameAndValues);

		/** Unnamed query parameters */
		const std::vector<CqlProtocolValue>& getValues() const& { return values_.get(); }
		std::vector<CqlProtocolValue>& getValues() & { return values_.get(); }
		void setValues(const std::vector<CqlProtocolValue>& values);
		void setValues(std::vector<CqlProtocolValue>&& values);

		/** An [int] controlling the desired page size of the result, check section 8 for more details */
		std::size_t getPageSize() const { return static_cast<std::size_t>(pageSize_.get()); }
		void setPageSize(std::size_t pageSize);

		/** The query will be executed but starting from a given paging state, check section 8 */
		const seastar::sstring& getPagingState() const& { return pagingState_.get(); }
		void setPagingState(const seastar::sstring& pagingState);
		void setPagingState(seastar::sstring&& pagingState);

		/**
		 * The [consistency] level for the serial phase of conditional updates,
		 * can only be either SERIAL or LOCAL_SERIAL, the default is SERIAL.
		 */
		CqlConsistencyLevel getSerialConsistency() const { return serialConsistency_.get(); }
		void setSerialConsistency(CqlConsistencyLevel serialConsistency);

		/**
		 * This will replace the server side assigned timestamp as default timestamp,
		 * note that a timestamp in the query itself will still override this timestamp.
		 */
		std::uint64_t getDefaultTimestamp() const { return defaultTimestamp_.get(); }
		void setDefaultTimestamp(std::uint64_t timestamp);

		/** Encode and decode functions */
		void encode(seastar::sstring& data) const;
		void decode(const char*& ptr, const char* end);

		/** Constructor */
		CqlProtocolQueryParameters();

	private:
		CqlProtocolConsistency consistency_;
		CqlProtocolByte flags_;
		CqlProtocolValueList values_;
		CqlProtocolValueMap nameAndValues_;
		CqlProtocolInt pageSize_;
		CqlProtocolBytes pagingState_;
		CqlProtocolConsistency serialConsistency_;
		CqlProtocolLong defaultTimestamp_;
	};
}

