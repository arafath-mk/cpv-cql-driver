#pragma once
#include "CompressorBase.hpp"

namespace cql {
	/**
	 * Compressor uses lz4 algorithm.
	 * The first four bytes of the body will be the uncompressed length (little endian),
	 * followed by the compressed bytes.
	 */
	class LZ4Compressor : public CompressorBase {
	public:
		/** Get the name of the compressor */
		const std::string& name() const& override;

		/** Compress the request frame's body */
		void compress(const std::string& source, std::string& output) override;

		/** Decompress the response frame's body */
		seastar::temporary_buffer<char> decompress(
			const ConnectionInfo& connectionInfo,
			seastar::temporary_buffer<char>&& source) override;
	};
}

