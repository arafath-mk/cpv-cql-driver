#include <CqlDriver/Common/Exceptions/CqlDecodeException.hpp>
#include <LowLevel/ProtocolTypes/CqlProtocolConsistency.hpp>
#include <TestUtility/GTestUtils.hpp>

TEST(TestCqlProtocolConsistency, all) {
	cql::CqlProtocolConsistency value(cql::CqlConsistencyLevel::One);
	ASSERT_EQ(value.get(), cql::CqlConsistencyLevel::One);
	value.set(cql::CqlConsistencyLevel::Quorum);
	ASSERT_EQ(value.get(), cql::CqlConsistencyLevel::Quorum);

	value = cql::CqlProtocolConsistency();
	ASSERT_EQ(value.get(), cql::CqlConsistencyLevel::Any);
}

TEST(TestCqlProtocolConsistency, encode) {
	cql::CqlProtocolConsistency value(cql::CqlConsistencyLevel::Three);
	seastar::sstring data;
	value.encode(data);
	ASSERT_EQ(data, seastar::sstring("\x00\x03", 2));
}

TEST(TestCqlProtocolConsistency, decode) {
	cql::CqlProtocolConsistency value;
	seastar::sstring data("\x00\x03", 2);
	auto ptr = data.c_str();
	auto end = ptr + data.size();
	value.decode(ptr, end);
	ASSERT_TRUE(ptr == end);
	ASSERT_EQ(value.get(), cql::CqlConsistencyLevel::Three);
}

TEST(TestCqlProtocolConsistency, decodeError) {
	cql::CqlProtocolConsistency value;
	seastar::sstring data("\x00", 1);
	auto ptr = data.c_str();
	auto end = ptr + data.size();
	ASSERT_THROWS(cql::CqlDecodeException, value.decode(ptr, end));
}

