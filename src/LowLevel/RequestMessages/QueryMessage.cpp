#include "./QueryMessage.hpp"
#include "../ProtocolTypes/ProtocolInt.hpp"

namespace cql {
	/** For Object */
	void QueryMessage::reset(MessageHeader&& header) {
		RequestMessageBase::reset(std::move(header));
		queryParameters_.reset();
	}

	/** Get description of this message */
	seastar::sstring QueryMessage::toString() const {
		seastar::sstring query;
		auto& command = queryParameters_.getCommand();
		if (command.isValid()) {
			query.append(command.getQuery().first, command.getQuery().second);
		}
		return joinString("", "QueryMessage(query: ", query, ")");
	}

	/** Encode message body to binary data */
	void QueryMessage::encodeBody(const ConnectionInfo&, seastar::sstring& data) const {
		// The body of the message must be: <query><query_parameters>
		// where <query> is a [long string] represeting the query, and <query_parameters> must be:
		// (check comments on ProtocolQueryParameters)
		auto& command = queryParameters_.getCommand();
		if (!command.isValid()) {
			throw LogicException(CQL_CODEINFO, "invalid(moved) command");
		}
		// <query>
		auto query = command.getQuery();
		ProtocolInt querySize(query.second);
		querySize.encode(data);
		data.append(query.first, query.second);
		// <query_parameters>
		queryParameters_.encode(data);
	}

	/** Constructor */
	QueryMessage::QueryMessage() :
		queryParameters_() { }
}
