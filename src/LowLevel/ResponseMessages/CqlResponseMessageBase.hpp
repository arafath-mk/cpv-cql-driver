#pragma once
#include "../CqlMessageBase.hpp"

namespace cql {
	/** The base class of response message */
	class CqlResponseMessageBase : public CqlMessageBase {
	public:
		/** Decode message body from binary data */
		virtual void decodeBody(const CqlConnectionInfo& info, const seastar::temporary_buffer<char>& data) = 0;
	};
}

