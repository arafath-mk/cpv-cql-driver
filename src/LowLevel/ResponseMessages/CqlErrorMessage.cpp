#include "CqlErrorMessage.hpp"

namespace cql {
	/** For CqlObject */
	void CqlErrorMessage::reset(CqlMessageHeader&& header) {
		CqlResponseMessageBase::reset(std::move(header));
		errorCode_.reset();
		errorMessage_.reset();
		extraContents_.resize(0);
	}

	/** Decode message body from binary data */
	void CqlErrorMessage::decodeBody(const CqlConnectionInfo&, const char*& ptr, const char* end) {
		// the body of the message will be an [int] error code followed by a [string] error message
		// then, depending on the exception, more content may follow
		errorCode_.decode(ptr, end);
		errorMessage_.decode(ptr, end);
		extraContents_.resize(0);
		extraContents_.append(ptr, end - ptr);
	}

	/** Constructor */
	CqlErrorMessage::CqlErrorMessage() :
		errorCode_(),
		errorMessage_(),
		extraContents_() { }
}

