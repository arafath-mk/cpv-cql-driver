#include "CqlProtocolInetAddr.hpp"
#include <CqlDriver/Common/Exceptions/CqlDecodeException.hpp>
#include <CqlDriver/Common/Exceptions/CqlEncodeException.hpp>

namespace cql {
	/** Encode to binary data */
	void CqlProtocolInetAddr::encode(seastar::sstring& data) const {
		std::uint8_t size = static_cast<std::uint8_t>(value_.size());
		if (size == 0) {
			throw CqlEncodeException(CQL_CODEINFO, "address is undefined");
		}
		data.append(reinterpret_cast<const char*>(&size), sizeof(size));
		data.append(reinterpret_cast<const char*>(value_.data()), size);
	}

	/** Decode from binary data */
	void CqlProtocolInetAddr::decode(const char*& ptr, const char* end) {
		std::uint8_t size = 0;
		if (ptr + sizeof(size) > end) {
			throw CqlDecodeException(CQL_CODEINFO, "length not enough");
		}
		std::memcpy(&size, ptr, sizeof(size));
		ptr += sizeof(size);
		if (ptr + static_cast<std::size_t>(size) > end) {
			throw CqlDecodeException(CQL_CODEINFO, "length not enough");
		}
		if (size == sizeof(::in_addr)) {
			::in_addr addr_v4;
			std::memcpy(&addr_v4, ptr, size);
			value_ = seastar::net::inet_address(addr_v4);
			ptr += size;
		} else if (size == sizeof(::in6_addr)) {
			::in6_addr addr_v6;
			std::memcpy(&addr_v6, ptr, size);
			value_ = seastar::net::inet_address(addr_v6);
			ptr += size;
		} else {
			throw CqlDecodeException(CQL_CODEINFO, "unsupport address size");
		}
	}
}

