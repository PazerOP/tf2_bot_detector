

#include <catch2/catch.hpp>
#include <mh/text/codecvt.hpp>

#include <string_view>

using namespace std::string_view_literals;

template<typename T>
static void RequireEqual(const std::basic_string_view<T>& a, const std::basic_string_view<T>& b)
{
	std::vector<int64_t> araw(a.begin(), a.end()), braw(b.begin(), b.end());
	CAPTURE(araw, braw);

	REQUIRE(araw == braw);
}

template<typename T1, typename T2>
static void CompareExpected(const std::basic_string_view<T1>& v1, const std::basic_string_view<T2>& v2)
{
	RequireEqual<T2>(mh::change_encoding<T2>(v1), v2);
	RequireEqual<T1>(v1, mh::change_encoding<T1>(v2));
}

static void CompareExpected3(const std::u8string_view& v1, const std::u16string_view& v2, const std::u32string_view& v3)
{
	CompareExpected(v1, v2);

	CompareExpected(v1, v3);

	CompareExpected(v2, v3);
}

TEST_CASE("tf2bd_char_conversion_fundamental")
{
	CompareExpected3(u8"\U00010348", u"\U00010348", U"\U00010348");
	CompareExpected3(u8"\u0024", u"\u0024", U"\u0024");
	CompareExpected3(u8"\u00a2", u"\u00a2", U"\u00a2");
	CompareExpected3(u8"\u0939", u"\u0939", U"\u0939");
	CompareExpected3(u8"\u20ac", u"\u20ac", U"\u20ac");
	CompareExpected3(u8"\ud55c", u"\ud55c", U"\ud55c");
	CompareExpected3(u8"😐", u"😐", U"😐");
}

template<typename TConvertTo, typename TInput>
static void CompareRoundtrip(const std::basic_string_view<TInput>& val)
{
	const auto converted = mh::change_encoding<TConvertTo>(val);
	const auto convertedBack = mh::change_encoding<TInput>(converted);

	REQUIRE(convertedBack.size() == val.size());
	for (size_t i = 0; i < val.size(); i++)
	{
		CAPTURE(i);
		REQUIRE(((int64_t)convertedBack.at(i)) == ((int64_t)val.at(i)));
	}
}

template<typename T>
static void CompareStringsAll(const std::basic_string_view<T>& val)
{
	CompareRoundtrip<char8_t>(val);
	CompareRoundtrip<char16_t>(val);
	CompareRoundtrip<char32_t>(val);
}

TEST_CASE("tf2bd_char_conversion")
{
	constexpr const std::u8string_view value_u8 = u8"😐";
	constexpr const std::u16string_view value_u16 = u"😐";
	constexpr const std::u32string_view value_u32 = U"😐";

	CompareStringsAll(value_u8);
	CompareStringsAll(value_u16);
	CompareStringsAll(value_u32);

	//REQUIRE(mh::change_encoding<char16_t>(value_u8) == value_u16);
	//auto u8Str = mh::change_encoding<char8_t>(value_u8);

	//auto u16Str2 = mh::change_encoding<char16_t>(input);

}
